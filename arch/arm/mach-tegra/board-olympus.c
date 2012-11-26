/*
 * arch/arm/mach-tegra/board-stingray.c
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

#include <linux/console.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/serial_8250.h>
#include <linux/serial_core.h>
#include <linux/clk.h>
#include <linux/mtd/mtd.h>
#include <linux/mtd/partitions.h>
#include <linux/dma-mapping.h>
#include <linux/fsl_devices.h>
#include <linux/platform_data/tegra_usb.h>
#include <linux/platform_data/ram_console.h>
#include <linux/pda_power.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/reboot.h>
#include <linux/i2c.h>
#include <linux/i2c-tegra.h>
#include <linux/spi/cpcap.h>
#include <linux/memblock.h>
#include <linux/tegra_uart.h>

#include <linux/qtouch_obp_ts.h>
#include <linux/isl29030.h>
#include <linux/leds-lm3530.h>
#include <linux/leds-lm3532.h>
#include <linux/kxtf9.h>

#include <asm/mach-types.h>
#include <asm/mach/arch.h>
#include <asm/mach/time.h>
#include <asm/setup.h>

#include <mach/apanic.h>
#include <mach/nand.h>
#include <mach/io.h>
#include <mach/w1.h>
#include <mach/iomap.h>
#include <mach/irqs.h>
#include <mach/iomap.h>
#include <mach/sdhci.h>
#include <mach/gpio.h>
#include <mach/clk.h>
#include <mach/usb_phy.h>
#include <mach/i2s.h>
#include <mach/spdif.h>
#include <mach/audio.h>
#include <mach/cpcap_audio.h>
#include <mach/system.h>
#include <mach/tegra_fiq_debugger.h>
#include <mach/tegra_hsuart.h>
#include <mach/nvmap.h>
#include <mach/bcm_bt_lpm.h>

#include <linux/usb/oob_wake.h>

#include "board.h"
#include "board-olympus.h"
#include "clock.h"
#include "gpio-names.h"
#include "devices.h"
#include "pm.h"

#include <linux/delay.h>

/* NVidia bootloader tags */
#define ATAG_NVIDIA		0x41000801

#define ATAG_NVIDIA_RM			0x1
#define ATAG_NVIDIA_DISPLAY		0x2
#define ATAG_NVIDIA_FRAMEBUFFER		0x3
#define ATAG_NVIDIA_CHIPSHMOO		0x4
#define ATAG_NVIDIA_CHIPSHMOOPHYS	0x5
#define ATAG_NVIDIA_PRESERVED_MEM_0	0x10000
#define ATAG_NVIDIA_PRESERVED_MEM_N	2
#define ATAG_NVIDIA_FORCE_32		0x7fffffff

#define XMEGAT_I2C_ADDR			0x24 

#define OLYMPUS_TOUCH_RESET_N_GPIO	TEGRA_GPIO_PF4
#define OLYMPUS_TOUCH_INT_N_GPIO	TEGRA_GPIO_PF5

#define TEGRA_PROX_INT_GPIO		TEGRA_GPIO_PE1
#define TEGRA_KXTF9_INT_GPIO		TEGRA_GPIO_PV3

#define TEGRA_BACKLIGHT_EN_GPIO 32 /* TEGRA_GPIO_PE0 */
#define TEGRA_KEY_BACKLIGHT_EN_GPIO 47 /* TEGRA_GPIO_PE0 */

#define ATAG_NONE	0x00000000
#define ATAG_CORE	0x54410001
#define ATAG_MEM	0x54410002
#define ATAG_VIDEOTEXT	0x54410003
#define ATAG_RAMDISK	0x54410004
#define ATAG_INITRD	0x54410005
#define ATAG_INITRD2	0x54420005
#define ATAG_SERIAL	0x54410006
#define ATAG_REVISION	0x54410007
#define ATAG_VIDEOLFB	0x54410008
#define ATAG_CMDLINE	0x54410009
#define ATAG_ACORN	0x41000101
#define ATAG_MEMCLK	0x41000402
#define ATAG_NVIDIA_TEGRA 0x41000801
#define ATAG_MOTOROLA 0x41000810
#define ATAG_WLAN_MAC 0x57464d41
#define ATAG_WLAN_MAC_LEN 6
#define ATAG_BLDEBUG 0x41000811
#define ATAG_POWERUP_REASON 0xf1000401

struct tag_tegra {
	__u32 bootarg_key;
	__u32 bootarg_len;
	char bootarg[1];
};

static int __init parse_tag_nvidia(const struct tag *tag)
{

	return 0;
}
__tagtable(ATAG_NVIDIA, parse_tag_nvidia);

static unsigned long ramconsole_start = SZ_512M - SZ_1M;
static unsigned long ramconsole_size = SZ_1M;

static struct plat_serial8250_port debug_uartb_platform_data[] = {
	{
		.membase	= IO_ADDRESS(TEGRA_UARTB_BASE),
		.mapbase	= TEGRA_UARTB_BASE,
		.irq		= INT_UARTB,
		.flags		= UPF_BOOT_AUTOCONF | UPF_FIXED_TYPE,
		.type           = PORT_TEGRA,
		.iotype		= UPIO_MEM,
		.regshift	= 2,
		.uartclk	= 1843200,
	}, {
		.flags		= 0,
	}
};

static struct platform_device debug_uartb = {
	.name = "serial8250",
	.id = PLAT8250_DEV_PLATFORM,
	.dev = {
		.platform_data = debug_uartb_platform_data,
	},
};

static struct platform_device *olympus_uart_devices[] __initdata = {
	&tegra_uarta_device,
	&tegra_uartb_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
};

struct uart_clk_parent uart_parent_clk[] = {
	[0] = {.name = "pll_p"},
	[1] = {.name = "pll_c"},
	[2] = {.name = "pll_m"},
	[3] = {.name = "clk_m"},
};

static struct tegra_uart_platform_data olympus_uart_pdata;

static void __init uart_debug_init(void)
{
	unsigned long rate;
	struct clk *debug_uart_clk;
	struct clk *c;

	/* UARTB is the debug port. */
	pr_info("Selecting UARTB as the debug console\n");
	olympus_uart_devices[1] = &debug_uartb;
	debug_uart_clk = clk_get_sys("tegra_uart.1", "uartb");

	/* Clock enable for the debug channel */
	if (!IS_ERR_OR_NULL(debug_uart_clk)) {
		rate = debug_uartb_platform_data[0].uartclk;
		pr_info("The debug console clock name is %s\n",
					debug_uart_clk->name);
		c = tegra_get_clock_by_name("pll_c");
		if (IS_ERR_OR_NULL(c))
			pr_err("Not getting the parent clock pll_c\n");
		else
			clk_set_parent(debug_uart_clk, c);
			clk_enable(debug_uart_clk);
		clk_set_rate(debug_uart_clk, 115200*16);
	} else {
		pr_err("Not getting the clock %s for debug console\n",
					debug_uart_clk->name);
	}
}

