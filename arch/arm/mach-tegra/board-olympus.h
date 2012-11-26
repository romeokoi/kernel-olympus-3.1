/*
 * arch/arm/mach-tegra/board-stingray.h
 *
 * Copyright (C) 2010 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _MACH_TEGRA_BOARD_OLYMPUS_H
#define _MACH_TEGRA_BOARD_OLYMPUS_H

void stingray_fb_alloc(void);

void __init tegra_common_init(void);
void olympus_pinmux_init(void);
int stingray_panel_init(void);
int stingray_keypad_init(void);
int olympus_wlan_init(void);
int stingray_sensors_init(void);
int olympus_touch_init(void);
int olympus_power_init(void);
unsigned int olympus_revision(void);
unsigned int olympus_powerup_reason (void);
void stingray_gps_init(void);
int olympus_qbp_usb_hw_bypass_enabled(void);
void olympus_init_emc(void);
void change_power_brcm_4329(bool);

/* STI-OLY doesn't need
bool stingray_hw_has_cdma(void);
bool stingray_hw_has_lte(void);
bool stingray_hw_has_wifi(void);
bool stingray_hw_has_umts(void);
*/

#define HWREV(x)    (((x)>>16) & 0xFFFF)
#define INSTANCE(x) ((x) & 0xFFFF)
#define _HWREV(x) ((x)<<16)

#define OLYMPUS_REVISION_UNKNOWN  0x0000

#define HWREV_TYPE_S     0x1000
#define HWREV_TYPE_M     0x2000
#define HWREV_TYPE_P     0x8000

/* portable with debugging enabled */
#define HWREV_TYPE_DEBUG 0x9000

/* Final hardware can't depend on revision bytes */
#define HWREV_TYPE_FINAL 0xA000

#define HWREV_TYPE_IS_BRASSBOARD(x)  (((x) & 0xF000) == HWREV_TYPE_S)
#define HWREV_TYPE_IS_MORTABLE(x)    (((x) & 0xF000) == HWREV_TYPE_M)
#define HWREV_TYPE_IS_PORTABLE(x)   ((((x) & 0xF000) == HWREV_TYPE_P)||(((x) & 0xF000) == HWREV_TYPE_DEBUG))
#define HWREV_TYPE_IS_DEBUG(x)       (((x) & 0xF000) == HWREV_TYPE_DEBUG)
#define HWREV_TYPE_IS_FINAL(x)       (((x) & 0xF000) == HWREV_TYPE_FINAL)


#define OLYMPUS_REVISION_0      _HWREV(0x0000)
#define OLYMPUS_REVISION_1      _HWREV(0x0100)
#define OLYMPUS_REVISION_1B     _HWREV(0x01B0)
#define OLYMPUS_REVISION_1C     _HWREV(0x01C0)
#define OLYMPUS_REVISION_2      _HWREV(0x0200)
#define OLYMPUS_REVISION_2B     _HWREV(0x02B0)
#define OLYMPUS_REVISION_2C     _HWREV(0x02C0)
#define OLYMPUS_REVISION_3      _HWREV(0x0300)
#define OLYMPUS_REVISION_3B     _HWREV(0x03B0)
#define OLYMPUS_REVISION_4      _HWREV(0x0400)
#define OLYMPUS_REVISION_4A     _HWREV(0x04A0)
#define OLYMPUS_REVISION_4B     _HWREV(0x04B0)
#define OLYMPUS_REVISION_4F0    _HWREV(0x04F0)
#define OLYMPUS_REVISION_4FB    _HWREV(0x04FB)

#define PU_REASON_TIME_OF_DAY_ALARM     0x00000008 /* Bit 3  */
#define PU_REASON_USB_CABLE             0x00000010 /* Bit 4  */
#define PU_REASON_FACTORY_CABLE         0x00000020 /* Bit 5  */
#define PU_REASON_AIRPLANE_MODE         0x00000040 /* Bit 6  */
#define PU_REASON_PWR_KEY_PRESS         0x00000080 /* Bit 7  */
#define PU_REASON_CHARGER               0x00000100 /* Bit 8  */
#define PU_REASON_POWER_CUT             0x00000200 /* Bit 9  */
#define PU_REASON_REGRESSION_CABLE      0x00000400 /* Bit 10 */
#define PU_REASON_SYSTEM_RESTART        0x00000800 /* Bit 11 */
#define PU_REASON_MODEL_ASSEMBLY        0x00001000 /* Bit 12 */
#define PU_REASON_MODEL_ASSEMBLY_VOL    0x00002000 /* Bit 13 */
#define PU_REASON_SW_AP_RESET           0x00004000 /* Bit 14 */
#define PU_REASON_WDOG_AP_RESET         0x00008000 /* Bit 15 */
#define PU_REASON_CLKMON_CKIH_RESET     0x00010000 /* Bit 16 */
#define PU_REASON_AP_KERNEL_PANIC       0x00020000 /* Bit 17 */
#define PU_REASON_CPCAP_WDOG            0x00040000 /* Bit 18 */
#define PU_REASON_CIDTCMD               0x00080000 /* Bit 19 */
#define PU_REASON_BAREBOARD             0x00100000 /* Bit 20 */
#define PU_REASON_INVALID               0xFFFFFFFF

#endif
