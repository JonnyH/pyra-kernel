/*
 * Toppoly TD028TTEC1 panel support
 *
 * Copyright (C) 2008 Nokia Corporation
 * Author: Tomi Valkeinen <tomi.valkeinen@nokia.com>
 *
 * Neo 1973 code (jbt6k74.c):
 * Copyright (C) 2006-2007 by OpenMoko, Inc.
 * Author: Harald Welte <laforge@openmoko.org>
 *
 * Ported and adapted from Neo 1973 U-Boot by H. Nikolaus Schaller <hns@goldelico.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <asm/mach-types.h>

//#include <plat/display.h>
#include <video/omapdss.h>
#include <video/omap-panel-tpo-td028ttec1.h>
#include <linux/gpio.h>

static struct omap_video_timings td028ttec1_panel_timings = {
.x_res		= 480,
.y_res		= 640,
.pixel_clock	= 22153,
.hfp		= 24,
.hsw		= 8,
.hbp		= 8,
.vfp		= 4,
.vsw		= 2,
.vbp		= 2,

.vsync_level	= OMAPDSS_SIG_ACTIVE_LOW,
.hsync_level	= OMAPDSS_SIG_ACTIVE_LOW,

.data_pclk_edge	= OMAPDSS_DRIVE_SIG_FALLING_EDGE,
.de_level	= OMAPDSS_SIG_ACTIVE_HIGH,
.sync_pclk_edge	= OMAPDSS_DRIVE_SIG_OPPOSITE_EDGES,
};

struct jbt_info {
	u_int16_t tx_buf[4];
	struct spi_device *spi_dev;
	int state;
	int gpio_cs;
	int gpio_scl;
	int gpio_din;
	int gpio_dout;
};

static struct jbt_info _jbt, *jbt = &_jbt;

#define JBT_COMMAND	0x000
#define JBT_DATA	0x100

#define SPI_READ()      (gpio_get_value(jbt->gpio_din))
#define SPI_CS(bit) 	(gpio_set_value(jbt->gpio_cs, bit))
#define SPI_SDA(bit)    (gpio_set_value(jbt->gpio_dout, bit))
#define SPI_SCL(bit)    (gpio_set_value(jbt->gpio_scl, bit))

/* 150uS minimum clock cycle, we have two of this plus our other
 * instructions */

#define SPI_DELAY()	udelay(200)

struct panel_drv_data {
	struct mutex lock;
};

static int jbt_spi_xfer(int wordnum, int bitlen, u_int16_t *dout)
{
	u_int16_t tmpdout = 0;
	int   i, j;
	
// 	printk("jbt_spi_xfer: dout %08X wordnum %u bitlen %d\n", *(uint *)dout, wordnum, bitlen);
	
	SPI_CS(0);
	
	for (i = 0; i < wordnum; i ++) {
		tmpdout = dout[i];
		
		for (j = 0; j < bitlen; j++) {
			SPI_SCL(0);
			if (tmpdout & (1 << (bitlen-1))) {
				SPI_SDA(1);
				if(SPI_READ() == 0)
					return 1;
			} else {
				SPI_SDA(0);
				if(SPI_READ() != 0)
					return 1;
			}
			SPI_DELAY();
			SPI_SCL(1);
			SPI_DELAY();
			tmpdout <<= 1;
		}
	}
	
	SPI_CS(1);
	
	return 0;
}

#define JBT_COMMAND	0x000
#define JBT_DATA	0x100

int jbt_reg_write_nodata(struct jbt_info *jbt, u_int8_t reg)
{
	int rc;
	
	jbt->tx_buf[0] = JBT_COMMAND | reg;
	
	rc = jbt_spi_xfer(1, 9, jbt->tx_buf);
	
	return rc;
}


int jbt_reg_write(struct jbt_info *jbt, u_int8_t reg, u_int8_t data)
{
	int rc;
	
	jbt->tx_buf[0] = JBT_COMMAND | reg;
	jbt->tx_buf[1] = JBT_DATA | data;
	
	rc = jbt_spi_xfer(2, 9, jbt->tx_buf);
	
	return rc;
}

