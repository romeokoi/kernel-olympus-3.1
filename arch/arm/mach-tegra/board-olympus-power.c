/*
 * Copyright (C) 2010 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/leds-ld-cpcap.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/radio_ctrl/mdm6600_ctrl.h>
#include <linux/reboot.h>
#include <linux/regulator/consumer.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/fixed.h>
#include <linux/regulator/machine.h>
/* STI-OLY we don't need it 
#include <linux/radio_ctrl/wrigley_ctrl.h>

#include <linux/regulator/max8649.h>

#include <linux/l3g4200d.h>
*/
#include <linux/spi/cpcap.h>
#include <linux/spi/cpcap-regbits.h>
#include <linux/spi/spi.h>


#include <mach/gpio.h>
#include <mach/iomap.h>
#include <mach/irqs.h>

#include "board-olympus.h"
#include "gpio-names.h"

/* For the PWR + VOL UP reset, CPCAP can perform a hard or a soft reset. A hard
 * reset will reset the entire system, where a soft reset will reset only the
 * T20. Uncomment this line to use soft resets (should not be enabled on
 * production builds). */
/* #define ENABLE_SOFT_RESET_DEBUGGING */

static struct cpcap_device *cpcap_di;

static int cpcap_validity_reboot(struct notifier_block *this,
				 unsigned long code, void *cmd)
{
	int ret = -1;
	int result = NOTIFY_DONE;
	char *mode = cmd;

	dev_info(&(cpcap_di->spi->dev), "Saving power down reason.\n");

	if (code == SYS_RESTART) {
		if (mode != NULL && !strncmp("outofcharge", mode, 12)) {
			/* Set the outofcharge bit in the cpcap */
			ret = cpcap_regacc_write(cpcap_di, CPCAP_REG_VAL1,
						 CPCAP_BIT_OUT_CHARGE_ONLY,
						 CPCAP_BIT_OUT_CHARGE_ONLY);
			if (ret) {
				dev_err(&(cpcap_di->spi->dev),
					"outofcharge cpcap set failure.\n");
				result = NOTIFY_BAD;
			}
			/* Set the soft reset bit in the cpcap */
			cpcap_regacc_write(cpcap_di, CPCAP_REG_VAL1,
					   CPCAP_BIT_SOFT_RESET,
					   CPCAP_BIT_SOFT_RESET);
			if (ret) {
				dev_err(&(cpcap_di->spi->dev),
					"reset cpcap set failure.\n");
				result = NOTIFY_BAD;
			}
		}

		/* Check if we are starting recovery mode */
		if (mode != NULL && !strncmp("recovery", mode, 9)) {
			/* Set the fota (recovery mode) bit in the cpcap */
			ret = cpcap_regacc_write(cpcap_di, CPCAP_REG_VAL1,
				CPCAP_BIT_FOTA_MODE, CPCAP_BIT_FOTA_MODE);
			if (ret) {
				dev_err(&(cpcap_di->spi->dev),
					"Recovery cpcap set failure.\n");
				result = NOTIFY_BAD;
			}
		} else {
			/* Set the fota (recovery mode) bit in the cpcap */
			ret = cpcap_regacc_write(cpcap_di, CPCAP_REG_VAL1, 0,
						 CPCAP_BIT_FOTA_MODE);
			if (ret) {
				dev_err(&(cpcap_di->spi->dev),
					"Recovery cpcap clear failure.\n");
				result = NOTIFY_BAD;
			}
		}
		/* Check if we are going into fast boot mode */
		if (mode != NULL && !strncmp("bootloader", mode, 11)) {
			/* Set the bootmode bit in the cpcap */
			ret = cpcap_regacc_write(cpcap_di, CPCAP_REG_VAL1,
				CPCAP_BIT_BOOT_MODE, CPCAP_BIT_BOOT_MODE);
			if (ret) {
				dev_err(&(cpcap_di->spi->dev),
					"Boot mode cpcap set failure.\n");
				result = NOTIFY_BAD;
			}
		}
	} else {
		ret = cpcap_regacc_write(cpcap_di, CPCAP_REG_VAL1,
					 0,
					 CPCAP_BIT_OUT_CHARGE_ONLY);
		if (ret) {
			dev_err(&(cpcap_di->spi->dev),
				"outofcharge cpcap set failure.\n");
			result = NOTIFY_BAD;
		}

		/* Clear the soft reset bit in the cpcap */
		ret = cpcap_regacc_write(cpcap_di, CPCAP_REG_VAL1, 0,
					 CPCAP_BIT_SOFT_RESET);
		if (ret) {
			dev_err(&(cpcap_di->spi->dev),
				"SW Reset cpcap set failure.\n");
			result = NOTIFY_BAD;
		}
		/* Clear the fota (recovery mode) bit in the cpcap */
		ret = cpcap_regacc_write(cpcap_di, CPCAP_REG_VAL1, 0,
					 CPCAP_BIT_FOTA_MODE);
		if (ret) {
			dev_err(&(cpcap_di->spi->dev),
				"Recovery cpcap clear failure.\n");
			result = NOTIFY_BAD;
		}
	}

	/* Always clear the kpanic bit */
	ret = cpcap_regacc_write(cpcap_di, CPCAP_REG_VAL1,
				 0, CPCAP_BIT_AP_KERNEL_PANIC);
	if (ret) {
		dev_err(&(cpcap_di->spi->dev),
			"Clear kernel panic bit failure.\n");
		result = NOTIFY_BAD;
	}

	return result;
}

static int is_olympus_ge_p0(struct cpcap_device *cpcap)
{
		return 1;
}

static int is_olympus_ge_p3(struct cpcap_device *cpcap)
{
	if (HWREV_TYPE_IS_FINAL(system_rev) ||
			(HWREV_TYPE_IS_PORTABLE(system_rev) &&
			 (HWREV(system_rev) >= OLYMPUS_REVISION_3))) {
			return 1;
	}
	return 0;
}

enum cpcap_revision cpcap_get_revision(struct cpcap_device *cpcap)
{
	unsigned short value;

