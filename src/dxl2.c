/*
 * dxl2.c — DYNAMIXEL Protocol 2.0 driver for STM32 (HAL, half-duplex UART)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 aReyRen
 */

#include "dxl2.h"
#include <string.h>

static UART_HandleTypeDef *dxl2_uart;
static uint32_t dxl2_timeout_ms = 2;
static uint8_t tx_buf[DXL2_BUF_SIZE];
static uint8_t rx_buf[DXL2_BUF_SIZE];
static uint8_t last_error;

void dxl2_init(UART_HandleTypeDef *huart, uint32_t timeout_ms) {
	dxl2_uart = huart;
	dxl2_timeout_ms = timeout_ms < 2 ? 2 : timeout_ms;
}

uint8_t dxl2_last_error(void) {
	return last_error;
}

uint16_t dxl2_crc(const uint8_t *data, uint16_t len) {
	uint16_t crc = 0;
	for (uint16_t i = 0; i < len; i++) {
		crc ^= (uint16_t)data[i] << 8;
		for (int b = 0; b < 8; b++)
			crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x8005) : (uint16_t)(crc << 1);
	}
	return crc;
}

/* ---------- UART ---------- */

/* Throw away old bytes and error flags before receiving.
 * Newer USARTs (F0/F3/F7/G0/G4/H7/L0/L4/...) have a flush request; older ones (F1/F2/F4/L1) read DR. */
static void rx_prepare(void) {
	__HAL_UART_CLEAR_OREFLAG(dxl2_uart);
	__HAL_UART_CLEAR_FEFLAG(dxl2_uart);
	__HAL_UART_CLEAR_NEFLAG(dxl2_uart);
#if defined(UART_RXDATA_FLUSH_REQUEST)
	__HAL_UART_SEND_REQ(dxl2_uart, UART_RXDATA_FLUSH_REQUEST);
#else
	if (__HAL_UART_GET_FLAG(dxl2_uart, UART_FLAG_RXNE))
		(void)dxl2_uart->Instance->DR;
#endif
}

/* Send a packet. HAL_UART_Transmit returns after the last stop bit (TC), then the bus is turned around.
 * Keep the time between the end of transmission and HAL_HalfDuplex_EnableReceiver short:
 * with Return Delay Time = 0 the servo answers almost immediately. */
static dxl2_result_t send(uint16_t size, int expect_reply) {
	HAL_HalfDuplex_EnableTransmitter(dxl2_uart);
	if (HAL_UART_Transmit(dxl2_uart, tx_buf, size, dxl2_timeout_ms) != HAL_OK)
		return DXL2_ERR_TIMEOUT;
	if (expect_reply) {
		HAL_HalfDuplex_EnableReceiver(dxl2_uart);
		rx_prepare();
	}
	return DXL2_OK;
}

/* ---------- packets ---------- */

/* Build an instruction packet in tx_buf. Returns its size, or 0 if it does not fit.
 * Byte stuffing: inside INST and PARAM, FF FF FD is sent as FF FF FD FD. */
static uint16_t build(uint8_t id, uint8_t inst, const uint8_t *params, uint16_t n) {
	uint16_t i = 0;
	tx_buf[i++] = 0xFF;
	tx_buf[i++] = 0xFF;
	tx_buf[i++] = 0xFD;
	tx_buf[i++] = 0x00;
	tx_buf[i++] = id;
	i += 2;                                        /* LEN, filled after stuffing */
	uint16_t body = i;
	tx_buf[i++] = inst;
	for (uint16_t k = 0; k < n; k++) {
		if (i + 4 > DXL2_BUF_SIZE)                 /* this byte + a stuffing byte + CRC */
			return 0;
		tx_buf[i++] = params[k];
		if (i - body >= 3 && tx_buf[i - 3] == 0xFF && tx_buf[i - 2] == 0xFF && tx_buf[i - 1] == 0xFD)
			tx_buf[i++] = 0xFD;
	}
	uint16_t length = (uint16_t)(i - body + 2);
	dxl2_put_u16(&tx_buf[5], length);
	uint16_t crc = dxl2_crc(tx_buf, i);
	dxl2_put_u16(&tx_buf[i], crc);
	i = (uint16_t)(i + 2);
	return i;
}

/* Check one status packet that starts at pkt (its full size is 7 + LEN), remove byte stuffing from
 * its parameters in place, and return the parameters through *params / *n_params. */