int jbt_reg_write16(struct jbt_info *jbt, u_int8_t reg, u_int16_t data)
{
	int rc;
	
	jbt->tx_buf[0] = JBT_COMMAND | reg;
	jbt->tx_buf[1] = JBT_DATA | (data >> 8);
	jbt->tx_buf[2] = JBT_DATA | (data & 0xff);
	
	rc = jbt_spi_xfer(3, 9, jbt->tx_buf);
	
	return rc;
}

static int jbt_reg_init(struct jbt_info *jbt)
{
	int r;
	
	printk("jbt_reg_init()\n");
	
	r = gpio_request(jbt->gpio_cs, "TD028_CS");
	if(r < 0)
		printk(KERN_ERR "Unable to get TD028_CS GPIO %d\n", jbt->gpio_cs);
	r = gpio_request(jbt->gpio_scl, "TD028_SCL");
	if(r < 0)
		printk(KERN_ERR "Unable to get TD028_SCL GPIO %d\n", jbt->gpio_scl);
	r = gpio_request(jbt->gpio_dout, "TD028_DOUT");
	if(r < 0)
		printk(KERN_ERR "Unable to get TD028_DOUT GPIO %d\n", jbt->gpio_dout);
	r = gpio_request(jbt->gpio_din, "TD028_DIN");
	if(r < 0)
		printk(KERN_ERR "Unable to get TD028_DIN GPIO %d\n", jbt->gpio_din);

	gpio_direction_output(jbt->gpio_cs, true);
	gpio_direction_output(jbt->gpio_scl, true);
	gpio_direction_output(jbt->gpio_dout, true);
	gpio_direction_input(jbt->gpio_din);

	SPI_CS(1);	// unselect
	SPI_SCL(1);	// inactive
	SPI_SDA(0);	// default

	/* according to data sheet: wait 50ms (Tpos of LCM). However, 50ms
	 * seems unreliable with later LCM batches, increasing to 90ms */
	mdelay(90);
	
#if 0
	for(i=0; i<16; i++)
		{ // check for connection between GPIO158 -> GPIO159; since we have 10 kOhm pse. make sure that the PUP/PDN is disabled in the MUX config!
			int bit=i&1;
			SPI_SDA(bit);	// write bit
			SPI_DELAY();
#if 1
			printk("bit: %d out: %d in: %d (%d)\n", bit, gpio_get_value(GPIO_DOUT), gpio_get_value(GPIO_DIN), SPI_READ());
#endif
			if(SPI_READ() != bit)	// did not read back correctly
				failed++;
		}	
	if(failed > 0)
		{
		printk("  machine_arch_type = %d (%d)\n", machine_arch_type, machine_is_gta04());
		printk("jbt_reg_init() - no correct response, assuming no connection between GPIO%d and GPIO%d\n", GPIO_DOUT, GPIO_DIN);
		return 1;
		}
#endif
	
	printk("did jbt_reg_init()\n");
	return 0;
}

enum jbt_register {
	JBT_REG_SLEEP_IN		= 0x10,
	JBT_REG_SLEEP_OUT		= 0x11,
	
	JBT_REG_DISPLAY_OFF		= 0x28,
	JBT_REG_DISPLAY_ON		= 0x29,
	
	JBT_REG_RGB_FORMAT		= 0x3a,
	JBT_REG_QUAD_RATE		= 0x3b,
	
	JBT_REG_POWER_ON_OFF		= 0xb0,
	JBT_REG_BOOSTER_OP		= 0xb1,
	JBT_REG_BOOSTER_MODE		= 0xb2,
	JBT_REG_BOOSTER_FREQ		= 0xb3,
	JBT_REG_OPAMP_SYSCLK		= 0xb4,
	JBT_REG_VSC_VOLTAGE		= 0xb5,
	JBT_REG_VCOM_VOLTAGE		= 0xb6,
	JBT_REG_EXT_DISPL		= 0xb7,
	JBT_REG_OUTPUT_CONTROL		= 0xb8,
	JBT_REG_DCCLK_DCEV		= 0xb9,
	JBT_REG_DISPLAY_MODE1		= 0xba,
	JBT_REG_DISPLAY_MODE2		= 0xbb,
	JBT_REG_DISPLAY_MODE		= 0xbc,
	JBT_REG_ASW_SLEW		= 0xbd,
	JBT_REG_DUMMY_DISPLAY		= 0xbe,
	JBT_REG_DRIVE_SYSTEM		= 0xbf,
	