	/* Code taken from drivers/mfd/cpcap_core.c, since the revision value
	   is not initialized until after the registers are initialized, which
	   will happen after the trgra_cpcap_spi_init table is used. */
	(void)cpcap_regacc_read(cpcap, CPCAP_REG_VERSC1, &value);
	return (enum cpcap_revision)(((value >> 3) & 0x0007) |
						((value << 3) & 0x0038));
}

int is_cpcap_eq_3_1(struct cpcap_device *cpcap)
{
	return cpcap_get_revision(cpcap) == CPCAP_REVISION_3_1;
}

static struct notifier_block validity_reboot_notifier = {
	.notifier_call = cpcap_validity_reboot,
};

static int cpcap_validity_probe(struct platform_device *pdev)
{
	int err;

	if (pdev->dev.platform_data == NULL) {
		dev_err(&pdev->dev, "no platform_data\n");
		return -EINVAL;
	}

	cpcap_di = pdev->dev.platform_data;

	cpcap_regacc_write(cpcap_di, CPCAP_REG_VAL1,
			   (CPCAP_BIT_AP_KERNEL_PANIC | CPCAP_BIT_SOFT_RESET),
			   (CPCAP_BIT_AP_KERNEL_PANIC | CPCAP_BIT_SOFT_RESET));

	register_reboot_notifier(&validity_reboot_notifier);

	/* CORE_PWR_REQ is only properly connected on P1 hardware and later */
	if (olympus_revision() >= OLYMPUS_REVISION_1) {
		err = cpcap_uc_start(cpcap_di, CPCAP_MACRO_14);
		dev_info(&pdev->dev, "Started macro 14: %d\n", err);
	} else
		dev_info(&pdev->dev, "Not starting macro 14 (no hw support)\n");

	/* Enable workaround to allow soft resets to work */
	cpcap_regacc_write(cpcap_di, CPCAP_REG_PGC,
			   CPCAP_BIT_SYS_RST_MODE, CPCAP_BIT_SYS_RST_MODE);
	err = cpcap_uc_start(cpcap_di, CPCAP_MACRO_15);
	dev_info(&pdev->dev, "Started macro 15: %d\n", err);

	return 0;
}

static int cpcap_validity_remove(struct platform_device *pdev)
{
	unregister_reboot_notifier(&validity_reboot_notifier);
	cpcap_di = NULL;

	return 0;
}

static struct platform_driver cpcap_validity_driver = {
	.probe = cpcap_validity_probe,
	.remove = cpcap_validity_remove,
	.driver = {
		.name = "cpcap_validity",
		.owner  = THIS_MODULE,
	},
};

static struct platform_device cpcap_validity_device = {
	.name   = "cpcap_validity",
	.id     = -1,
	.dev    = {
		.platform_data  = NULL,
	},
};

static struct platform_device cpcap_3mm5_device = {
	.name   = "cpcap_3mm5",
	.id     = -1,
	.dev    = {
		.platform_data  = NULL,
	},
};

static struct cpcap_whisper_pdata whisper_pdata = {
	.data_gpio = TEGRA_GPIO_PV4,
	.pwr_gpio  = TEGRA_GPIO_PT2,
	.uartmux   = 1,
};

static struct platform_device cpcap_whisper_device = {
	.name   = "cpcap_whisper",
	.id     = -1,
	.dev    = {
		.platform_data  = &whisper_pdata,
	},
};

struct cpcap_leds olympus_cpcap_leds = {
	.button_led = {
		.button_reg = CPCAP_REG_KLC,
		.button_mask = 0x03FF,
		.button_on = 0xFFFF,
		.button_off = 0x0000,
		.regulator = "sw5",  /* set to NULL below for products with button LED on B+ */
	},
	.rgb_led = {
		.rgb_on = 0x0053,
		.regulator = "sw5",  /* set to NULL below for products with RGB LED on B+ */
		.regulator_macro_controlled = false, /* Oly, sunfire and older etna set this true below */
	},
};

extern struct platform_device cpcap_disp_button_led;
extern struct platform_device cpcap_rgb_led;

static struct platform_device *cpcap_devices[] = {
	&cpcap_validity_device,
	/* STI-OLY
	&cpcap_notification_led,
	&cpcap_privacy_led,*/
	&cpcap_3mm5_device,
};

