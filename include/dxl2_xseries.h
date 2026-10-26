/*
 * dxl2_xseries.h — control table addresses shared by DYNAMIXEL X series
 * (e.g. XL330, XC330, XL430, XC430, XM430, XH430, XM540, XH540).
 *
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 aReyRen
 *
 * Always check the e-Manual of your model: a few models have extra or missing items
 * (for example, models without current sensing have no Current Limit / Present Current).
 * Values in comments are byte sizes.
 */

#ifndef DXL2_XSERIES_H
#define DXL2_XSERIES_H

/* EEPROM area: write only while Torque Enable = 0. Kept after power off. */
#define DXL2_X_MODEL_NUMBER          0    /* 2 */
#define DXL2_X_FIRMWARE_VERSION      6    /* 1 */
#define DXL2_X_ID                    7    /* 1 */
#define DXL2_X_BAUD_RATE             8    /* 1: 1 = 57600, 3 = 1 M, 4 = 2 M, 5 = 3 M, 6 = 4 M */
#define DXL2_X_RETURN_DELAY_TIME     9    /* 1: unit 2 µs. 0 is fastest */
#define DXL2_X_DRIVE_MODE            10   /* 1 */
#define DXL2_X_OPERATING_MODE        11   /* 1: 0 current, 1 velocity, 3 position, 4 extended position, 5 current-based position, 16 PWM */
#define DXL2_X_PROTOCOL_TYPE         13   /* 1: 2 = Protocol 2.0 */
#define DXL2_X_HOMING_OFFSET         20   /* 4 */
#define DXL2_X_MAX_VOLTAGE_LIMIT     32   /* 2: unit 0.1 V */
#define DXL2_X_MIN_VOLTAGE_LIMIT     34   /* 2 */
#define DXL2_X_PWM_LIMIT             36   /* 2: 885 = 100 % */
#define DXL2_X_CURRENT_LIMIT         38   /* 2 */
#define DXL2_X_VELOCITY_LIMIT        44   /* 4 */
#define DXL2_X_MAX_POSITION_LIMIT    48   /* 4 */
#define DXL2_X_MIN_POSITION_LIMIT    52   /* 4 */
#define DXL2_X_SHUTDOWN              63   /* 1 */

/* RAM area: reset at power on / reboot. */
#define DXL2_X_TORQUE_ENABLE         64   /* 1 */
#define DXL2_X_LED                   65   /* 1 */
#define DXL2_X_STATUS_RETURN_LEVEL   68   /* 1 */
#define DXL2_X_HARDWARE_ERROR_STATUS 70   /* 1: bit 0 voltage, 2 overheating, 3 encoder, 4 electrical shock, 5 overload */
#define DXL2_X_VELOCITY_I_GAIN       76   /* 2 */
#define DXL2_X_VELOCITY_P_GAIN       78   /* 2 */
#define DXL2_X_POSITION_D_GAIN       80   /* 2 */
#define DXL2_X_POSITION_I_GAIN       82   /* 2 */
#define DXL2_X_POSITION_P_GAIN       84   /* 2 */
#define DXL2_X_GOAL_PWM              100  /* 2 */
#define DXL2_X_GOAL_CURRENT          102  /* 2 */
#define DXL2_X_GOAL_VELOCITY         104  /* 4 */
#define DXL2_X_PROFILE_ACCELERATION  108  /* 4 */
#define DXL2_X_PROFILE_VELOCITY      112  /* 4 */
#define DXL2_X_GOAL_POSITION         116  /* 4: 4096 per turn */
#define DXL2_X_MOVING                122  /* 1 */
#define DXL2_X_PRESENT_PWM           124  /* 2: signed, 885 = 100 % */
#define DXL2_X_PRESENT_CURRENT       126  /* 2: signed */
#define DXL2_X_PRESENT_VELOCITY      128  /* 4: signed, unit 0.229 rpm */
#define DXL2_X_PRESENT_POSITION      132  /* 4: signed */
#define DXL2_X_PRESENT_INPUT_VOLTAGE 144  /* 2: unit 0.1 V */
#define DXL2_X_PRESENT_TEMPERATURE   146  /* 1: unit 1 °C */

#endif /* DXL2_XSERIES_H */