static void __init olympus_uart_init(void)
{
	int i;
	struct clk *c;

	for (i = 0; i < ARRAY_SIZE(uart_parent_clk); ++i) {
		c = tegra_get_clock_by_name(uart_parent_clk[i].name);
		if (IS_ERR_OR_NULL(c)) {
			pr_err("Not able to get the clock for %s\n",
						uart_parent_clk[i].name);
			continue;
		}
		uart_parent_clk[i].parent_clk = c;
		uart_parent_clk[i].fixed_clk_rate = clk_get_rate(c);
	}
	olympus_uart_pdata.parent_clk_list = uart_parent_clk;
	olympus_uart_pdata.parent_clk_count = ARRAY_SIZE(uart_parent_clk);

	tegra_uarta_device.dev.platform_data = &olympus_uart_pdata;
	tegra_uartb_device.dev.platform_data = &olympus_uart_pdata;
	tegra_uartc_device.dev.platform_data = &olympus_uart_pdata;

	/* Register low speed only if it is selected */
	if (!is_tegra_debug_uartport_hs())
		uart_debug_init();

	platform_add_devices(olympus_uart_devices,
				ARRAY_SIZE(olympus_uart_devices));
}

static struct resource mdm6600_resources[] = {
	[0] = {
		.flags = IORESOURCE_IRQ,
		.start = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PQ6),
		.end   = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PQ6),
	},
};

static struct platform_device mdm6600_modem = {
	.name = "mdm6600_modem",
	.id   = -1,
	.resource = mdm6600_resources,
	.num_resources = ARRAY_SIZE(mdm6600_resources),
};

/* OTG gadget device */
static struct tegra_utmip_config udc_phy_config = {
	.hssync_start_delay = 9,
	.idle_wait_delay = 17,
	.elastic_limit = 16,
	.term_range_adj = 6,
	.xcvr_setup = 15,
	.xcvr_lsfslew = 1,
	.xcvr_lsrslew = 1,
};

static struct fsl_usb2_platform_data tegra_udc_pdata = {
	.operating_mode	= FSL_USB2_DR_DEVICE,
	.phy_mode	= FSL_USB2_PHY_UTMI,
	.phy_config	= &udc_phy_config,
};

/* OTG transceiver */
static struct resource cpcap_otg_resources[] = {
	[0] = {
		.start  = TEGRA_USB_BASE,
		.end    = TEGRA_USB_BASE + TEGRA_USB_SIZE - 1,
		.flags  = IORESOURCE_MEM,
	},
};

static struct platform_device cpcap_otg = {
	.name = "cpcap-otg",
	.id   = -1,
	.resource = cpcap_otg_resources,
	.num_resources = ARRAY_SIZE(cpcap_otg_resources),
	.dev = {
		.platform_data = &tegra_ehci1_device,
	},
};

static struct cpcap_audio_state stingray_cpcap_audio_state = {
	.cpcap                   = NULL,
	.mode                    = CPCAP_AUDIO_MODE_NORMAL,
	.codec_mode              = CPCAP_AUDIO_CODEC_OFF,
	.codec_rate              = CPCAP_AUDIO_CODEC_RATE_8000_HZ,
	.codec_mute              = CPCAP_AUDIO_CODEC_MUTE,
	.stdac_mode              = CPCAP_AUDIO_STDAC_OFF,
	.stdac_rate              = CPCAP_AUDIO_STDAC_RATE_44100_HZ,
	.stdac_mute              = CPCAP_AUDIO_STDAC_MUTE,
	.analog_source           = CPCAP_AUDIO_ANALOG_SOURCE_OFF,
	.codec_primary_speaker   = CPCAP_AUDIO_OUT_NONE,
	.codec_secondary_speaker = CPCAP_AUDIO_OUT_NONE,
	.stdac_primary_speaker   = CPCAP_AUDIO_OUT_NONE,
	.stdac_secondary_speaker = CPCAP_AUDIO_OUT_NONE,
	.ext_primary_speaker     = CPCAP_AUDIO_OUT_NONE,
	.ext_secondary_speaker   = CPCAP_AUDIO_OUT_NONE,
	.codec_primary_balance   = CPCAP_AUDIO_BALANCE_NEUTRAL,
	.stdac_primary_balance   = CPCAP_AUDIO_BALANCE_NEUTRAL,
	.ext_primary_balance     = CPCAP_AUDIO_BALANCE_NEUTRAL,
	.output_gain             = 7,
	.microphone              = CPCAP_AUDIO_IN_NONE,
	.input_gain              = 31,
	.rat_type                = CPCAP_AUDIO_RAT_NONE
};

/* CPCAP is i2s master; tegra_audio_pdata.master == false */
static void init_dac2(bool bluetooth);
static struct cpcap_audio_platform_data cpcap_audio_pdata = {
	.master = true,
	.regulator = "vaudio",
	.state = &stingray_cpcap_audio_state,
	.speaker_gpio = TEGRA_GPIO_PR3,
	.headset_gpio = -1,
	.spdif_gpio = TEGRA_GPIO_PD4,
	.bluetooth_bypass = init_dac2,
};

static struct platform_device cpcap_audio_device = {
	.name   = "cpcap_audio",
	.id     = -1,
	.dev    = {
		.platform_data = &cpcap_audio_pdata,
	},
};

static struct resource oob_wake_resources[] = {
	[0] = {
		.flags = IORESOURCE_IRQ,
		.start = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PW3),
		.end   = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PW3),
	},
};
/* LTE USB Out-of-Band Wakeup Device */
static struct oob_wake_platform_data oob_wake_pdata = {
	.vendor = 0x22b8,
	.product = 0x4267,
};

static struct platform_device oob_wake_device = {
	.name   = "oob-wake",
	.id     = -1,
	.dev    = {
		.platform_data = &oob_wake_pdata,
	},
	.resource = oob_wake_resources,
	.num_resources = ARRAY_SIZE(oob_wake_resources),
};

/* This is the CPCAP Stereo DAC interface. */
static struct tegra_audio_platform_data tegra_audio_pdata = {
	.i2s_master	= false, /* CPCAP Stereo DAC */
	.dsp_master	= false, /* Don't care */
	.dma_on		= true,  /* use dma by default */
	.i2s_clk_rate	= 24000000,
	.dap_clk	= "cdev1",
	.audio_sync_clk = "audio_2x",
	.mode		= I2S_BIT_FORMAT_I2S,
	.fifo_fmt	= I2S_FIFO_PACKED,
	.bit_size	= I2S_BIT_SIZE_16,
	.i2s_bus_width = 32, /* Using Packed 16 bit data, the dma is 32 bit. */
	.dsp_bus_width = 16, /* When using DSP mode (unused), this should be 16 bit. */
	.mask		= TEGRA_AUDIO_ENABLE_TX,
};

/* Connected to CPCAP CODEC - Switchable to Bluetooth Audio. */
static struct tegra_audio_platform_data tegra_audio2_pdata = {
	.i2s_master	= false, /* CPCAP CODEC */
	.dsp_master	= true,  /* Bluetooth */
	.dsp_master_clk = 8000,  /* Bluetooth audio speed */
	.dma_on		= true,  /* use dma by default */
	.i2s_clk_rate	= 2000000, /* BCM4329 max bitclock is 2048000 Hz */
	.dap_clk	= "cdev1",
	.audio_sync_clk = "audio_2x",
	.mode		= I2S_BIT_FORMAT_DSP, /* Using COCEC in network mode */
	.fifo_fmt	= I2S_FIFO_16_LSB,
	.bit_size	= I2S_BIT_SIZE_16,
	.i2s_bus_width = 16, /* Capturing a single timeslot, mono 16 bits */
	.dsp_bus_width = 16,
	.mask		= TEGRA_AUDIO_ENABLE_TX | TEGRA_AUDIO_ENABLE_RX,
};