	JBT_REG_SLEEP_OUT_FR_A		= 0xc0,
	JBT_REG_SLEEP_OUT_FR_B		= 0xc1,
	JBT_REG_SLEEP_OUT_FR_C		= 0xc2,
	JBT_REG_SLEEP_IN_LCCNT_D	= 0xc3,
	JBT_REG_SLEEP_IN_LCCNT_E	= 0xc4,
	JBT_REG_SLEEP_IN_LCCNT_F	= 0xc5,
	JBT_REG_SLEEP_IN_LCCNT_G	= 0xc6,
	
	JBT_REG_GAMMA1_FINE_1		= 0xc7,
	JBT_REG_GAMMA1_FINE_2		= 0xc8,
	JBT_REG_GAMMA1_INCLINATION	= 0xc9,
	JBT_REG_GAMMA1_BLUE_OFFSET	= 0xca,
	
	JBT_REG_BLANK_CONTROL		= 0xcf,
	JBT_REG_BLANK_TH_TV		= 0xd0,
	JBT_REG_CKV_ON_OFF		= 0xd1,
	JBT_REG_CKV_1_2			= 0xd2,
	JBT_REG_OEV_TIMING		= 0xd3,
	JBT_REG_ASW_TIMING_1		= 0xd4,
	JBT_REG_ASW_TIMING_2		= 0xd5,
	
	JBT_REG_HCLOCK_VGA		= 0xec,
	JBT_REG_HCLOCK_QVGA		= 0xed,
	
};

static struct panel_td028ttec1_data
*get_panel_data(struct omap_dss_device *dssdev)
{
	return (struct panel_td028ttec1_data *) dssdev->data;
}

static int td028ttec1_panel_probe(struct omap_dss_device *dssdev)
{
	struct panel_td028ttec1_data *panel_data = get_panel_data(dssdev);
	int rc;
	struct panel_drv_data *drv_data = NULL;

	drv_data = devm_kzalloc(&dssdev->dev, sizeof(*drv_data), GFP_KERNEL);
	if (!drv_data)
		return -ENOMEM;
	mutex_init(&drv_data->lock);
	dev_set_drvdata(&dssdev->dev, drv_data);
	printk("td028ttec1_panel_probe()\n");
	dssdev->panel.timings = td028ttec1_panel_timings;
	
	jbt->gpio_cs = panel_data->gpio_cs;
	jbt->gpio_scl = panel_data->gpio_scl;
	jbt->gpio_din = panel_data->gpio_din;
	jbt->gpio_dout = panel_data->gpio_dout;

	rc = jbt_reg_init(jbt);
	
	return 0;
	return rc ? -ENODEV : 0;
}

static void td028ttec1_panel_remove(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *drv_data = dev_get_drvdata(&dssdev->dev);
	printk("td028ttec1_panel_remove()\n");
	// disable GPIOs?
	dev_set_drvdata(&dssdev->dev, NULL);
}