struct cpcap_spi_init_data olympus_cpcap_spi_init[] = {
	/* STI-OLY replaced with atrix ones 	
	{CPCAP_REG_S1C1,      0x0000},
	{CPCAP_REG_S1C2,      0x0000},
	{CPCAP_REG_S2C1,      0x4830},
	{CPCAP_REG_S2C2,      0x3030},
	{CPCAP_REG_S3C,       0x0439},
	{CPCAP_REG_S4C1,      0x4930},
	{CPCAP_REG_S4C2,      0x301C},
	{CPCAP_REG_S5C,       0x0000},
	{CPCAP_REG_S6C,       0x0000},
	{CPCAP_REG_VRF1C,     0x0000},
	{CPCAP_REG_VRF2C,     0x0000},
	{CPCAP_REG_VRFREFC,   0x0000},
	{CPCAP_REG_VAUDIOC,   0x0065},
	{CPCAP_REG_ADCC1,     0x9000},
	{CPCAP_REG_ADCC2,     0x4136},
	{CPCAP_REG_USBC1,     0x1201},
	{CPCAP_REG_USBC3,     0x7DFB},
	{CPCAP_REG_OWDC,      0x0003},
	{CPCAP_REG_ADLC,      0x0000},
	*/
	/* Set SW1 to AMS/AMS 1.025v. */
	{CPCAP_REG_S1C1,      0x4822, NULL             },
	/* Set SW2 to AMS/AMS 1.2v. */
	{CPCAP_REG_S2C1,      0x4830, NULL             },
	/* Set SW3 to AMS/AMS 1.8V for version 3.1 only. */
	{CPCAP_REG_S3C,       0x0445, is_cpcap_eq_3_1  },
	/* Set SW3 to Pulse Skip/PFM. */
	{CPCAP_REG_S3C,       0x043d, NULL             },
	/* Set SW4 to AMS/AMS 1.2v for version 3.1 only. */
	{CPCAP_REG_S4C1,      0x4830, is_cpcap_eq_3_1  },
	/* Set SW4 to PFM/PFM 1.2v. */
	{CPCAP_REG_S4C1,      0x4930, NULL             },
	/* Set SW4 down to 0.95v when secondary standby is asserted. */
	{CPCAP_REG_S4C2,      0x301c, NULL             },
	/* Set SW5 to On/Off. */
	{CPCAP_REG_S5C,       0x0020, NULL             },
	/* Set SW6 to Off/Off. */
	{CPCAP_REG_S6C,       0x0000, NULL             },
	/* Set VCAM to Off/Off. */
	{CPCAP_REG_VCAMC,     0x0030, NULL             },
	/* Set VCSI to AMS/Off 1.2v. */
	{CPCAP_REG_VCSIC,     0x0007, NULL             },
	{CPCAP_REG_VDACC,     0x0000, NULL             },
	{CPCAP_REG_VDIGC,     0x0000, NULL             },
	{CPCAP_REG_VFUSEC,    0x0000, NULL             },
	/* Set VHVIO to On/LP. */
	{CPCAP_REG_VHVIOC,    0x0002, NULL             },
	/* Set VSDIO to On/LP. */
	{CPCAP_REG_VSDIOC,    0x003A, NULL             },
	/* Set VPLL to On/Off. */
	{CPCAP_REG_VPLLC,     0x0019, NULL             },
	{CPCAP_REG_VRF1C,     0x0002, NULL             },
	{CPCAP_REG_VRF2C,     0x0000, NULL             },
	{CPCAP_REG_VRFREFC,   0x0000, NULL             },
	/* Set VWLAN1 to off */
	{CPCAP_REG_VWLAN1C,   0x0000, is_olympus_ge_p3 },
	/* Set VWLAN1 to AMS/AMS 1.8v */
	{CPCAP_REG_VWLAN1C,   0x0005, NULL             },
	/* Set VWLAN2 to On/LP 3.3v. */
	{CPCAP_REG_VWLAN2C,   0x0089, is_olympus_ge_p3 },
	/* Set VWLAN2 to On/On 3.3v */
	{CPCAP_REG_VWLAN2C,   0x008d, NULL             },
	/* Set VSIMCARD to AMS/Off 2.9v. */
	{CPCAP_REG_VSIMC,     0x1e08, NULL             },
	/* Set to off 3.0v. */
	{CPCAP_REG_VVIBC,     0x000C, NULL             },
	/* Set VUSB to On/On */
	{CPCAP_REG_VUSBC,     0x004C, NULL             },
	{CPCAP_REG_VUSBINT1C, 0x0000, NULL             },
	{CPCAP_REG_USBC1,     0x1201, NULL             },
	{CPCAP_REG_USBC2,     0xC058, NULL             },
	{CPCAP_REG_USBC3,     0x7DFF, NULL             },
	/* one wire level shifter */
	{CPCAP_REG_OWDC,      0x0003, NULL             },
	/* power cut is enabled, the timer is set to 312.5 ms */
	{CPCAP_REG_PC1,       0x010A, NULL             },
	/* power cut counter is enabled to prevent ambulance mode */
	{CPCAP_REG_PC2,       0x0150, NULL             },
	/* Enable coin cell charger and set charger voltage to 3.0 V
	   Enable coulomb counter, enable dithering and set integration
	   period to 250 mS*/
	{CPCAP_REG_CCCC2,     0x002B, NULL             },
    /* Set ADC_CLK to 3 MHZ
       Disable leakage currents into channels between ADC
	   conversions */
	{CPCAP_REG_ADCC1,     0x9000, NULL             },
	/* Disable TS_REF
	   Enable coin cell charger input to A/D
	   Ignore ADTRIG signal
	   THERMBIAS pin is open circuit
	   Use B+ for ADC channel 4, Bank 0
	   Enable BATDETB comparator
	   Do not apply calibration offsets to ADC current readings */
	{CPCAP_REG_ADCC2,     0x4136, NULL             },
	/* Clear UC Control 1 */
	{CPCAP_REG_UCC1,      0x0000, NULL             },
};

/* STI-OLY replaced with atrix ones 	
unsigned short cpcap_regulator_mode_values[CPCAP_NUM_REGULATORS] = {
		
	[CPCAP_SW2]      = 0x0800,
	[CPCAP_SW4]      = 0x0900,
	[CPCAP_SW5]      = 0x0022,
	[CPCAP_VCAM]     = 0x0007,
	[CPCAP_VCSI]     = 0x0007,
	[CPCAP_VDAC]     = 0x0003,
	[CPCAP_VDIG]     = 0x0005,
	[CPCAP_VFUSE]    = 0x0080,
	[CPCAP_VHVIO]    = 0x0002,
	[CPCAP_VSDIO]    = 0x0002,
	[CPCAP_VPLL]     = 0x0001,
	[CPCAP_VRF1]     = 0x000C,
	[CPCAP_VRF2]     = 0x0003,
	[CPCAP_VRFREF]   = 0x0003,
	[CPCAP_VWLAN1]   = 0x0005,
	[CPCAP_VWLAN2]   = 0x0008,
	[CPCAP_VSIM]     = 0x0003,
	[CPCAP_VSIMCARD] = 0x1E00,
	[CPCAP_VVIB]     = 0x0001,
	[CPCAP_VUSB]     = 0x000C,
	[CPCAP_VAUDIO]   = 0x0004, */