static struct tegra_audio_platform_data tegra_spdif_pdata = {
	.dma_on		= true,  /* use dma by default */
	.i2s_clk_rate	= 5644800,
	.mode		= SPDIF_BIT_MODE_MODE16BIT,
	.fifo_fmt	= 1,
};

static struct tegra_utmip_config utmi_phy_config[] = {
	[0] = {
		.hssync_start_delay = 9,
		.idle_wait_delay = 17,
		.elastic_limit = 16,
		.term_range_adj = 6,
		.xcvr_setup = 15,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
	},
	[1] = {
		.hssync_start_delay = 0,
		.idle_wait_delay = 17,
		.elastic_limit = 16,
		.term_range_adj = 6,
		.xcvr_setup = 8,
		.xcvr_lsfslew = 2,
		.xcvr_lsrslew = 2,
	},
};

static struct tegra_ulpi_config ulpi_phy_config = {
	.reset_gpio = TEGRA_GPIO_PG2,
	.clk = "cdev2",
};

static struct tegra_ehci_platform_data tegra_ehci_pdata[] = {
	[0] = {
		.phy_config = &utmi_phy_config[0],
		.operating_mode = TEGRA_USB_OTG,
		.power_down_on_bus_suspend = 0,
	},
	[1] = {
		.phy_config = &ulpi_phy_config,
		.operating_mode = TEGRA_USB_HOST,
		.power_down_on_bus_suspend = 1,
	},
	[2] = {
		.phy_config = &utmi_phy_config[1],
		.operating_mode = TEGRA_USB_HOST,
		.power_down_on_bus_suspend = 1,
	},
};

/* bq24617 charger */
static struct resource bq24617_resources[] = {
	[0] = {
		.name  = "stat1",
		.flags = IORESOURCE_IRQ,
		.start = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV5),
		.end   = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV5),
	},
	[1] = {
		.name  = "stat2",
		.flags = IORESOURCE_IRQ,
		.start = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PD1),
		.end   = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PD1),
	},
	[2] = {
		.name  = "detect",
		.flags = IORESOURCE_IRQ,
		.start = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
		.end   = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
	},
};

static struct resource bq24617_resources_m1_p0[] = {
	[0] = {
		.name  = "stat1",
		.flags = IORESOURCE_IRQ,
		.start = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV5),
		.end   = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV5),
	},
	[1] = {
		.name  = "stat2",
		.flags = IORESOURCE_IRQ,
		.start = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
		.end   = TEGRA_GPIO_TO_IRQ(TEGRA_GPIO_PV6),
	},
};

static struct platform_device bq24617_device = {
	.name		= "bq24617",
	.id		= -1,
	.resource       = bq24617_resources,
	.num_resources  = ARRAY_SIZE(bq24617_resources),
};

static struct resource tegra_gart_resources[] = {
    {
	.name = "mc",
	.flags = IORESOURCE_MEM,
	.start = TEGRA_MC_BASE,
	.end = TEGRA_MC_BASE + TEGRA_MC_SIZE - 1,
    },
    {
	.name = "gart",
	.flags = IORESOURCE_MEM,
	.start = 0x58000000,
	.end = 0x58000000 - 1 + 32 * 1024 * 1024,
    }
};


static struct platform_device tegra_gart_dev = {
    .name = "tegra_gart",
    .id = -1,
    .num_resources = ARRAY_SIZE(tegra_gart_resources),
    .resource = tegra_gart_resources
};

static struct platform_device bcm4329_bluetooth_device = {
	.name = "bcm4329_bluetooth",
	.id = -1,
};

static struct platform_device tegra_camera = {
	.name = "tegra_camera",
	.id = -1,
};

static struct tegra_w1_timings tegra_w1_platform_timings = {
	.tsu = 0x1,
	.trelease = 0xf,
	.trdv = 0xf,
	.tlow0 = 0x3c,
	.tlow1 = 0x1,
	.tslot = 0x78,

	.tpdl = 0x3c,
	.tpdh = 0x1e,
	.trstl = 0x1ea,
	.trsth = 0x1df,

	.rdsclk = 0x7,
	.psclk = 0x50,
};

static struct tegra_w1_platform_data tegra_w1_pdata = {
	.clk_id = NULL,
	.timings = &tegra_w1_platform_timings,
};

static struct resource ram_console_resources[] = {
	{
		/* .start and .end filled in later */
		.flags  = IORESOURCE_MEM,
	},
};

static struct ram_console_platform_data ram_console_pdata;

static struct platform_device ram_console_device = {
	.name           = "ram_console",
	.id             = -1,
	.num_resources  = ARRAY_SIZE(ram_console_resources),
	.resource       = ram_console_resources,
	.dev    = {
		.platform_data = &ram_console_pdata,
	},
};

static struct nvmap_platform_carveout stingray_carveouts[] = {
	[0] = {
		.name		= "iram",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_IRAM,
		.base		= TEGRA_IRAM_BASE,
		.size		= TEGRA_IRAM_SIZE,
		.buddy_size	= 0,
	},
	[1] = {
		.name		= "generic-0",
		.usage_mask	= NVMAP_HEAP_CARVEOUT_GENERIC,
		/* .base and .size to be filled in later */
		.buddy_size	= SZ_32K,
	},
};

static struct nvmap_platform_data stingray_nvmap_data = {
	.carveouts	= stingray_carveouts,
	.nr_carveouts	= ARRAY_SIZE(stingray_carveouts),
};

static int disp_backlight_init(void)
{
    int ret;
    if ((ret = gpio_request(TEGRA_BACKLIGHT_EN_GPIO, "backlight_en"))) {
        pr_err("%s: gpio_request(%d, backlight_en) failed: %d\n",
            __func__, TEGRA_BACKLIGHT_EN_GPIO, ret);
        return ret;
    } else {
        pr_info("%s: gpio_request(%d, backlight_en) success!\n",
            __func__, TEGRA_BACKLIGHT_EN_GPIO);
    }
    if ((ret = gpio_direction_output(TEGRA_BACKLIGHT_EN_GPIO, 1))) {
        pr_err("%s: gpio_direction_output(backlight_en) failed: %d\n",
            __func__, ret);
        return ret;
    }
    if ((ret = gpio_request(TEGRA_KEY_BACKLIGHT_EN_GPIO,
				"key_backlight_en"))) {
			pr_err("%s: gpio_request(%d, key_backlight_en) failed: %d\n",
				__func__, TEGRA_KEY_BACKLIGHT_EN_GPIO, ret);
			return ret;
    } else {
			pr_info("%s: gpio_request(%d, key_backlight_en) success!\n",
				__func__, TEGRA_KEY_BACKLIGHT_EN_GPIO);
		}
    if ((ret = gpio_direction_output(TEGRA_KEY_BACKLIGHT_EN_GPIO, 1))) {
			pr_err("%s: gpio_direction_output(key_backlight_en) failed: %d\n",
				__func__, ret);
			return ret;
		}

    return 0;
}

static int disp_backlight_power_on(void)
{
    pr_info("%s: display backlight is powered on\n", __func__);
    gpio_set_value(TEGRA_BACKLIGHT_EN_GPIO, 1);
    return 0;
}