static int td028ttec1_panel_enable(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *drv_data = dev_get_drvdata(&dssdev->dev);
	int rc = 0;
	printk("td028ttec1_panel_enable() - state %d\n", dssdev->state);
	mutex_lock(&drv_data->lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_ACTIVE) {
		mutex_unlock(&drv_data->lock);
		return rc;
	}

	if (dssdev->platform_enable)
		rc = dssdev->platform_enable(dssdev);	// enable e.g. power, backlight

	if(rc) {
		mutex_unlock(&drv_data->lock);
		return rc;
	}

	// 1. standby_to_sleep()

	/* three times command zero */
	rc = jbt_reg_write_nodata(jbt, 0x00);
	udelay(1000);
	rc = jbt_reg_write_nodata(jbt, 0x00);
	udelay(1000);
	rc = jbt_reg_write_nodata(jbt, 0x00);
	udelay(1000);

	/* deep standby out */
	rc |= jbt_reg_write(jbt, JBT_REG_POWER_ON_OFF, 0x17);

	// 2. sleep_to_normal()

	/* RGB I/F on, RAM write off, QVGA through, SIGCON enable */
	rc = jbt_reg_write(jbt, JBT_REG_DISPLAY_MODE, 0x80);

	/* Quad mode off */
	rc |= jbt_reg_write(jbt, JBT_REG_QUAD_RATE, 0x00);

	/* AVDD on, XVDD on */
	rc |= jbt_reg_write(jbt, JBT_REG_POWER_ON_OFF, 0x16);

	/* Output control */
	rc |= jbt_reg_write16(jbt, JBT_REG_OUTPUT_CONTROL, 0xfff9);

	/* Sleep mode off */
	rc |= jbt_reg_write_nodata(jbt, JBT_REG_SLEEP_OUT);

	/* at this point we have like 50% grey */

	/* initialize register set */
	rc = jbt_reg_write(jbt, JBT_REG_DISPLAY_MODE1, 0x01);
	rc |= jbt_reg_write(jbt, JBT_REG_DISPLAY_MODE2, 0x00);
	rc |= jbt_reg_write(jbt, JBT_REG_RGB_FORMAT, 0x60);
	rc |= jbt_reg_write(jbt, JBT_REG_DRIVE_SYSTEM, 0x10);
	rc |= jbt_reg_write(jbt, JBT_REG_BOOSTER_OP, 0x56);
	rc |= jbt_reg_write(jbt, JBT_REG_BOOSTER_MODE, 0x33);
	rc |= jbt_reg_write(jbt, JBT_REG_BOOSTER_FREQ, 0x11);
	rc |= jbt_reg_write(jbt, JBT_REG_BOOSTER_FREQ, 0x11);
	rc |= jbt_reg_write(jbt, JBT_REG_OPAMP_SYSCLK, 0x02);
	rc |= jbt_reg_write(jbt, JBT_REG_VSC_VOLTAGE, 0x2b);
	rc |= jbt_reg_write(jbt, JBT_REG_VCOM_VOLTAGE, 0x40);
	rc |= jbt_reg_write(jbt, JBT_REG_EXT_DISPL, 0x03);
	rc |= jbt_reg_write(jbt, JBT_REG_DCCLK_DCEV, 0x04);
	/*
	 * default of 0x02 in JBT_REG_ASW_SLEW responsible for 72Hz requirement
	 * to avoid red / blue flicker
	 */
	rc |= jbt_reg_write(jbt, JBT_REG_ASW_SLEW, 0x04);
	rc |= jbt_reg_write(jbt, JBT_REG_DUMMY_DISPLAY, 0x00);

	rc |= jbt_reg_write(jbt, JBT_REG_SLEEP_OUT_FR_A, 0x11);
	rc |= jbt_reg_write(jbt, JBT_REG_SLEEP_OUT_FR_B, 0x11);
	rc |= jbt_reg_write(jbt, JBT_REG_SLEEP_OUT_FR_C, 0x11);
	rc |= jbt_reg_write16(jbt, JBT_REG_SLEEP_IN_LCCNT_D, 0x2040);
	rc |= jbt_reg_write16(jbt, JBT_REG_SLEEP_IN_LCCNT_E, 0x60c0);
	rc |= jbt_reg_write16(jbt, JBT_REG_SLEEP_IN_LCCNT_F, 0x1020);
	rc |= jbt_reg_write16(jbt, JBT_REG_SLEEP_IN_LCCNT_G, 0x60c0);

	rc |= jbt_reg_write16(jbt, JBT_REG_GAMMA1_FINE_1, 0x5533);
	rc |= jbt_reg_write(jbt, JBT_REG_GAMMA1_FINE_2, 0x00);
	rc |= jbt_reg_write(jbt, JBT_REG_GAMMA1_INCLINATION, 0x00);
	rc |= jbt_reg_write(jbt, JBT_REG_GAMMA1_BLUE_OFFSET, 0x00);
	rc |= jbt_reg_write(jbt, JBT_REG_GAMMA1_BLUE_OFFSET, 0x00);

	rc |= jbt_reg_write16(jbt, JBT_REG_HCLOCK_VGA, 0x1f0);
	rc |= jbt_reg_write(jbt, JBT_REG_BLANK_CONTROL, 0x02);
	rc |= jbt_reg_write16(jbt, JBT_REG_BLANK_TH_TV, 0x0804);
	rc |= jbt_reg_write16(jbt, JBT_REG_BLANK_TH_TV, 0x0804);

	rc |= jbt_reg_write(jbt, JBT_REG_CKV_ON_OFF, 0x01);
	rc |= jbt_reg_write16(jbt, JBT_REG_CKV_1_2, 0x0000);

	rc |= jbt_reg_write16(jbt, JBT_REG_OEV_TIMING, 0x0d0e);
	rc |= jbt_reg_write16(jbt, JBT_REG_ASW_TIMING_1, 0x11a4);
	rc |= jbt_reg_write(jbt, JBT_REG_ASW_TIMING_2, 0x0e);

#if 0
	rc |= jbt_reg_write16(jbt, JBT_REG_HCLOCK_QVGA, 0x00ff);
	rc |= jbt_reg_write16(jbt, JBT_REG_HCLOCK_QVGA, 0x00ff);
#endif

	jbt_reg_write_nodata(jbt, JBT_REG_DISPLAY_ON);

	omapdss_dpi_set_timings(dssdev, &dssdev->panel.timings);
	omapdss_dpi_set_data_lines(dssdev, dssdev->phy.dpi.data_lines);
	rc |= omapdss_dpi_display_enable(dssdev);
	if(rc) {
		mutex_unlock(&drv_data->lock);
		return -EIO;
	}
	dssdev->state = OMAP_DSS_DISPLAY_ACTIVE;
	mutex_unlock(&drv_data->lock);

	return 0;
}