struct cpcap_mode_value *cpcap_regulator_mode_values[] = {
	[CPCAP_SW1] = (struct cpcap_mode_value []) {
		/* AMS/AMS Primary control via Macro */
		{0x6800, NULL }
	},
	[CPCAP_SW2] = (struct cpcap_mode_value []) {
		/* AMS/AMS Secondary control via Macro */
		{0x4804, NULL }
	},
	[CPCAP_SW3] = (struct cpcap_mode_value []) {
		/* AMS/AMS Secondary Standby */
		{0x0040, is_cpcap_eq_3_1  },
		/* Pulse Skip/PFM Secondary Standby */
		{0x043c, NULL             },
	},
	[CPCAP_SW4] = (struct cpcap_mode_value []) {
		/* AMS/AMS Secondary Standby */
		{ 0x0800, is_cpcap_eq_3_1  },
		/* PFM/PFM Secondary Standby */
		{ 0x4909, NULL             },
	},
	[CPCAP_SW5] = (struct cpcap_mode_value []) {
		/* All versions of olympus and sunfire support shutting down SW5
		   when binking the message LED.  Set SW5 to On/Off Secondary
		   control when off. */
		{ CPCAP_REG_OFF_MODE_SEC | 0x0020, is_olympus_ge_p0 },
		/* On/Off */
		{ 0x0020, NULL             },
	},
	[CPCAP_VCAM] = (struct cpcap_mode_value []) {
		/* AMS/Off Secondary Standby */
		{0x0007, NULL }
	},
	[CPCAP_VCSI] = (struct cpcap_mode_value []) {
		/* AMS/Off Secondary Standby */
		{0x0007, NULL }
	},
	[CPCAP_VDAC] = (struct cpcap_mode_value []) {
		/* Off/Off */
		{0x0000, NULL }
	},
	[CPCAP_VDIG] = (struct cpcap_mode_value []) {
		/* Off/Off */
		{0x0000, NULL }
	},
	[CPCAP_VFUSE] = (struct cpcap_mode_value []) {
		/* Off/Off */
		{0x0000, NULL }
	},
	[CPCAP_VHVIO] = (struct cpcap_mode_value []) {
		/* On/LP  Secondary Standby */
		{0x0002, NULL }
	},
	[CPCAP_VSDIO] = (struct cpcap_mode_value []) {
		/* On/LP  Secondary Standby */
		{0x0002, NULL }
	},
	[CPCAP_VPLL] = (struct cpcap_mode_value []) {
		/* On/Off Secondary Standby */
		{0x0001, NULL }
	},
	[CPCAP_VRF1] = (struct cpcap_mode_value []) {
		/* Off/Off */
		{0x0000, NULL }
	},
	[CPCAP_VRF2] = (struct cpcap_mode_value []) {
		/* Off/Off */
		{0x0000, NULL }
	},
	[CPCAP_VRFREF] = (struct cpcap_mode_value []) {
		/* Off/Off */
		{0x0000, NULL  }
	},
	[CPCAP_VWLAN1] = (struct cpcap_mode_value []) {
		/* Off/Off */
		{0x0000, is_olympus_ge_p3 },
		/* AMS/AMS. */
		{0x0005, NULL             },
	},
	[CPCAP_VWLAN2] = (struct cpcap_mode_value []) {
		/* On/LP 3.3v Secondary Standby (external pass) */
		{0x0009, is_olympus_ge_p3 },
		/* On/On 3.3v (external pass) */
		{0x000D, NULL             },
	},
	[CPCAP_VSIM] = (struct cpcap_mode_value []) {
		/* Off/Off */
		{0x0000, NULL }
	},
	[CPCAP_VSIMCARD] = (struct cpcap_mode_value []) {
		/* AMS/Off Secondary Standby */
		{0x1E00, NULL }
	},
	[CPCAP_VVIB] = (struct cpcap_mode_value []) {
		/* On */
		{0x0001, NULL }
	},
	[CPCAP_VUSB] = (struct cpcap_mode_value []) {
		/* On/On */
		{0x000C, NULL }
	},
	[CPCAP_VAUDIO] = (struct cpcap_mode_value []) {
		/* On/LP Secondary Standby */
		{0x0005, NULL }
	},	

};
/* STI-OLY replaced with atrix ones 	
unsigned short cpcap_regulator_off_mode_values[CPCAP_NUM_REGULATORS] = {
	[CPCAP_SW2]      = 0x0000,
	[CPCAP_SW4]      = 0x0000,
	[CPCAP_SW5]      = 0x0000,
	[CPCAP_VCAM]     = 0x0000,
	[CPCAP_VCSI]     = 0x0000,
	[CPCAP_VDAC]     = 0x0000,
	[CPCAP_VDIG]     = 0x0000,
	[CPCAP_VFUSE]    = 0x0000,
	[CPCAP_VHVIO]    = 0x0000,
	[CPCAP_VSDIO]    = 0x0000,
	[CPCAP_VPLL]     = 0x0000,
	[CPCAP_VRF1]     = 0x0000,
	[CPCAP_VRF2]     = 0x0000,
	[CPCAP_VRFREF]   = 0x0000,
	[CPCAP_VWLAN1]   = 0x0000,
	[CPCAP_VWLAN2]   = 0x0000,
	[CPCAP_VSIM]     = 0x0000,
	[CPCAP_VSIMCARD] = 0x0000,
	[CPCAP_VVIB]     = 0x0000,
	[CPCAP_VUSB]     = 0x0000,
	[CPCAP_VAUDIO]   = 0x0000,*/

struct cpcap_mode_value *cpcap_regulator_off_mode_values[] = {
	[CPCAP_SW1] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_SW2] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_SW3] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_SW4] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_SW5] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VCAM] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VCSI] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VDAC] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VDIG] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VFUSE] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VHVIO] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VSDIO] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VPLL] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VRF1] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VRF2] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VRFREF] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VWLAN1] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VWLAN2] = (struct cpcap_mode_value []) {
		/* Turn off only once sec standby is entered. */
		{0x0004, is_olympus_ge_p3 },
		{0x0000, NULL             },
	},
	[CPCAP_VSIM] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VSIMCARD] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VVIB] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VUSB] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},
	[CPCAP_VAUDIO] = (struct cpcap_mode_value []) {
		{0x0000, NULL }
	},

};

#define REGULATOR_CONSUMER(name, device) { .supply = name, .dev_name = device, }
#define REGULATOR_CONSUMER_BY_DEVICE(name, device) \
	{ .supply = name, .dev = device, }

struct regulator_consumer_supply cpcap_sw1_consumers[] = {
	REGULATOR_CONSUMER("sw1", NULL /* core */),
};

