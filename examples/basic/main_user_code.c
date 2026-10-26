/*
 * Example: pieces to put into the USER CODE sections of a CubeMX-generated main.c
 *
 * CubeMX settings
 *   USART1 : Mode = Single Wire (Half-Duplex), 1 000 000 bit/s, 8N1   (TX pin only)
 *   USART2 : Asynchronous, 115200 bit/s (printf to the ST-LINK virtual COM port)
 *
 * What it does
 *   1. scans ID 1-20 and prints the servos it finds
 *   2. enables torque on ID 1 and ID 2
 *   3. moves both with Sync Write (sine wave around the centre) and reads them back with Sync Read
 *
 * SPDX-License-Identifier: MIT
 */

/* USER CODE BEGIN Includes */
#include <math.h>
#include <stdio.h>
#include "dxl2.h"
#include "dxl2_xseries.h"
/* USER CODE END Includes */

/* USER CODE BEGIN 0 */
int __io_putchar(int ch) {                       /* printf -> USART2 */
	HAL_UART_Transmit(&huart2, (uint8_t *)&ch, 1, 10);
	return ch;
}
/* USER CODE END 0 */

  /* USER CODE BEGIN 2 */
  dxl2_init(&huart1, 2);                         /* 2 ms timeout */

  for (uint8_t id = 1; id <= 20; id++) {          /* 1. scan */
    uint16_t model;
    uint8_t fw;
    if (dxl2_ping(id, &model, &fw) == DXL2_OK)
      printf("found ID %u: model %u, firmware %u\r\n", id, model, fw);
  }

  const uint8_t ids[2] = {1, 2};
  for (int i = 0; i < 2; i++) {                   /* 2. torque on (goal = present first, so it does not jump) */
    uint8_t b[4];
    if (dxl2_read(ids[i], DXL2_X_PRESENT_POSITION, b, 4) == DXL2_OK)
      dxl2_write(ids[i], DXL2_X_GOAL_POSITION, b, 4);
    dxl2_write1(ids[i], DXL2_X_TORQUE_ENABLE, 1);
  }
  uint32_t next = HAL_GetTick();
  /* USER CODE END 2 */

    /* USER CODE BEGIN 3 */
    float t = HAL_GetTick() / 1000.0f;            /* 3. 100 Hz loop */
    uint8_t goal[2 * 4];
    for (int i = 0; i < 2; i++)
      dxl2_put_u32(&goal[4 * i], (uint32_t)(2048 + (int32_t)(300.0f * sinf(2.0f * 3.14159265f * 0.5f * t))));
    dxl2_sync_write(DXL2_X_GOAL_POSITION, 4, ids, goal, 2);

    uint8_t now[2 * 4];
    dxl2_result_t res[2];
    if (dxl2_sync_read(DXL2_X_PRESENT_POSITION, 4, ids, now, 2, res) == DXL2_OK)
      printf("%ld %ld\r\n", (long)dxl2_get_i32(&now[0]), (long)dxl2_get_i32(&now[4]));

    next += 10;
    while ((int32_t)(HAL_GetTick() - next) < 0) {}
  }
  /* USER CODE END 3 */