static int disp_backlight_power_off(void)
{
    pr_info("%s: display backlight is powered off\n", __func__);
    gpio_set_value(TEGRA_BACKLIGHT_EN_GPIO, 0);
    return 0;
}

struct lm3532_platform_data lm3532_pdata = {
    .flags = LM3532_CONFIG_BUTTON_BL | LM3532_HAS_WEBTOP | LM3532_DISP_BTN_TIED,
    .init = disp_backlight_init,
    .power_on = disp_backlight_power_on,
    .power_off = disp_backlight_power_off,

    .ramp_time = 0,   /* Ramp time in milliseconds */
    .ctrl_a_fs_current = LM3532_26p6mA_FS_CURRENT,
    .ctrl_b_fs_current = LM3532_8p2mA_FS_CURRENT,
    .ctrl_a_mapping_mode = LM3532_LINEAR_MAPPING,
    .ctrl_b_mapping_mode = LM3532_LINEAR_MAPPING,
	.ctrl_a_pwm = 0x82,
};

static struct platform_device stingray_nvmap_device = {
	.name	= "tegra-nvmap",
	.id	= -1,
	.dev	= {
		.platform_data = &stingray_nvmap_data,
	},
};

static struct tegra_hsuart_platform_data tegra_uartc_pdata = {
	.exit_lpm_cb	= bcm_bt_lpm_exit_lpm_locked,
	.rx_done_cb	= bcm_bt_rx_done_locked,
};

static struct platform_device *olympus_devices[] __initdata = {
	&cpcap_otg,
	/*&bq24617_device,*/
	&bcm4329_bluetooth_device,
	&tegra_uarta_device,
	&tegra_uartc_device,
	&tegra_uartd_device,
	&tegra_uarte_device,
	&tegra_spi_device1,
	&tegra_spi_device2,
	&tegra_spi_device3,
	&tegra_spi_device4,
	&tegra_gart_dev,
	&stingray_nvmap_device,
	&tegra_grhost_device,
	&ram_console_device,
	&tegra_camera,
	&tegra_i2s_device1,
	&tegra_i2s_device2,
	&mdm6600_modem,
	&tegra_spdif_device,
	&tegra_avp_device,
	&tegra_pmu_device,
	&tegra_aes_device,
	&tegra_wdt_device,
	&oob_wake_device,
};

extern struct tegra_sdhci_platform_data olympus_wifi_data; /* sdhci2 */

static struct tegra_sdhci_platform_data stingray_sdhci_sdcard_platform_data = {
	/*.clk_id = NULL,
	.force_hs = 0,*/
	.cd_gpio = TEGRA_GPIO_PI5,
	.wp_gpio = -1,
	.power_gpio = -1,
};

static struct tegra_sdhci_platform_data stingray_sdhci_platform_data4 = {
	/*.clk_id = NULL,
	.force_hs = 0,*/
	.cd_gpio = -1,
	.wp_gpio = -1,
	.power_gpio = -1,
	.is_8bit = 1,
};

/*static const struct tegra_pingroup_config i2c1_ddc = {
	.pingroup	= TEGRA_PINGROUP_I2CP,
	.func		= TEGRA_MUX_I2C,
};

static const struct tegra_pingroup_config i2c1_gen = {
	.pingroup	= TEGRA_PINGROUP_RM,
	.func		= TEGRA_MUX_I2C,
};*/

static struct tegra_i2c_platform_data olympus_i2c1_platform_data = {
	.adapter_nr	= 0,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
/*	.bus_mux	= { &i2c1_ddc, &i2c1_gen }, */
	.scl_gpio		= {TEGRA_GPIO_PC4, 0},
	.sda_gpio		= {TEGRA_GPIO_PC5, 0},
};

static const struct tegra_pingroup_config i2c2_ddc = {
	.pingroup	= TEGRA_PINGROUP_DDC,
	.func		= TEGRA_MUX_I2C2,
};

static const struct tegra_pingroup_config i2c2_gen2 = {
	.pingroup	= TEGRA_PINGROUP_PTA,
	.func		= TEGRA_MUX_I2C2,
};

static struct tegra_i2c_platform_data olympus_i2c2_platform_data = {
	.adapter_nr	= 1,
	.bus_count	= 2,
	.bus_clk_rate	= { 400000, 400000 },
	.bus_mux	= { &i2c2_ddc, &i2c2_gen2 },
	.bus_mux_len	= { 1, 1 },
	.scl_gpio		= {0, TEGRA_GPIO_PT5},
	.sda_gpio		= {0, TEGRA_GPIO_PT6},
};

static struct tegra_i2c_platform_data olympus_i2c3_platform_data = {
	.adapter_nr	= 3,
	.bus_count	= 1,
	.bus_clk_rate	= { 400000, 0 },
/*	.scl_gpio		= {TEGRA_GPIO_PBB2, 0},
	.sda_gpio		= {TEGRA_GPIO_PBB3, 0},*/
};

static struct tegra_i2c_platform_data olympus_dvc_platform_data = {
	.adapter_nr	= 4,
	.bus_count	= 1,
	.bus_clk_rate	= { 100000, 0 },
	.is_dvc		= true,
/*	.scl_gpio		= {TEGRA_GPIO_PZ6, 0},
	.sda_gpio		= {TEGRA_GPIO_PZ7, 0},*/
};

static __initdata struct tegra_clk_init_table olympus_clk_init_table[] = {
	/* name		parent		rate		enabled */
	{ "uarta",	"pll_p",	216000000,	true},
	{ "uartb",	"pll_c",	216000000,	true},
	{ "uartc",	"pll_m",	216000000,	true},
	{ "uartd",	"clk_m",	216000000,	true},
	/*{ "emc",	"pll_p",	0,		true},
	{ "emc",	"pll_m",	600000000,	false},*/
	{ "pll_m",	NULL,		600000000,	true},
	{ "mpe",	"pll_m",	250000000,	false},
	{ "pll_a",	NULL,		56448000,	false},
	{ "pll_a_out0",	NULL,		11289600,	false},
	{ "i2s1",	"pll_p",	24000000,	false},
	{ "i2s2",	"pll_p",	2000000,	false},
	{ "sdmmc2",	"pll_m",	48000000,	false},
	{ "spdif_out",	"pll_a_out0",	5644800,	false},
	{ "sdmmc3",	"pll_m",	48000000,	false},
	{ NULL,		NULL,		0,		0},
};

static int isl29030_getIrqStatus(void)
{
	int	status = -1;

	status = gpio_get_value(TEGRA_PROX_INT_GPIO);
	return status;
}

struct isl29030_platform_data isl29030_als_ir_data_Olympus = {
/*
	NOTE: Original values
	.configure = 0x6c,
	.interrupt_cntrl = 0x40,
	.prox_lower_threshold = 0x1e,
	.prox_higher_threshold = 0x32,
	.als_ir_low_threshold = 0x00,
	.als_ir_high_low_threshold = 0x00,
	.als_ir_high_threshold = 0x45,
	.lens_percent_t = 100,
*/
	.init = NULL,
	.exit = NULL,
	.power_on = NULL,
	.power_off = NULL,
	.configure = 0x66,
	.interrupt_cntrl = 0x20,
	.prox_lower_threshold = 0x0A,
	.prox_higher_threshold = 0x14,
	.crosstalk_vs_covered_threshold = 0xB4,
	.default_prox_noise_floor = 0x96,
	.num_samples_for_noise_floor = 0x05,
	.lens_percent_t = 10,
	.irq = 0,
	.getIrqStatus = isl29030_getIrqStatus,
	.gpio_intr = TEGRA_PROX_INT_GPIO,
};