static dxl2_result_t parse(uint8_t *pkt, uint16_t size, uint8_t id, uint8_t **params, uint16_t *n_params) {
	if (size < 11 || pkt[0] != 0xFF || pkt[1] != 0xFF || pkt[2] != 0xFD || pkt[3] != 0x00 || pkt[7] != DXL2_INST_STATUS)
		return DXL2_ERR_HEADER;
	if (pkt[4] != id)
		return DXL2_ERR_ID;
	if (dxl2_get_u16(&pkt[size - 2]) != dxl2_crc(pkt, (uint16_t)(size - 2)))
		return DXL2_ERR_CRC;
	/* remove stuffing: FF FF FD FD -> FF FF FD (parameters are between the error byte and the CRC) */
	uint16_t r = 9, w = 9, end = (uint16_t)(size - 2);
	while (r < end) {
		pkt[w++] = pkt[r++];
		if (w - 9 >= 3 && pkt[w - 3] == 0xFF && pkt[w - 2] == 0xFF && pkt[w - 1] == 0xFD && r < end && pkt[r] == 0xFD)
			r++;
	}
	last_error = pkt[8];
	*params = &pkt[9];
	*n_params = (uint16_t)(w - 9);
	return (last_error & 0x7F) ? DXL2_ERR_STATUS : DXL2_OK;
}

/* Receive one status packet into rx_buf: the first 7 bytes give LEN, then LEN more bytes. */
static dxl2_result_t receive_one(uint8_t id, uint8_t **params, uint16_t *n_params) {
	if (HAL_UART_Receive(dxl2_uart, rx_buf, 7, dxl2_timeout_ms) != HAL_OK)
		return DXL2_ERR_TIMEOUT;
	uint16_t len = dxl2_get_u16(&rx_buf[5]);
	if (len < 4 || 7u + len > DXL2_BUF_SIZE)
		return DXL2_ERR_HEADER;
	if (HAL_UART_Receive(dxl2_uart, &rx_buf[7], len, dxl2_timeout_ms) != HAL_OK)
		return DXL2_ERR_TIMEOUT;
	return parse(rx_buf, (uint16_t)(7 + len), id, params, n_params);
}

/* Send an instruction to one servo and receive its status packet. */
static dxl2_result_t transfer(uint8_t id, uint8_t inst, const uint8_t *tx_params, uint16_t n_tx,
                              uint8_t **rx_params, uint16_t *n_rx) {
	uint16_t size = build(id, inst, tx_params, n_tx);
	if (size == 0)
		return DXL2_ERR_PARAM;
	dxl2_result_t r = send(size, 1);
	if (r != DXL2_OK)
		return r;
	return receive_one(id, rx_params, n_rx);
}

/* ---------- single servo ---------- */

dxl2_result_t dxl2_ping(uint8_t id, uint16_t *model_number, uint8_t *firmware_version) {
	uint8_t *p;
	uint16_t n;
	dxl2_result_t r = transfer(id, DXL2_INST_PING, NULL, 0, &p, &n);
	if (r != DXL2_OK && r != DXL2_ERR_STATUS)
		return r;
	if (n != 3)
		return DXL2_ERR_LENGTH;
	if (model_number) *model_number = dxl2_get_u16(p);
	if (firmware_version) *firmware_version = p[2];
	return r;
}

dxl2_result_t dxl2_read(uint8_t id, uint16_t addr, uint8_t *out, uint16_t len) {
	uint8_t q[4];
	uint8_t *p;
	uint16_t n;
	dxl2_put_u16(&q[0], addr);
	dxl2_put_u16(&q[2], len);
	dxl2_result_t r = transfer(id, DXL2_INST_READ, q, sizeof(q), &p, &n);
	if (r != DXL2_OK && r != DXL2_ERR_STATUS)
		return r;
	if (n != len)
		return DXL2_ERR_LENGTH;
	memcpy(out, p, len);                           /* also copied when the servo reports an error */
	return r;
}

dxl2_result_t dxl2_write(uint8_t id, uint16_t addr, const uint8_t *data, uint16_t len) {
	uint8_t q[DXL2_BUF_SIZE / 2];
	uint8_t *p;
	uint16_t n;
	if (2u + len > sizeof(q))
		return DXL2_ERR_PARAM;
	dxl2_put_u16(&q[0], addr);
	memcpy(&q[2], data, len);
	if (id == DXL2_BROADCAST_ID) {                 /* no status packet for broadcast */
		uint16_t size = build(id, DXL2_INST_WRITE, q, (uint16_t)(2 + len));
		return size ? send(size, 0) : DXL2_ERR_PARAM;
	}
	return transfer(id, DXL2_INST_WRITE, q, (uint16_t)(2 + len), &p, &n);
}

