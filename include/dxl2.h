/*
 * dxl2.h — DYNAMIXEL Protocol 2.0 driver for STM32 (HAL, half-duplex UART)
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 aReyRen
 *
 * This is an unofficial library. It is not affiliated with or endorsed by ROBOTIS.
 * DYNAMIXEL is a trademark of ROBOTIS Co., Ltd.
 *
 * Packet format (Protocol 2.0):
 *   FF FF FD 00 | ID | LEN_L LEN_H | INST | PARAM... | CRC_L CRC_H
 *   LEN = number of bytes after LEN (INST + PARAM + CRC). All 2-byte values are little endian.
 */

#ifndef DXL2_H
#define DXL2_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/* The HAL header. CubeMX projects include the HAL through main.h.
 * Define DXL2_HAL_HEADER before including this file to use another header (e.g. "stm32g4xx_hal.h"). */
#ifndef DXL2_HAL_HEADER
#define DXL2_HAL_HEADER "main.h"
#endif
#include DXL2_HAL_HEADER

/* Size of the transmit / receive buffers [bytes]. Increase it for long Sync Write / Sync Read packets. */
#ifndef DXL2_BUF_SIZE
#define DXL2_BUF_SIZE 256
#endif

#define DXL2_BROADCAST_ID     0xFE

#define DXL2_INST_PING        0x01
#define DXL2_INST_READ        0x02
#define DXL2_INST_WRITE       0x03
#define DXL2_INST_REBOOT      0x08
#define DXL2_INST_SYNC_READ   0x82
#define DXL2_INST_SYNC_WRITE  0x83
#define DXL2_INST_STATUS      0x55

typedef enum {
	DXL2_OK = 0,
	DXL2_ERR_TIMEOUT,   /* no (complete) status packet: wiring, baud rate, ID, power, or the bus turned around too late */
	DXL2_ERR_HEADER,    /* status packet does not start with FF FF FD 00 55 */
	DXL2_ERR_ID,        /* status packet came from another ID */
	DXL2_ERR_CRC,       /* status packet is corrupted (noise, signal level) */
	DXL2_ERR_STATUS,    /* the servo returned an error (see dxl2_last_error()) */
	DXL2_ERR_PARAM,     /* invalid argument (too long for DXL2_BUF_SIZE, etc.) */
	DXL2_ERR_LENGTH,    /* status packet has an unexpected number of parameters */
} dxl2_result_t;

/* Call once after the UART is initialised with HAL_HalfDuplex_Init().
 * timeout_ms: how long to wait for a status packet. HAL_GetTick() has 1 ms resolution, so use 2 or more. */
void dxl2_init(UART_HandleTypeDef *huart, uint32_t timeout_ms);

/* CRC-16 used by Protocol 2.0 (polynomial 0x8005, initial value 0). */
uint16_t dxl2_crc(const uint8_t *data, uint16_t len);

/* Single servo (waits for the status packet). */
dxl2_result_t dxl2_ping(uint8_t id, uint16_t *model_number, uint8_t *firmware_version);
dxl2_result_t dxl2_read(uint8_t id, uint16_t addr, uint8_t *out, uint16_t len);
dxl2_result_t dxl2_write(uint8_t id, uint16_t addr, const uint8_t *data, uint16_t len);
dxl2_result_t dxl2_write1(uint8_t id, uint16_t addr, uint8_t value);
dxl2_result_t dxl2_write2(uint8_t id, uint16_t addr, uint16_t value);
dxl2_result_t dxl2_write4(uint8_t id, uint16_t addr, uint32_t value);
dxl2_result_t dxl2_reboot(uint8_t id);   /* the servo restarts after the status packet; wait a few hundred ms */

/* Many servos, same address.
 * dxl2_sync_write: data holds n blocks of len bytes, in the order of ids. No status packet is returned.
 * dxl2_sync_read : out receives n blocks of len bytes, in the order of ids.
 *                  results (may be NULL) receives one dxl2_result_t per servo.
 *                  Returns DXL2_OK only if every servo answered. */
dxl2_result_t dxl2_sync_write(uint16_t addr, uint16_t len, const uint8_t *ids, const uint8_t *data, uint8_t n);
dxl2_result_t dxl2_sync_read(uint16_t addr, uint16_t len, const uint8_t *ids, uint8_t *out, uint8_t n,
                             dxl2_result_t *results);

/* Error byte of the last status packet.
 * bits 0–6: error number (1 result fail, 2 instruction, 3 CRC, 4 data range, 5 data length, 6 data limit, 7 access)
 * bit 7   : alert — the servo has a hardware error; read its Hardware Error Status register. */
uint8_t dxl2_last_error(void);

/* Little-endian helpers. */
static inline uint16_t dxl2_get_u16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static inline int16_t  dxl2_get_i16(const uint8_t *p) { return (int16_t)dxl2_get_u16(p); }
static inline uint32_t dxl2_get_u32(const uint8_t *p) {
	return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
static inline int32_t  dxl2_get_i32(const uint8_t *p) { return (int32_t)dxl2_get_u32(p); }
static inline void dxl2_put_u16(uint8_t *p, uint16_t v) { p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)(v >> 8); }
static inline void dxl2_put_u32(uint8_t *p, uint32_t v) {
	p[0] = (uint8_t)(v & 0xFF); p[1] = (uint8_t)((v >> 8) & 0xFF); p[2] = (uint8_t)((v >> 16) & 0xFF); p[3] = (uint8_t)(v >> 24);
}

#ifdef __cplusplus
}
#endif

#endif /* DXL2_H */