/*
 * Accelerometer
 */

struct kxtf9_platform_data kxtf9_data = {

	.min_interval	= 2,
	.poll_interval	= 200,

	.g_range	= KXTF9_G_8G,

	.axis_map_x	= 0,
	.axis_map_y	= 1,
	.axis_map_z	= 2,

	.negate_x	= 1,
	.negate_y	= 1,
	.negate_z	= 0,

	.data_odr_init		= ODR25,
	.ctrl_reg1_init		= RES_12BIT | KXTF9_G_2G | TPE | WUFE | TDTE,
	.int_ctrl_init		= IEA | IEN,
	.tilt_timer_init	= 0x03,
	.engine_odr_init	= OTP12_5 | OWUF50 | OTDT400,
	.wuf_timer_init		= 0x0A,
	.wuf_thresh_init	= 0x20,
	.tdt_timer_init		= 0x78,
	.tdt_h_thresh_init	= 0xB6,
	.tdt_l_thresh_init	= 0x1A,
	.tdt_tap_timer_init	= 0xA2,
	.tdt_total_timer_init	= 0x24,
	.tdt_latency_timer_init	= 0x28,
	.tdt_window_timer_init	= 0xA0,

	.gpio = TEGRA_KXTF9_INT_GPIO,
	.gesture = 0,
	.sensitivity_low = {
		  0x50, 0xFF, 0xFF, 0x32, 0x09, 0x0A, 0xA0,
	},
	.sensitivity_medium = {
		  0x50, 0xFF, 0x68, 0xA3, 0x09, 0x0A, 0xA0,
	},
	.sensitivity_high = {
		  0x78, 0xB6, 0x1A, 0xA2, 0x24, 0x28, 0xA0,
	},
};

static struct i2c_board_info __initdata olympus_i2c_bus1_board_info[] = {
	{ /* Display backlight */
		I2C_BOARD_INFO(LM3532_NAME, LM3532_I2C_ADDR),
		.platform_data = &lm3532_pdata,
		/*.irq = ..., */
	},
	{
		I2C_BOARD_INFO(QTOUCH_TS_NAME, XMEGAT_I2C_ADDR),
		.irq = TEGRA_GPIO_TO_IRQ(OLYMPUS_TOUCH_INT_N_GPIO),
	},
		{
		/*  ISL 29030 (prox/ALS) driver */
		I2C_BOARD_INFO(LD_ISL29030_NAME, 0x44),
		.platform_data = &isl29030_als_ir_data_Olympus,
		.irq =  TEGRA_GPIO_TO_IRQ(TEGRA_PROX_INT_GPIO),
	},
};

static struct i2c_board_info __initdata olympus_i2c_bus4_board_info[] = {
        {
                I2C_BOARD_INFO("akm8973", 0x0C),
                /*.irq = TEGRA_GPIO_TO_IRQ(OLYMPUS_COMPASS_IRQ_GPIO),*/
        },
	{
		I2C_BOARD_INFO("kxtf9", 0x0F),
		.platform_data = &kxtf9_data,
	},
};

static void olympus_i2c_init(void)
{
	i2c_register_board_info(0, olympus_i2c_bus1_board_info, 1);
	i2c_register_board_info(3, olympus_i2c_bus4_board_info, 1);

	tegra_i2c_device1.dev.platform_data = &olympus_i2c1_platform_data;
	tegra_i2c_device2.dev.platform_data = &olympus_i2c2_platform_data;
	tegra_i2c_device3.dev.platform_data = &olympus_i2c3_platform_data;
	tegra_i2c_device4.dev.platform_data = &olympus_dvc_platform_data;

	platform_device_register(&tegra_i2c_device1);
	platform_device_register(&tegra_i2c_device2);
	platform_device_register(&tegra_i2c_device3);
	platform_device_register(&tegra_i2c_device4);
}

static void olympus_sdhci_init(void)
{
	/* TODO: setup GPIOs for cd, wd, and power */
	tegra_sdhci_device2.dev.platform_data = &olympus_wifi_data;
	tegra_sdhci_device3.dev.platform_data = &stingray_sdhci_sdcard_platform_data;
	tegra_sdhci_device4.dev.platform_data = &stingray_sdhci_platform_data4;

	platform_device_register(&tegra_sdhci_device2);
	platform_device_register(&tegra_sdhci_device3);
	platform_device_register(&tegra_sdhci_device4);
}
#define ATAG_BDADDR 0x43294329	/* stingray bluetooth address tag */
#define ATAG_BDADDR_SIZE 4
#define BDADDR_STR_SIZE 18

static char bdaddr[BDADDR_STR_SIZE];

module_param_string(bdaddr, bdaddr, sizeof(bdaddr), 0400);
MODULE_PARM_DESC(bdaddr, "bluetooth address");

static int __init parse_tag_bdaddr(const struct tag *tag)
{
	unsigned char *b = (unsigned char *)&tag->u;

	if (tag->hdr.size != ATAG_BDADDR_SIZE)
		return -EINVAL;

	snprintf(bdaddr, BDADDR_STR_SIZE, "%02X:%02X:%02X:%02X:%02X:%02X",
			b[0], b[1], b[2], b[3], b[4], b[5]);

	return 0;
}
__tagtable(ATAG_BDADDR, parse_tag_bdaddr);

static DEFINE_SPINLOCK(brcm_4329_enable_lock);
static int brcm_4329_enable_count;

static void olympus_w1_init(void)
{
	tegra_w1_device.dev.platform_data = &tegra_w1_pdata;
	platform_device_register(&tegra_w1_device);
}

/* powerup reason */
#define ATAG_POWERUP_REASON		 0xf1000401
#define ATAG_POWERUP_REASON_SIZE 3 /* size + tag id + tag data */

static unsigned int powerup_reason = PU_REASON_PWR_KEY_PRESS;

unsigned int olympus_powerup_reason (void)
{
	return powerup_reason;
}

static int __init parse_tag_powerup_reason(const struct tag *tag)
{
	if (tag->hdr.size != ATAG_POWERUP_REASON_SIZE)
		return -EINVAL;
	memcpy(&powerup_reason, &tag->u, sizeof(powerup_reason));
	printk(KERN_INFO "powerup reason=0x%08x\n", powerup_reason);
	return 0;
}
__tagtable(ATAG_POWERUP_REASON, parse_tag_powerup_reason);

#define BOOT_MODE_MAX_LEN 30
static char boot_mode[BOOT_MODE_MAX_LEN + 1];
int __init board_boot_mode_init(char *s)
{
	strncpy(boot_mode, s, BOOT_MODE_MAX_LEN);
	boot_mode[BOOT_MODE_MAX_LEN] = '\0';
	printk(KERN_INFO "boot_mode=%s\n", boot_mode);
	return 1;
}
__setup("androidboot.mode=", board_boot_mode_init);

#define SERIAL_NUMBER_LENGTH 16
static char usb_serial_num[SERIAL_NUMBER_LENGTH + 1];
static int __init olympus_usb_serial_num_setup(char *options)
{
	strncpy(usb_serial_num, options, SERIAL_NUMBER_LENGTH);
	usb_serial_num[SERIAL_NUMBER_LENGTH] = '\0';
	printk(KERN_INFO "usb_serial_num=%s\n", usb_serial_num);
	return 1;
}
__setup("androidboot.serialno=", olympus_usb_serial_num_setup);