struct regulator_consumer_supply cpcap_sw2_consumers[] = {
	REGULATOR_CONSUMER("sw2", NULL /* core */),
};

struct regulator_consumer_supply cpcap_sw3_consumers[] = {
	REGULATOR_CONSUMER("sw3", NULL /* VIO */),
	REGULATOR_CONSUMER("odm-kit-vio", NULL),
};

struct regulator_consumer_supply cpcap_sw4_consumers[] = {
	REGULATOR_CONSUMER("sw4", NULL /* core */),
};

struct regulator_consumer_supply cpcap_sw5_consumers[] = {
	REGULATOR_SUPPLY("sw5", "button-backlight"),
	REGULATOR_SUPPLY("sw5", "notification-led"),
	REGULATOR_CONSUMER("odm-kit-sw5", NULL),
	REGULATOR_SUPPLY("sw5", NULL),
};

struct regulator_consumer_supply cpcap_vcam_consumers[] = {
	REGULATOR_CONSUMER("vcam", NULL /* cpcap_cam_device */),
	REGULATOR_CONSUMER("odm-kit-vcam", NULL),
};

struct regulator_consumer_supply cpcap_vhvio_consumers[] = {
	REGULATOR_CONSUMER("vhvio", NULL /* lighting_driver */),
	REGULATOR_CONSUMER("odm-kit-vhvio", NULL),
#if 0
	REGULATOR_CONSUMER("vhvio", NULL /* lighting_driver */),
	REGULATOR_CONSUMER("vhvio", NULL /* magnetometer */),
	REGULATOR_CONSUMER("vhvio", NULL /* light sensor */),
	REGULATOR_CONSUMER("vhvio", NULL /* accelerometer */),
	REGULATOR_CONSUMER("vhvio", NULL /* display */),
#endif
};

struct regulator_consumer_supply cpcap_vsdio_consumers[] = {
	REGULATOR_CONSUMER("vsdio", NULL),
	REGULATOR_CONSUMER("odm-kit-vsdio", NULL)
};

struct regulator_consumer_supply cpcap_vpll_consumers[] = {
	REGULATOR_CONSUMER("vpll", NULL),
	REGULATOR_CONSUMER("odm-kit-vpll", NULL),
};

struct regulator_consumer_supply cpcap_vcsi_consumers[] = {
	REGULATOR_CONSUMER("vcsi", NULL),
};

struct regulator_consumer_supply cpcap_vwlan1_consumers[] = {
	REGULATOR_CONSUMER("vwlan1", NULL),
	REGULATOR_CONSUMER("odm-kit-vwlan1", NULL),
};

struct regulator_consumer_supply cpcap_vwlan2_consumers[] = {
	REGULATOR_CONSUMER("vwlan2", NULL),
	/* Powers the tegra usb block, cannot be named vusb, since
	   this name already exists in regulator-cpcap.c. */
	REGULATOR_CONSUMER("vusb_modem_flash", NULL),
	REGULATOR_CONSUMER("vusb_modem_ipc", NULL),
	REGULATOR_CONSUMER("vhdmi", NULL),
	REGULATOR_CONSUMER("odm-kit-vwlan2", NULL),
};

struct regulator_consumer_supply cpcap_vsimcard_consumers[] = {
	REGULATOR_CONSUMER("vsimcard", NULL /* sd slot */),
	REGULATOR_CONSUMER("odm-kit-vsimcard", NULL),
};

/* STI-OLY not used
struct regulator_consumer_supply cpcap_vusb_consumers[] = {
	REGULATOR_CONSUMER_BY_DEVICE("vusb", &cpcap_whisper_device.dev),
}; */

struct regulator_consumer_supply cpcap_vvib_consumers[] = {
	REGULATOR_CONSUMER("vvib", NULL /* vibrator */),
};

