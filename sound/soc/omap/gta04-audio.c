/*
 * gta04.c  --  SoC audio for GTA04 (based on OMAP3 Beagle)
 *
 * Author: Steve Sakoman <steve@sakoman.com>
 * Author: Nikolaus Schaller <hns@goldelico.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 *
 */

#include <linux/clk.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/soc.h>
#include <sound/soc-dapm.h>

#include <asm/mach-types.h>
#include <mach/hardware.h>
#include <mach/gpio.h>
#include <plat/mcbsp.h>

#include "omap-mcbsp.h"
#include "omap-pcm.h"
#include "../codecs/twl4030.h"

static int omap3gta04_hw_params(struct snd_pcm_substream *substream,
	struct snd_pcm_hw_params *params)
{
	struct snd_soc_pcm_runtime *rtd = substream->private_data;
	struct snd_soc_dai *codec_dai = rtd->codec_dai;
	struct snd_soc_dai *cpu_dai = rtd->cpu_dai;
	unsigned int fmt;
	int ret;

	switch (params_channels(params)) {
	case 2: /* Stereo I2S mode */
		fmt =	SND_SOC_DAIFMT_I2S |
			SND_SOC_DAIFMT_NB_NF |
			SND_SOC_DAIFMT_CBM_CFM;
		break;
	case 4: /* Four channel TDM mode */
		fmt =	SND_SOC_DAIFMT_DSP_A |
			SND_SOC_DAIFMT_IB_NF |
			SND_SOC_DAIFMT_CBM_CFM;
		break;
	default:
		return -EINVAL;
	}

	/* Set codec DAI configuration */
	ret = snd_soc_dai_set_fmt(codec_dai, fmt);
	if (ret < 0) {
		printk(KERN_ERR "can't set codec DAI configuration\n");
		return ret;
	}

	/* Set cpu DAI configuration */
	ret = snd_soc_dai_set_fmt(cpu_dai, fmt);
	if (ret < 0) {
		printk(KERN_ERR "can't set cpu DAI configuration\n");
		return ret;
	}

	/* Set the codec system clock for DAC and ADC */
	ret = snd_soc_dai_set_sysclk(codec_dai, 0, 26000000,
				     SND_SOC_CLOCK_IN);
	if (ret < 0) {
		printk(KERN_ERR "can't set codec system clock\n");
		return ret;
	}

	return 0;
}

/* this shows how we could control the AUX in/out switch or the Video in/out */

static int omap3pandora_hp_event(struct snd_soc_dapm_widget *w,
								 struct snd_kcontrol *k, int event)
{
	/*
	 if (SND_SOC_DAPM_EVENT_ON(event)) {
	 gpio_set_value(OMAP3_PANDORA_DAC_POWER_GPIO, 1);
	 gpio_set_value(OMAP3_PANDORA_AMP_POWER_GPIO, 1);
	 } else {
	 gpio_set_value(OMAP3_PANDORA_AMP_POWER_GPIO, 0);
	 mdelay(1);
	 gpio_set_value(OMAP3_PANDORA_DAC_POWER_GPIO, 0);
	 }
	 */	
	return 0;
}

static const struct snd_soc_dapm_widget gta04_dapm_widgets[] = {
	SND_SOC_DAPM_DAC("PCM DAC", "HiFi Playback", SND_SOC_NOPM, 0, 0),
	SND_SOC_DAPM_PGA_E("Headphone Amplifier", SND_SOC_NOPM,
					   0, 0, NULL, 0, omap3pandora_hp_event,
					   SND_SOC_DAPM_POST_PMU | SND_SOC_DAPM_PRE_PMD),
	SND_SOC_DAPM_HP("Headphone Jack", NULL),
	SND_SOC_DAPM_LINE("Line Out", NULL),
	SND_SOC_DAPM_MIC("Internal Mic", NULL),
	SND_SOC_DAPM_MIC("Headphone Mic", NULL),
	SND_SOC_DAPM_LINE("Line In", NULL),
};

static const struct snd_soc_dapm_route audio_map[] = {
	{"Headphone Amplifier", NULL, "PCM DAC"},
	{"Line Out", NULL, "PCM DAC"},
	{"Headphone Jack", NULL, "Headphone Amplifier"},

	{"AUXL", NULL, "Line In"},
	{"AUXR", NULL, "Line In"},
	
	/* Headset Mic: HSMIC with bias */
	{"HSMIC", NULL, "Headset Mic Bias"},
	{"Headset Mic Bias", NULL, "Headphone Mic"},

	{"MAINMIC", NULL, "Mic Bias 1"},
	{"Mic Bias 1", NULL, "Internal Mic"},
	/*
	 {"SUBMIC", NULL, "Mic Bias 2"},
	 {"Mic Bias 2", NULL, "Mic (external)"},
	 */
};