static int olympus_boot_recovery = 0;
static int __init olympus_bm_recovery_setup(char *options)
{
       olympus_boot_recovery = 1;
       return 1;
}
__setup("rec", olympus_bm_recovery_setup);


static void stingray_usb_init(void)
{
	int factorycable = !strncmp(boot_mode, "factorycable",
			BOOT_MODE_MAX_LEN);

	tegra_udc_device.dev.platform_data = &tegra_udc_pdata;
	tegra_ehci2_device.dev.platform_data = &tegra_ehci_pdata[1];
	tegra_ehci3_device.dev.platform_data = &tegra_ehci_pdata[2];

	if (!(factorycable && olympus_boot_recovery))
		platform_device_register(&tegra_udc_device);

	platform_device_register(&tegra_ehci3_device);
}

static void olympus_reset(char mode, const char *cmd)
{
	/* STI-OLY need to find correct GPIOs */
	/* Signal to CPCAP to stop the uC. */
	/* gpio_set_value(TEGRA_GPIO_PG3, 0);
	mdelay(100);
	gpio_set_value(TEGRA_GPIO_PG3, 1);
	mdelay(100);*/

	tegra_assert_system_reset(mode, cmd);
}

static void olympus_power_off(void)
{
	printk(KERN_INFO "olympus_pm_power_off...\n");

	local_irq_disable();

	/* signal WDI gpio to shutdown CPCAP, which will
	   cascade to all of the regulators. */
	gpio_direction_output(TEGRA_GPIO_PV7, 0);
	gpio_set_value(TEGRA_GPIO_PV7, 0);

	do {} while (1);

	local_irq_enable();
}

static void __init olympus_power_off_init(void)
{
	tegra_gpio_enable(TEGRA_GPIO_PZ2);
	gpio_request(TEGRA_GPIO_PZ2, "sys_restart_b");
	gpio_direction_output(TEGRA_GPIO_PZ2, 0);
	arch_reset = olympus_reset;

	tegra_gpio_enable(TEGRA_GPIO_PV7);
	if (!gpio_request(TEGRA_GPIO_PV7, "wdi"))
		pm_power_off = olympus_power_off;
}

static unsigned int olympus_board_revision = OLYMPUS_REVISION_UNKNOWN;

unsigned int olympus_revision(void)
{
	return olympus_board_revision;
}

static int __init olympus_revision_parse(char *options)
{
	if (!strcmp(options, "0"))
		olympus_board_revision = OLYMPUS_REVISION_0;
	else if (!strcmp(options, "1"))
		olympus_board_revision = OLYMPUS_REVISION_1;
	else if (!strcmp(options, "1B"))
		olympus_board_revision = OLYMPUS_REVISION_1B;
	else if (!strcmp(options, "1C"))
		olympus_board_revision = OLYMPUS_REVISION_1C;
	else if (!strcmp(options, "2"))
		olympus_board_revision = OLYMPUS_REVISION_2;
	else if (!strcmp(options, "2B"))
		olympus_board_revision = OLYMPUS_REVISION_2B;
	else if (!strcmp(options, "2C"))
		olympus_board_revision = OLYMPUS_REVISION_2C;
	else if (!strcmp(options, "3"))
		olympus_board_revision = OLYMPUS_REVISION_3;
	else if (!strcmp(options, "3B"))
		olympus_board_revision = OLYMPUS_REVISION_3B;
	else if (!strcmp(options, "4A"))
		olympus_board_revision = OLYMPUS_REVISION_4A;
	else if (!strcmp(options, "4B"))
		olympus_board_revision = OLYMPUS_REVISION_4B;
	else if (!strcmp(options, "4F0"))
		olympus_board_revision = OLYMPUS_REVISION_4F0;
	else if (!strcmp(options, "4FB"))
		olympus_board_revision = OLYMPUS_REVISION_4FB;
	else
		olympus_board_revision = system_rev;

	printk(KERN_INFO "hw_rev=0x%x\n", olympus_board_revision);

	return 1;
}
__setup("hw_rev=", olympus_revision_parse);

int olympus_qbp_usb_hw_bypass_enabled(void)
{
	/* We could use the boot_mode string instead of probing the HW, but
	 * that would not work if we enable run-time switching to this mode
	 * in the future.
	 */
	if (gpio_get_value(TEGRA_GPIO_PT3) && !gpio_get_value(TEGRA_GPIO_PV4)) {
		pr_info("olympus_qbp_usb_hw_bypass enabled\n");
		return 1;
	}
	return 0;
}

static struct tegra_suspend_platform_data olympus_suspend = {
	.cpu_timer = 1500,
	.cpu_off_timer = 1,
	.core_timer = 0x7e7e,
	.core_off_timer = 0xf,
        .corereq_high = true,
	.sysclkreq_high = true,
	.suspend_mode = TEGRA_SUSPEND_LP0,
};

static void *das_base = IO_ADDRESS(TEGRA_APB_MISC_BASE);

static inline void das_writel(unsigned long value, unsigned long offset)
{
	writel(value, das_base + offset);
}

#define APB_MISC_DAS_DAP_CTRL_SEL_0             0xc00
#define APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0   0xc40

static void init_dac1(void)
{
	bool master = tegra_audio_pdata.i2s_master;
	/* DAC1 -> DAP1 */
	das_writel((!master)<<31, APB_MISC_DAS_DAP_CTRL_SEL_0);
	das_writel(0, APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0);
}

static void init_dac2(bool bluetooth)
{
	if (!bluetooth) {
		/* DAC2 -> DAP2 for CPCAP CODEC */
		bool master = tegra_audio2_pdata.i2s_master;
		das_writel((!master)<<31 | 1, APB_MISC_DAS_DAP_CTRL_SEL_0 + 4);
		das_writel(1<<28 | 1<<24 | 1,
				APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0 + 4);
	} else {
		/* DAC2 -> DAP4 for Bluetooth Voice */
		bool master = tegra_audio2_pdata.dsp_master;
		das_writel((!master)<<31 | 1, APB_MISC_DAS_DAP_CTRL_SEL_0 + 12);
		das_writel(3<<28 | 3<<24 | 3,
				APB_MISC_DAS_DAC_INPUT_DATA_CLK_SEL_0 + 4);
	}
}

void change_power_brcm_4329(bool enable) {
	unsigned long flags;

	spin_lock_irqsave(&brcm_4329_enable_lock, flags);
	if (enable) {
		gpio_set_value(TEGRA_GPIO_PU4, enable);
		brcm_4329_enable_count++;
		// The following shouldn't happen but protect
		// if the user doesn't cleanup.
		if (brcm_4329_enable_count > 2)
			brcm_4329_enable_count = 2;
	} else {
		if (brcm_4329_enable_count > 0)
			brcm_4329_enable_count--;
		if (!brcm_4329_enable_count)
			gpio_set_value(TEGRA_GPIO_PU4, enable);
	}
	spin_unlock_irqrestore(&brcm_4329_enable_lock, flags);
}

/* STI-OLY APANIC_MMC_INIT Start */

static u64 tegra_dma_mask = DMA_BIT_MASK(32);