static void td028ttec1_panel_disable(struct omap_dss_device *dssdev)
{
	struct panel_drv_data *drv_data = dev_get_drvdata(&dssdev->dev);
	printk("td028ttec1_panel_disable()\n");
	mutex_lock(&drv_data->lock);
	if (dssdev->state == OMAP_DSS_DISPLAY_DISABLED) {
		mutex_unlock(&drv_data->lock);
		return;
	}
	if (dssdev->platform_disable)
		dssdev->platform_disable(dssdev);

	// 1. normal_to_sleep()

	printk("td028ttec1_panel_suspend()\n");

	omapdss_dpi_display_disable(dssdev);

	jbt_reg_write_nodata(jbt, JBT_REG_DISPLAY_OFF);
	jbt_reg_write16(jbt, JBT_REG_OUTPUT_CONTROL, 0x8002);
	jbt_reg_write_nodata(jbt, JBT_REG_SLEEP_IN);

	// 2. sleep_to_standby()

	jbt_reg_write(jbt, JBT_REG_POWER_ON_OFF, 0x00);

	dssdev->state=OMAP_DSS_DISPLAY_DISABLED;
	mutex_unlock(&drv_data->lock);
}

static void td028ttec1_panel_set_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	omapdss_dpi_set_timings(dssdev, timings);
	dssdev->panel.timings = *timings;
}

static void td028ttec1_panel_get_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	*timings = dssdev->panel.timings;
}

static int td028ttec1_panel_check_timings(struct omap_dss_device *dssdev,
		struct omap_video_timings *timings)
{
	return dpi_check_timings(dssdev, timings);
}

static struct omap_dss_driver td028ttec1_driver = {
	.probe		= td028ttec1_panel_probe,
	.remove		= td028ttec1_panel_remove,

	.enable		= td028ttec1_panel_enable,
	.disable	= td028ttec1_panel_disable,

	.set_timings	= td028ttec1_panel_set_timings,
	.get_timings	= td028ttec1_panel_get_timings,
	.check_timings	= td028ttec1_panel_check_timings,

	.driver         = {
		.name   = "td028ttec1_panel",
		.owner  = THIS_MODULE,
	},
};

static int __init td028ttec1_panel_drv_init(void)
{
	return omap_dss_register_driver(&td028ttec1_driver);
}

static void __exit td028ttec1_panel_drv_exit(void)
{
	omap_dss_unregister_driver(&td028ttec1_driver);
}

module_init(td028ttec1_panel_drv_init);
module_exit(td028ttec1_panel_drv_exit);
MODULE_LICENSE("GPL");