struct regulator_consumer_supply cpcap_vaudio_consumers[] = {
	REGULATOR_CONSUMER("vaudio", NULL /* mic opamp */),
	REGULATOR_CONSUMER("odm-kit-vaudio", NULL /* mic opamp */),
};
/* STI-OLY not used */
#if 0 
struct regulator_consumer_supply cpcap_vdig_consumers[] = {
	REGULATOR_CONSUMER("vdig", NULL /* gps */),
};
#endif
static struct regulator_init_data cpcap_regulator[CPCAP_NUM_REGULATORS] = {
	[CPCAP_SW1] = {
		.constraints = {
			.min_uV			= 750000,
			.max_uV			= 1100000,
			.valid_ops_mask		= REGULATOR_CHANGE_STATUS |
                                                  REGULATOR_CHANGE_VOLTAGE,
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_sw1_consumers),
		.consumer_supplies	= cpcap_sw1_consumers,
	},	
	[CPCAP_SW2] = {
		.constraints = {
			.min_uV			= 900000,
			.max_uV			= 1200000,
			.valid_ops_mask		= REGULATOR_CHANGE_STATUS |
                                                  REGULATOR_CHANGE_VOLTAGE,
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_sw2_consumers),
		.consumer_supplies	= cpcap_sw2_consumers,
	},
	[CPCAP_SW3] = {
		.constraints = {
			.min_uV			= 1350000,
			.max_uV			= 1875000,
			.valid_ops_mask		= REGULATOR_CHANGE_STATUS |
                                                 REGULATOR_CHANGE_VOLTAGE,
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_sw3_consumers),
		.consumer_supplies	= cpcap_sw3_consumers,
	},
	[CPCAP_SW4] = {
		.constraints = {
			.min_uV			= 900000,
			.max_uV			= 1200000,
			.valid_ops_mask		= REGULATOR_CHANGE_STATUS |
                                                  REGULATOR_CHANGE_VOLTAGE,
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_sw4_consumers),
		.consumer_supplies	= cpcap_sw4_consumers,
	},
	[CPCAP_SW5] = {
		.constraints = {
			.min_uV			= 5050000,
			.max_uV			= 5050000,
			.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
			.apply_uV		= 1,
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_sw5_consumers),
		.consumer_supplies	= cpcap_sw5_consumers,
	},
	[CPCAP_VCAM] = {
		.constraints = {
			.min_uV			= 2600000,
			.max_uV			= 2900000,
			.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
			/* STI-OLY.apply_uV		= 1,*/

		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_vcam_consumers),
		.consumer_supplies	= cpcap_vcam_consumers,
	},
	[CPCAP_VCSI] = {
		.constraints = {
			.min_uV			= 1200000,
			.max_uV			= 1200000,
			.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
			.boot_on		= 1,
			.apply_uV		= 1,
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_vcsi_consumers),
		.consumer_supplies	= cpcap_vcsi_consumers,
	},
	[CPCAP_VDAC] = {
		.constraints = {
			.min_uV			= 1200000,
			.max_uV			= 2500000,
			.valid_ops_mask		= 0,
		},
	},
	[CPCAP_VDIG] = {
		.constraints = {
			.min_uV			= 1200000,
			.max_uV			= 1875000,
			.valid_ops_mask		= 0,
		/* STI-OLY
			.always_on		= 1,
			.apply_uV		= 1,*/
		},
		/* STI-OLY
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_vdig_consumers),
		.consumer_supplies	= cpcap_vdig_consumers, */
	},
	[CPCAP_VFUSE] = {
		.constraints = {
			.min_uV			= 1500000,
			.max_uV			= 3150000,
			.valid_ops_mask		= 0,
			/* STI-OLY
			.valid_ops_mask		= (REGULATOR_CHANGE_VOLTAGE |
						   REGULATOR_CHANGE_STATUS),*/
		},
	},
	[CPCAP_VHVIO] = {
		.constraints = {
			.min_uV			= 2775000,
			.max_uV			= 2775000,
			.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
			.always_on		= 1,
			.apply_uV		= 1,
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_vhvio_consumers),
		.consumer_supplies	= cpcap_vhvio_consumers,
	},
	[CPCAP_VSDIO] = {
		.constraints = {
			.min_uV			= 1500000,
			.max_uV			= 3000000,
			/* STI-OLY			
			.valid_ops_mask		= 0,
			.always_on		= 1,
			.apply_uV		= 1,
		},*/
			.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_vsdio_consumers),
		.consumer_supplies	= cpcap_vsdio_consumers,
			
	},
	[CPCAP_VPLL] = {
		.constraints = {
			.min_uV			= 1800000,
			.max_uV			= 1800000,
			.valid_ops_mask		= 0,
			/* STI-OLY .always_on		= 1, */
			.apply_uV		= 1,
		},
		.num_consumer_supplies = ARRAY_SIZE(cpcap_vpll_consumers),
		.consumer_supplies = cpcap_vpll_consumers,
	},
	[CPCAP_VRF1] = {
		.constraints = {
			.min_uV			= 2500000,
			.max_uV			= 2775000,
			.valid_ops_mask		= 0,
		},
	},
	[CPCAP_VRF2] = {
		.constraints = {
			.min_uV			= 2775000,
			.max_uV			= 2775000,
			.valid_ops_mask		= 0,
		},
	},
	[CPCAP_VRFREF] = {
		.constraints = {
			.min_uV			= 2500000,
			.max_uV			= 2775000,
			.valid_ops_mask		= 0,
		},
	},
	[CPCAP_VWLAN1] = {
		.constraints = {
			.min_uV			= 1800000,
			.max_uV			= 1900000,
			.valid_ops_mask		= 0,
			/* STI-OLY .always_on		= 1,*/
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_vwlan1_consumers),
		.consumer_supplies	= cpcap_vwlan1_consumers,
	},
	[CPCAP_VWLAN2] = {
		.constraints = {
			.min_uV			= 2775000,
			.max_uV			= 3300000,
			/* STI-OLY .valid_ops_mask		= 0,
			.always_on		= 1,
			.apply_uV		= 1,
		},*/
			.valid_ops_mask		= (REGULATOR_CHANGE_VOLTAGE | REGULATOR_CHANGE_STATUS),
			.always_on		= 1,  /* Reinitialized based on hwrev in mot_setup_power() */
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_vwlan2_consumers),
		.consumer_supplies	= cpcap_vwlan2_consumers,
	},
	[CPCAP_VSIM] = {
		.constraints = {
			.min_uV			= 1800000,
			.max_uV			= 2900000,
			.valid_ops_mask		= 0,
		},
	},
	[CPCAP_VSIMCARD] = {
		.constraints = {
			.min_uV			= 1800000,
			.max_uV			= 2900000,
			.valid_ops_mask		= 0,
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_vsimcard_consumers),
		.consumer_supplies	= cpcap_vsimcard_consumers,
	},
	[CPCAP_VVIB] = {
		.constraints = {
			.min_uV			= 1300000,
			.max_uV			= 3000000,
			/*STI-OLY .valid_ops_mask		= 0,
		},*/
			.valid_ops_mask		= (REGULATOR_CHANGE_VOLTAGE |
						   REGULATOR_CHANGE_STATUS),
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_vvib_consumers),
		.consumer_supplies	= cpcap_vvib_consumers,
	},
	[CPCAP_VUSB] = {
		.constraints = {
			.min_uV			= 3300000,
			.max_uV			= 3300000,
			.valid_ops_mask		= REGULATOR_CHANGE_STATUS,
			.apply_uV		= 1,
		},
		/*.num_consumer_supplies	= ARRAY_SIZE(cpcap_vusb_consumers),
		.consumer_supplies	= cpcap_vusb_consumers,*/
	},
	[CPCAP_VAUDIO] = {
		.constraints = {
			.min_uV			= 2775000,
			.max_uV			= 2775000,
			.valid_modes_mask	= (REGULATOR_MODE_NORMAL |
						   REGULATOR_MODE_STANDBY |
						   REGULATOR_MODE_IDLE),
			.valid_ops_mask		= REGULATOR_CHANGE_MODE,
			.always_on		= 1,
			.apply_uV		= 1,
		},
		.num_consumer_supplies	= ARRAY_SIZE(cpcap_vaudio_consumers),
		.consumer_supplies	= cpcap_vaudio_consumers,
	},
};