static struct tegra_sdhci_simple_platform_data tegra_sdhci_simple_platform_data;
static struct platform_device tegra_sdhci_simple_device;

static struct apanic_mmc_platform_data apanic_mmc_platform_data;

static struct platform_device apanic_handle_mmc_platform_device = {
	.name          = "apanic_handle_mmc",
	.id            = 0,
	.dev =
	{
		.platform_data = &apanic_mmc_platform_data,
	}
};

int tegra_sdhci_boot_device = 0;

#define MAX_MTD_PARTNR 16

static struct mtd_partition tegra_mtd_partitions[MAX_MTD_PARTNR];

struct tegra_nand_platform tegra_nand_plat = {
	.parts = tegra_mtd_partitions,
	.nr_parts = 0,
};

static struct tegra_sdhci_platform_data tegra_sdhci_platform[] = {
	[0] = {
		/*.bus_width = 4,
		.debounce = 5,
		.max_power_class = 15,*/
	},
	[1] = {
/*		.bus_width = 4,
		.debounce = 5,
		.max_power_class = 15,*/
	},
	[2] = {
/*		.bus_width = 4,
		.debounce = 5,
		.max_power_class = 15,*/
	},
	[3] = {
/*		.bus_width = 4,
		.debounce = 5,
		.max_power_class = 15,*/
	},
};

static struct resource tegra_sdhci_resources[][2] = {
	[0] = {
		[0] = {
			.start = TEGRA_SDMMC1_BASE,
			.end = TEGRA_SDMMC1_BASE + TEGRA_SDMMC1_SIZE - 1,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = INT_SDMMC1,
			.end = INT_SDMMC1,
			.flags = IORESOURCE_IRQ,
		},
	},
	[1] = {
		[0] = {
			.start = TEGRA_SDMMC2_BASE,
			.end = TEGRA_SDMMC2_BASE + TEGRA_SDMMC2_SIZE - 1,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = INT_SDMMC2,
			.end = INT_SDMMC2,
			.flags = IORESOURCE_IRQ,
		},
	},
	[2] = {
		[0] = {
			.start = TEGRA_SDMMC3_BASE,
			.end = TEGRA_SDMMC3_BASE + TEGRA_SDMMC3_SIZE - 1,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = INT_SDMMC3,
			.end = INT_SDMMC3,
			.flags = IORESOURCE_IRQ,
		},
	},
	[3] = {
		[0] = {
			.start = TEGRA_SDMMC4_BASE,
			.end = TEGRA_SDMMC4_BASE + TEGRA_SDMMC4_SIZE - 1,
			.flags = IORESOURCE_MEM,
		},
		[1] = {
			.start = INT_SDMMC4,
			.end = INT_SDMMC4,
			.flags = IORESOURCE_IRQ,
		},
	},
};