dxl2_result_t dxl2_write1(uint8_t id, uint16_t addr, uint8_t value) {
	return dxl2_write(id, addr, &value, 1);
}

dxl2_result_t dxl2_write2(uint8_t id, uint16_t addr, uint16_t value) {
	uint8_t d[2];
	dxl2_put_u16(d, value);
	return dxl2_write(id, addr, d, 2);
}

dxl2_result_t dxl2_write4(uint8_t id, uint16_t addr, uint32_t value) {
	uint8_t d[4];
	dxl2_put_u32(d, value);
	return dxl2_write(id, addr, d, 4);
}

dxl2_result_t dxl2_reboot(uint8_t id) {
	uint8_t *p;
	uint16_t n;
	return transfer(id, DXL2_INST_REBOOT, NULL, 0, &p, &n);
}

/* ---------- many servos ---------- */

dxl2_result_t dxl2_sync_write(uint16_t addr, uint16_t len, const uint8_t *ids, const uint8_t *data, uint8_t n) {
	uint8_t q[DXL2_BUF_SIZE];
	uint16_t k = 0;
	if (4u + (uint32_t)n * (1u + len) > sizeof(q))
		return DXL2_ERR_PARAM;
	dxl2_put_u16(&q[k], addr); k += 2;
	dxl2_put_u16(&q[k], len);  k += 2;
	for (uint8_t m = 0; m < n; m++) {
		q[k++] = ids[m];
		memcpy(&q[k], &data[(uint32_t)m * len], len);
		k += len;
	}
	uint16_t size = build(DXL2_BROADCAST_ID, DXL2_INST_SYNC_WRITE, q, k);
	return size ? send(size, 0) : DXL2_ERR_PARAM;
}

/* The servos answer one after another without gaps, so all status packets are received with a single
 * HAL_UART_Receive call (n packets of 11 + len bytes) and parsed afterwards.
 * If a packet contains byte stuffing it is longer than expected, and the last servo(s) time out. */
dxl2_result_t dxl2_sync_read(uint16_t addr, uint16_t len, const uint8_t *ids, uint8_t *out, uint8_t n,
                             dxl2_result_t *results) {
	uint8_t q[4 + 253];
	uint32_t total = (uint32_t)n * (11u + len);
	if (n == 0 || n > 253 || total > DXL2_BUF_SIZE)
		return DXL2_ERR_PARAM;
	dxl2_put_u16(&q[0], addr);
	dxl2_put_u16(&q[2], len);
	memcpy(&q[4], ids, n);
	uint16_t size = build(DXL2_BROADCAST_ID, DXL2_INST_SYNC_READ, q, (uint16_t)(4 + n));
	if (size == 0)
		return DXL2_ERR_PARAM;
	dxl2_result_t r = send(size, 1);
	if (r != DXL2_OK)
		return r;

	/* receive as much as arrives; on timeout, RxXferCount tells how many bytes are missing */
	HAL_StatusTypeDef hs = HAL_UART_Receive(dxl2_uart, rx_buf, (uint16_t)total, dxl2_timeout_ms * n);
	uint16_t got = (hs == HAL_OK) ? (uint16_t)total : (uint16_t)(total - dxl2_uart->RxXferCount);

	dxl2_result_t all = DXL2_OK;
	uint16_t pos = 0;
	for (uint8_t m = 0; m < n; m++) {
		dxl2_result_t rm = DXL2_ERR_TIMEOUT;
		if (pos + 7u <= got) {
			uint16_t plen = (uint16_t)(7 + dxl2_get_u16(&rx_buf[pos + 5]));
			if (pos + plen <= got) {
				uint8_t *p;
				uint16_t np;
				rm = parse(&rx_buf[pos], plen, ids[m], &p, &np);
				if ((rm == DXL2_OK || rm == DXL2_ERR_STATUS) && np != len)
					rm = DXL2_ERR_LENGTH;
				if (rm == DXL2_OK || rm == DXL2_ERR_STATUS)
					memcpy(&out[(uint32_t)m * len], p, len);
				pos = (uint16_t)(pos + plen);
			}
		}
		if (results) results[m] = rm;
		if (rm != DXL2_OK && all == DXL2_OK) all = rm;
	}
	return all;
}