/* ADC conversion delays for battery V and I measurments taken in and out of TX burst  */
static struct cpcap_adc_ato olympus_cpcap_adc_ato = {
	/* STI-OLY replaced 
	.ato_in = 0x0480,
	.atox_in = 0,
	.adc_ps_factor_in = 0x0200,
	.atox_ps_factor_in = 0,
	.ato_out = 0,
	.atox_out = 0,
	.adc_ps_factor_out = 0,
	.atox_ps_factor_out = 0, */
	.ato_in 		= 0x0300,
	.atox_in 		= 0x0000,
	.adc_ps_factor_in 	= 0x0200,
	.atox_ps_factor_in 	= 0x0000,
	.ato_out 		= 0x0780,
	.atox_out 		= 0x0000,
	.adc_ps_factor_out 	= 0x0600,
	.atox_ps_factor_out 	= 0x0000,
};

static struct cpcap_platform_data olympus_cpcap_data = {
	.init = olympus_cpcap_spi_init,
	.init_len = ARRAY_SIZE(olympus_cpcap_spi_init),
	.leds = &olympus_cpcap_leds,
	.regulator_mode_values = cpcap_regulator_mode_values,
	.regulator_off_mode_values = cpcap_regulator_off_mode_values,
	.regulator_init = cpcap_regulator,
	.adc_ato = &olympus_cpcap_adc_ato,
	/* STI-OLY
	.ac_changed = NULL,
	.batt_changed = NULL,
	.usb_changed = NULL, */
	.wdt_disable = 0,
	.hwcfg = {
		(CPCAP_HWCFG0_SEC_STBY_SW3 |
		 CPCAP_HWCFG0_SEC_STBY_SW4 |
		 CPCAP_HWCFG0_SEC_STBY_VAUDIO |
		 CPCAP_HWCFG0_SEC_STBY_VCAM |
		 CPCAP_HWCFG0_SEC_STBY_VCSI |
		 CPCAP_HWCFG0_SEC_STBY_VHVIO |
		 CPCAP_HWCFG0_SEC_STBY_VPLL |
		 CPCAP_HWCFG0_SEC_STBY_VSDIO),
		(CPCAP_HWCFG1_SEC_STBY_VWLAN1 |    /* WLAN1 may be reset in mot_setup_power(). */
		 CPCAP_HWCFG1_SEC_STBY_VSIMCARD)},
	.spdif_gpio = TEGRA_GPIO_PD4
};

static struct spi_board_info olympus_spi_board_info[] __initdata = {
#ifdef CONFIG_MFD_CPCAP
    {
        .modalias = "cpcap",
        .bus_num = 1,
        .chip_select = 0,
        .mode = SPI_MODE_0,
        .max_speed_hz = 8000000,
        .controller_data = &olympus_cpcap_data,
        .irq = INT_EXTERNAL_PMU,
    },
#elif defined CONFIG_SPI_SPIDEV
    {
        .modalias = "spidev",
        .bus_num = 1,
        .chip_select = 0,
        .mode = SPI_MODE_0,
        .max_speed_hz = 18000000,
        .platform_data = NULL,
        .irq = 0,
    },
#endif
};

/* STI-OLY replacing with atrix regulator */
#if 0
struct regulator_consumer_supply max8649_consumers[] = {
	REGULATOR_CONSUMER("vdd_cpu", NULL /* cpu */),
};

struct regulator_init_data max8649_regulator_init_data[] = {
	{
		.constraints = {
			.min_uV			= 770000,
			.max_uV			= 1100000,
			.valid_ops_mask		= REGULATOR_CHANGE_VOLTAGE,
			.always_on		= 1,
		},
		.num_consumer_supplies	= ARRAY_SIZE(max8649_consumers),
		.consumer_supplies	= max8649_consumers,
	},
};

struct max8649_platform_data stingray_max8649_pdata = {
	.regulator = max8649_regulator_init_data,
	.mode = 1,
	.extclk = 0,
	.ramp_timing = MAX8649_RAMP_32MV,
	.ramp_down = 0,
};

static struct i2c_board_info __initdata stingray_i2c_bus4_power_info[] = {
	{
		I2C_BOARD_INFO("max8649", 0x60),
		.platform_data = &stingray_max8649_pdata,
	},
};
#endif

struct regulator_consumer_supply fixed_sdio_en_consumers[] = {
	REGULATOR_SUPPLY("vsdio_ext", NULL),
};

static struct regulator_init_data fixed_sdio_regulator = {
	.constraints = {
		.min_uV = 2800000,
		.max_uV = 2800000,
		.valid_ops_mask = REGULATOR_CHANGE_STATUS,
	},
	.num_consumer_supplies = ARRAY_SIZE(fixed_sdio_en_consumers),
	.consumer_supplies = fixed_sdio_en_consumers
};

static struct fixed_voltage_config fixed_sdio_config = {
	.supply_name = "sdio_en",
	.microvolts = 2800000,
	.gpio = TEGRA_GPIO_PF3,
	.enable_high = 1,
	.enabled_at_boot = 0,		/* Needs to be enabled on older oly & etna below */
	.init_data = &fixed_sdio_regulator,
};

static struct platform_device fixed_regulator_devices[] = {
	{
		.name = "reg-fixed-voltage",
		.id = 1,
		.dev = {
			.platform_data = &fixed_sdio_config,
		},
	},
};