struct platform_device tegra_sdhci_devices[] = {
	[0] = {
		.id = 0,
		.name = "tegra-sdhci",
		.resource = tegra_sdhci_resources[0],
		.num_resources = ARRAY_SIZE(tegra_sdhci_resources[0]),
		.dev = {
			.platform_data = &tegra_sdhci_platform[0],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
	[1] = {
		.id = 1,
		.name = "tegra-sdhci",
		.resource = tegra_sdhci_resources[1],
		.num_resources = ARRAY_SIZE(tegra_sdhci_resources[1]),
		.dev = {
			.platform_data = &tegra_sdhci_platform[1],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
	[2] = {
		.id = 2,
		.name = "tegra-sdhci",
		.resource = tegra_sdhci_resources[2],
		.num_resources = ARRAY_SIZE(tegra_sdhci_resources[2]),
		.dev = {
			.platform_data = &tegra_sdhci_platform[2],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
	[3] = {
		.id = 3,
		.name = "tegra-sdhci",
		.resource = tegra_sdhci_resources[3],
		.num_resources = ARRAY_SIZE(tegra_sdhci_resources[3]),
		.dev = {
			.platform_data = &tegra_sdhci_platform[3],
			.coherent_dma_mask = DMA_BIT_MASK(32),
			.dma_mask = &tegra_dma_mask,
		},
	},
};

int apanic_mmc_init(void)
{
	int i;
	int result = -ENOMEM;

	/*
	 * This is a little convoluted, but the simple driver needs to map the
	 * I/O port and access other resources in order to use the hardware.
	 * It can't do it through the normal means or else the kernel will try
	 * to claim the same resources as the real sdhci-tegra driver at boot.
	 */
	if (tegra_sdhci_boot_device >= 0) {
		tegra_sdhci_simple_platform_data.sdhci_pdata =
			tegra_sdhci_devices[tegra_sdhci_boot_device].dev.platform_data;
		tegra_sdhci_simple_platform_data.resource =
			tegra_sdhci_devices[tegra_sdhci_boot_device].resource;
		tegra_sdhci_simple_platform_data.num_resources =
			tegra_sdhci_devices[tegra_sdhci_boot_device].num_resources;
		tegra_sdhci_simple_platform_data.clk_dev_name =
			kasprintf(GFP_KERNEL, "tegra-sdhci.%d", tegra_sdhci_boot_device);

		tegra_sdhci_simple_device.id = tegra_sdhci_boot_device;
		tegra_sdhci_simple_device.name = "tegra-sdhci-simple";
		tegra_sdhci_simple_device.dev.platform_data =
				&tegra_sdhci_simple_platform_data;

		result = platform_device_register(&tegra_sdhci_simple_device);
	}

	/*
	 * FIXME: There is no way to "discover" the kpanic partition because
	 * it has no file system and the legacy MBR/EBR tables do not support
	 * labels.  GPT promises to address this in K35 or later.
	 */
	if (result == 0) {
		apanic_mmc_platform_data.id = 0;  /* mmc0 - not used by sdhci-tegra-simple */
		for (i = 0; i < tegra_nand_plat.nr_parts; i++) {
			if (strcmp(CONFIG_APANIC_PLABEL, tegra_nand_plat.parts[i].name))
				continue;
			apanic_mmc_platform_data.sector_size = 512;  /* fixme */
			apanic_mmc_platform_data.start_sector =
					tegra_nand_plat.parts[i].offset / 512;
			apanic_mmc_platform_data.sectors =
					tegra_nand_plat.parts[i].size / 512;
			break;
		}

		result = platform_device_register(&apanic_handle_mmc_platform_device);
	}

	return result;
}
/* STI-OLY APANIC_MMC_INIT End */

static void __init tegra_olympus_fixup(struct machine_desc *desc, struct tag *tags,
				 char **cmdline, struct meminfo *mi)
{
	mi->nr_banks = 2;
	mi->bank[0].start = PHYS_OFFSET;
	mi->bank[0].size = 448 * SZ_1M;
	mi->bank[1].start = SZ_512M;
	mi->bank[1].size = SZ_512M;
}

void olympus_set_hsj_mux(short hsj_mux_gpio)
{
	/* set pin M 2 to 1 to route audio onto headset or 0 to route console uart */
 
        gpio_free(TEGRA_GPIO_PM2);
 
        if( gpio_request(TEGRA_GPIO_PM2, "hs_detect_enable") < 0 )
                printk("\n%s: gpio_request error gpio %d  \n",
                                __FUNCTION__,TEGRA_GPIO_PM2);
        else {
                gpio_direction_output(TEGRA_GPIO_PM2, hsj_mux_gpio);
        }

}

static void __init tegra_olympus_init(void)
{	
	struct clk *clk;
	struct resource *res;
	short hsj_mux_gpio;

	/* force consoles to stay enabled across suspend/resume */
	console_suspend_enabled = 0;

	/*STI-OLY for now tegra_init_suspend(&olympus_suspend); */

	/* Olympus has a USB switch that disconnects the usb port from the AP20
	   unless a factory cable is used, the factory jumper is set, or the
	   usb_data_en gpio is set.
	 */
	tegra_gpio_enable(TEGRA_GPIO_PV6);
	gpio_request(TEGRA_GPIO_PV6, "usb_data_en");
	gpio_direction_output(TEGRA_GPIO_PV6, 1);

	tegra_clk_init_from_table(olympus_clk_init_table);
	olympus_pinmux_init();

	olympus_i2c_init();

	olympus_uart_init();

	hsj_mux_gpio=0;
	
	olympus_set_hsj_mux( hsj_mux_gpio );

	apanic_mmc_init();

	olympus_sdhci_init();

	olympus_init_emc();

	platform_add_devices(olympus_devices, ARRAY_SIZE(olympus_devices));

	pr_info("Board HW Revision: system_rev = %d\n", system_rev);



	/*init_dac1();
	init_dac2(false);
	tegra_i2s_device1.dev.platform_data = &tegra_audio_pdata;
	tegra_i2s_device2.dev.platform_data = &tegra_audio2_pdata;
	cpcap_device_register(&cpcap_audio_device);
	tegra_spdif_device.dev.platform_data = &tegra_spdif_pdata;

	tegra_ehci1_device.dev.platform_data = &tegra_ehci_pdata[0];
	tegra_uartc_device.dev.platform_data = &tegra_uartc_pdata;*/

	res = platform_get_resource(&ram_console_device, IORESOURCE_MEM, 0);
	res->start = ramconsole_start;
	res->end = ramconsole_start + ramconsole_size - 1;

	if (readl(IO_ADDRESS(TEGRA_CLK_RESET_BASE)) & BIT(12))
		ram_console_pdata.bootinfo =
			"tegra_wdt: last reset due to watchdog timeout";

	stingray_carveouts[1].base = tegra_carveout_start;
	stingray_carveouts[1].size = tegra_carveout_size;


	olympus_power_init();
	
	olympus_power_off_init();

	stingray_keypad_init();
	olympus_touch_init();
	olympus_w1_init();

	stingray_panel_init();

	/* STI-OLY for now 
	stingray_sensors_init();*/

	olympus_wlan_init();

	/* STI-OLY for now 
	stingray_gps_init();*/
	stingray_usb_init();

}

int __init olympus_protected_aperture_init(void)
{
	tegra_protected_aperture_init(tegra_grhost_aperture);
	memblock_free(tegra_bootloader_fb_start, tegra_bootloader_fb_size);
	return 0;
}

late_initcall(olympus_protected_aperture_init);

void __init olympus_map_io(void)
{
	tegra_map_common_io();
}

static int __init olympus_ramconsole_arg(char *options)
{
	char *p = options;

	ramconsole_size = memparse(p, &p);
	if (*p == '@')
		ramconsole_start = memparse(p+1, &p);

	return 0;
}
early_param("ramconsole", olympus_ramconsole_arg);

void __init olympus_reserve(void)
{
	long ret;
	if (memblock_reserve(0x0, 4096) < 0)
		pr_warn("Cannot reserve first 4K of memory for safety\n");

	ret = memblock_remove(SZ_512M - SZ_2M, SZ_2M);
	if (ret)
		pr_info("Failed to remove ram console\n");
	else
		pr_info("Reserved %08lx@%08lx for ram console\n",
			ramconsole_start, ramconsole_size);

	tegra_reserve(SZ_256M, SZ_8M, SZ_16M);

	if (memblock_reserve(tegra_bootloader_fb_start, tegra_bootloader_fb_size))
		pr_info("Failed to reserve old framebuffer location\n");
	else
		pr_info("HACK: Old framebuffer:  %08lx - %08lx\n",
			tegra_bootloader_fb_start,
			tegra_bootloader_fb_start + tegra_bootloader_fb_size - 1);
}

static void __init mot_fixup(struct machine_desc *desc, struct tag *tags,
                 char **cmdline, struct meminfo *mi)
{
        struct tag *t;
        int i;

        /*
         * Dump some key ATAGs
         */
        for (t=tags; t->hdr.size; t = tag_next(t)) {
                switch (t->hdr.tag) {
                case ATAG_WLAN_MAC:        // 57464d41 parsed in board-mot-wlan.c
                case ATAG_BLDEBUG:         // 41000811 same, in board-mot-misc.c
                case ATAG_POWERUP_REASON:  // F1000401 ex: 0x4000, parsed after... ignore
                        break;
                case ATAG_CORE:     // 54410001
                        printk("%s: atag_core hdr.size=%d\n", __func__, t->hdr.size);
                        break;
                case ATAG_CMDLINE:
                        printk("%s: atag_cmdline=\"%s\"\n", __func__, t->u.cmdline.cmdline);
                        break;
                case ATAG_REVISION: // 54410007
                        printk("%s: atag_revision=0x%x\n", __func__, t->u.revision.rev);
                        break;
                case ATAG_SERIAL:   // 54410006
                        printk("%s: atag_serial=%x%x\n", __func__, t->u.serialnr.low, t->u.serialnr.high);
                        break;
                case ATAG_INITRD2:  // 54420005
                        printk("%s: atag_initrd2=0x%x size=0x%x\n", __func__, t->u.initrd.start, t->u.initrd.size);
                        break;
                case ATAG_MEM:
                        printk("%s: atag_mem.start=0x%x, mem.size=0x%x\n", __func__, t->u.mem.start, t->u.mem.size);
                        break;
#ifdef CONFIG_MACH_MOT
                case ATAG_MOTOROLA: // 41000810
                        printk("%s: atag_moto allow_fb=%d\n", __func__, t->u.motorola.allow_fb_open);
                        break;
#endif
                case ATAG_NVIDIA_TEGRA: // 41000801
                        printk("%s: atag_tegra=0x%X\n", __func__, t->u.tegra.bootarg_key);
                        break;
                default:
                        printk("%s: ATAG %X\n", __func__, t->hdr.tag);
                }
        }

        /*
         * Dump memory nodes
         */
        for (i=0; i<mi->nr_banks; i++) {
                printk("%s: bank[%d]=%lx@%lx\n", __func__, i, mi->bank[i].size, mi->bank[i].start);
        }
}

MACHINE_START(OLYMPUS, "Olympus")
	.boot_params	= 0x00000100,
	.map_io		= olympus_map_io,
	//.reserve	= olympus_reserve,
	//.init_early	= tegra_init_early, 
	//.fixup		= mot_fixup,
	//.init_irq	= tegra_init_irq,
	//.timer		= &tegra_timer,
	//.init_machine	= tegra_olympus_init,
MACHINE_END

/* MACHINE_START(STINGRAY, "stingray")
	.boot_params	= 0x00000100,
	.map_io		= stingray_map_io,
	.reserve	= stingray_reserve,
	.init_early	= tegra_init_early,
	.init_irq	= tegra_init_irq,
	.timer		= &tegra_timer,
	.init_machine	= tegra_stingray_init,
MACHINE_END */