static int omap3gta04_init(struct snd_soc_pcm_runtime *runtime)
{
	int ret;
	struct snd_soc_codec *codec = runtime->codec;
	struct snd_soc_dapm_context *dapm = &codec->dapm;
	
	ret = snd_soc_dapm_new_controls(dapm, gta04_dapm_widgets,
					ARRAY_SIZE(gta04_dapm_widgets));
	if (ret < 0)
		return ret;
	
	snd_soc_dapm_add_routes(dapm, audio_map,
							ARRAY_SIZE(audio_map));

//	snd_soc_dapm_enable_pin(codec, "Ext Mic");
//	snd_soc_dapm_enable_pin(codec, "Ext Spk");
//	snd_soc_dapm_disable_pin(codec, "Headphone Mic");
//	snd_soc_dapm_disable_pin(codec, "Headphone Amplifier");

	/* TWL4030 not connected pins */
	//	snd_soc_dapm_nc_pin(codec, "OUTL");
	//	snd_soc_dapm_nc_pin(codec, "OUTR");
	//	snd_soc_dapm_nc_pin(codec, "EARPIECE");
	snd_soc_dapm_nc_pin(dapm, "PREDRIVEL");
	snd_soc_dapm_nc_pin(dapm, "PREDRIVER");
	//	snd_soc_dapm_nc_pin(codec, "HSOL");
	//	snd_soc_dapm_nc_pin(codec, "HSOR");
	snd_soc_dapm_nc_pin(dapm, "CARKITMIC");
	snd_soc_dapm_nc_pin(dapm, "CARKITL");
	snd_soc_dapm_nc_pin(dapm, "CARKITR");
	//	snd_soc_dapm_nc_pin(codec, "HFL");
	//	snd_soc_dapm_nc_pin(codec, "HFR");
	//	snd_soc_dapm_nc_pin(codec, "VIBRA");
	//	snd_soc_dapm_nc_pin(codec, "HSMIC");
	snd_soc_dapm_nc_pin(dapm, "DIGIMIC0");
	snd_soc_dapm_nc_pin(dapm, "DIGIMIC1");
		
	return snd_soc_dapm_sync(dapm);
}

static struct snd_soc_ops omap3gta04_ops = {
	.hw_params = omap3gta04_hw_params,
};

/* Digital audio interface glue - connects codec <--> CPU */
static struct snd_soc_dai_link omap3gta04_dai = {
		.name = "TWL4030",
		.stream_name = "TWL4030",
		.cpu_dai_name	= "omap-mcpdm-dai.0",
		.platform_name = "omap-pcm-audio",
		.codec_dai_name = "twl4030-hifi",
		.ops = &omap3gta04_ops,
		.init = &omap3gta04_init
};

/* Audio machine driver */
static struct snd_soc_card snd_soc_omap3gta04 = {
	.name = "gta04",
	.owner = THIS_MODULE,
	.dai_link = &omap3gta04_dai,
	.num_links = 1,
};

static struct platform_device *omap3gta04_snd_device;

static int __init omap3gta04_soc_init(void)
{
	int ret;

#if 0
	if (!machine_is_gta04() && !machine_is_omap3_gta04()) {
		pr_debug("Not GTA04!\n");
		return -ENODEV;
	}
#endif
	pr_info("GTA04 OMAP3 SoC snd init\n");
	
	// FIXME: set any GPIOs i.e. enable Audio in/out switch
	// microphone power etc.

	omap3gta04_snd_device = platform_device_alloc("soc-audio", -1);
	if (!omap3gta04_snd_device) {
		printk(KERN_ERR "Platform device allocation failed\n");
		return -ENOMEM;
	}

	platform_set_drvdata(omap3gta04_snd_device, &snd_soc_omap3gta04);

	ret = platform_device_add(omap3gta04_snd_device);
	if (ret)
		goto err1;

	return 0;

err1:
	printk(KERN_ERR "Unable to add platform device\n");
	platform_device_put(omap3gta04_snd_device);

	return ret;
}

static void __exit omap3gta04_soc_exit(void)
{
	platform_device_unregister(omap3gta04_snd_device);
}

module_init(omap3gta04_soc_init);
module_exit(omap3gta04_soc_exit);

MODULE_AUTHOR("Steve Sakoman <steve@sakoman.com>");
MODULE_DESCRIPTION("ALSA SoC OMAP3 GTA04");
MODULE_LICENSE("GPL");