static struct mdm_ctrl_platform_data mdm_ctrl_platform_data = {
	.gpios[MDM_CTRL_GPIO_AP_STATUS_0] = {
		TEGRA_GPIO_PC1, MDM_GPIO_DIRECTION_OUT, 0, 0, "mdm_ap_status0"},
	.gpios[MDM_CTRL_GPIO_AP_STATUS_1] = {
		TEGRA_GPIO_PC6, MDM_GPIO_DIRECTION_OUT, 0, 0, "mdm_ap_status1"},
	.gpios[MDM_CTRL_GPIO_AP_STATUS_2] = {
		TEGRA_GPIO_PQ3, MDM_GPIO_DIRECTION_OUT, 0, 0, "mdm_ap_status2"},
	.gpios[MDM_CTRL_GPIO_BP_STATUS_0] = {
		TEGRA_GPIO_PK3, MDM_GPIO_DIRECTION_IN, 0, 0, "mdm_bp_status0"},
	.gpios[MDM_CTRL_GPIO_BP_STATUS_1] = {
		TEGRA_GPIO_PK4, MDM_GPIO_DIRECTION_IN, 0, 0, "mdm_bp_status1"},
	.gpios[MDM_CTRL_GPIO_BP_STATUS_2] = {
		TEGRA_GPIO_PK2, MDM_GPIO_DIRECTION_IN, 0, 0, "mdm_bp_status2"},
	.gpios[MDM_CTRL_GPIO_BP_RESOUT]   = {
		TEGRA_GPIO_PS4, MDM_GPIO_DIRECTION_IN, 0, 0, "mdm_bp_resout"},
	.gpios[MDM_CTRL_GPIO_BP_RESIN]    = {
		TEGRA_GPIO_PZ1, MDM_GPIO_DIRECTION_OUT, 0, 0, "mdm_bp_resin"},
	.gpios[MDM_CTRL_GPIO_BP_PWRON]    = {
		TEGRA_GPIO_PS6, MDM_GPIO_DIRECTION_OUT, 0, 0, "mdm_bp_pwr_on"},
	.cmd_gpios = {TEGRA_GPIO_PQ5, TEGRA_GPIO_PS5},
};

static struct platform_device mdm_ctrl_platform_device = {
	.name = MDM_CTRL_MODULE_NAME,
	.id = -1,
	.dev = {
		.platform_data = &mdm_ctrl_platform_data,
	},
};

static void mdm_ctrl_register(void)
{
	int i;

	for (i = 0; i < MDM_CTRL_NUM_GPIOS; i++)
		tegra_gpio_enable(mdm_ctrl_platform_data.gpios[i].number);

	if (olympus_qbp_usb_hw_bypass_enabled()) {
		/* The default AP status is "no bypass", so we must override it */
		mdm_ctrl_platform_data.gpios[MDM_CTRL_GPIO_AP_STATUS_0]. \
				default_value = 1;
		mdm_ctrl_platform_data.gpios[MDM_CTRL_GPIO_AP_STATUS_1]. \
				default_value = 0;
		mdm_ctrl_platform_data.gpios[MDM_CTRL_GPIO_AP_STATUS_2]. \
				default_value = 0;
	}

	platform_device_register(&mdm_ctrl_platform_device);
}

#ifdef CONFIG_REGULATOR_VIRTUAL_CONSUMER
static struct platform_device cpcap_reg_virt_vcam =
{
    .name = "reg-virt-vcam",
    .id   = -1,
    .dev =
    {
        .platform_data = "vcam",
    },
};
static struct platform_device cpcap_reg_virt_vcsi =
{
    .name = "reg-virt-vcsi",
    .id   = -1,
    .dev =
    {
        .platform_data = "vcsi",
    },
};
static struct platform_device cpcap_reg_virt_vcsi_2 =
{
    .name = "reg-virt-vcsi_2",
    .id   = -1,
    .dev =
    {
        .platform_data = "vcsi",
    },
};
static struct platform_device cpcap_reg_virt_sw5 =
{
    .name = "reg-virt-sw5",
    .id   = -1,
    .dev =
    {
        .platform_data = "sw5",
    },
};
#endif

int __init olympus_power_init(void)
{
	int i;
	unsigned long pmc_cntrl_0;
	int error;

	/* Enable CORE_PWR_REQ signal from T20. The signal must be enabled
	 * before the CPCAP uC firmware is started. */
	pmc_cntrl_0 = readl(IO_ADDRESS(TEGRA_PMC_BASE));
	pmc_cntrl_0 |= 0x00000200;
	writel(pmc_cntrl_0, IO_ADDRESS(TEGRA_PMC_BASE));


#ifdef ENABLE_SOFT_RESET_DEBUGGING
	/* Only P3 and later hardware supports CPCAP resetting the T20. */
	if (olympus_revision() >= OLYMPUS_REVISION_3)
		olympus_cpcap_data.hwcfg[1] |= CPCAP_HWCFG1_SOFT_RESET_HOST;
#endif

	tegra_gpio_enable(TEGRA_GPIO_PT2);
	gpio_request(TEGRA_GPIO_PT2, "usb_host_pwr_en");
	gpio_direction_output(TEGRA_GPIO_PT2, 0);

	spi_register_board_info(olympus_spi_board_info,
				ARRAY_SIZE(olympus_spi_board_info));

	for (i = 0; i < ARRAY_SIZE(cpcap_devices); i++)
		cpcap_device_register(cpcap_devices[i]);

	for (i = 0; i < sizeof(fixed_regulator_devices)/sizeof(fixed_regulator_devices[0]); i++) {
		error = platform_device_register(&fixed_regulator_devices[i]);
		pr_info("Registered reg-fixed-voltage: %d result: %d\n", i, error);
	}

	if (!olympus_qbp_usb_hw_bypass_enabled())
		cpcap_device_register(&cpcap_whisper_device);

	(void) cpcap_driver_register(&cpcap_validity_driver);

#ifdef CONFIG_REGULATOR_VIRTUAL_CONSUMER
	(void) platform_device_register(&cpcap_reg_virt_vcam);
	(void) platform_device_register(&cpcap_reg_virt_vcsi);
	(void) platform_device_register(&cpcap_reg_virt_vcsi_2);
	(void) platform_device_register(&cpcap_reg_virt_sw5);
#endif

	/* STI-OLY registering max8649 which we don't have
	i2c_register_board_info(3, stingray_i2c_bus4_power_info,
		ARRAY_SIZE(stingray_i2c_bus4_power_info));*/

	/*
	if (stingray_hw_has_cdma() || stingray_hw_has_umts()) {*/
		mdm_ctrl_register();
	/*		wrigley_ctrl_register();
	} */

	return 0;
}
