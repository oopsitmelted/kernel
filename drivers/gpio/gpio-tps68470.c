// SPDX-License-Identifier: GPL-2.0
/*
 * GPIO driver for TPS68470 PMIC
 *
 * Copyright (C) 2017 Intel Corporation
 *
 * Authors:
 *	Antti Laakso <antti.laakso@intel.com>
 *	Tianshu Qiu <tian.shu.qiu@intel.com>
 *	Jian Xu Zheng <jian.xu.zheng@intel.com>
 *	Yuning Pu <yuning.pu@intel.com>
 */

#include <linux/gpio/driver.h>
#include <linux/mfd/tps68470.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>

#define TPS68470_N_LOGIC_OUTPUT	3
#define TPS68470_N_REGULAR_GPIO	7
#define TPS68470_N_GPIO	(TPS68470_N_LOGIC_OUTPUT + TPS68470_N_REGULAR_GPIO)
#define GPIO_LINE_DIRECTION_IN	1
#define GPIO_LINE_DIRECTION_OUT	0

struct tps68470_gpio{
	struct regmap *regmap;
	struct gpio_chip chip;
};

static inline struct tps68470_gpio *to_cg(struct gpio_chip *gc)
{
	return container_of(gc, struct tps68470_gpio, chip);
}

static int tps68470_gpio_get(struct gpio_chip *chip, unsigned int offset)
{
	struct tps68470_gpio *cg = to_cg(chip);
	struct regmap *regmap = cg->regmap;
	unsigned int reg = TPS68470_REG_GPDO;
	int val, ret;

	dev_err(cg->chip.dev, "tps68470: gpio_get: %d\n", offset);
	if (offset >= TPS68470_N_REGULAR_GPIO) {
		offset -= TPS68470_N_REGULAR_GPIO;
		reg = TPS68470_REG_SGPO;
	}

	ret = regmap_read(regmap, reg, &val);
	if (ret) {
		dev_err(cg->chip.dev, "reg 0x%x read failed\n",
			TPS68470_REG_SGPO);
		return ret;
	}
	return !!(val & BIT(offset));
}

static int tps68470_gpio_get_direction(struct gpio_chip *chip,
				       unsigned int offset)
{
	struct tps68470_gpio *cg = to_cg(chip);
	struct regmap *regmap = cg->regmap;
	int val, ret;

	dev_err(cg->chip.dev, "tps68470: gpio_get_direction: %d\n", offset);
	/* rest are always outputs */
	if (offset >= TPS68470_N_REGULAR_GPIO)
		return GPIO_LINE_DIRECTION_OUT;

	ret = regmap_read(regmap, TPS68470_GPIO_CTL_REG_A(offset), &val);
	if (ret) {
		dev_err(cg->chip.dev, "reg 0x%x read failed\n",
			TPS68470_GPIO_CTL_REG_A(offset));
		return ret;
	}

	val &= TPS68470_GPIO_MODE_MASK;
	return val >= TPS68470_GPIO_MODE_OUT_CMOS ? GPIO_LINE_DIRECTION_OUT :
						    GPIO_LINE_DIRECTION_IN;
}

static void tps68470_gpio_set(struct gpio_chip *chip, unsigned int offset,
				int value)
{
	struct tps68470_gpio *cg = to_cg(chip);
	struct regmap *regmap = cg->regmap;
	unsigned int reg = TPS68470_REG_GPDO;

	dev_err(cg->chip.dev, "tps68470: gpio_set: %d\n", offset);
	if (offset >= TPS68470_N_REGULAR_GPIO) {
		reg = TPS68470_REG_SGPO;
		offset -= TPS68470_N_REGULAR_GPIO;
	}

	regmap_update_bits(regmap, reg, BIT(offset), value ? BIT(offset) : 0);
}

static int tps68470_gpio_output(struct gpio_chip *chip, unsigned int offset,
				int value)
{
	struct tps68470_gpio *cg = to_cg(chip);
	struct regmap *regmap = cg->regmap;

	dev_err(cg->chip.dev, "tps68470: gpio_output: %d\n", offset);
	/* rest are always outputs */
	if (offset >= TPS68470_N_REGULAR_GPIO)
		return 0;

	/* Set the initial value */
	tps68470_gpio_set(chip, offset, value);

	return regmap_update_bits(regmap, TPS68470_GPIO_CTL_REG_A(offset),
				 TPS68470_GPIO_MODE_MASK,
				 TPS68470_GPIO_MODE_OUT_CMOS);
}

static int tps68470_gpio_input(struct gpio_chip *chip, unsigned int offset)
{
	struct tps68470_gpio *cg = to_cg(chip);
	struct regmap *regmap = cg->regmap;

	dev_err(cg->chip.dev, "tps68470: gpio_input: %d\n", offset);
	/* rest are always outputs */
	if (offset >= TPS68470_N_REGULAR_GPIO)
		return -EINVAL;

	return regmap_update_bits(regmap, TPS68470_GPIO_CTL_REG_A(offset),
				   TPS68470_GPIO_MODE_MASK, 0x00);
}

static const char *tps68470_names[TPS68470_N_GPIO] = {
	"gpio.0", "gpio.1", "gpio.2", "gpio.3",
	"gpio.4", "gpio.5", "gpio.6",
	"s_enable", "s_idle", "s_resetn",
};

static int tps68470_gpio_probe(struct platform_device *pdev)
{
	struct tps68470_gpio *cg;

	int ret;

	cg = devm_kzalloc(&pdev->dev, sizeof(*cg),
				     GFP_KERNEL);
	if (!cg)
		return -ENOMEM;

	platform_set_drvdata(pdev, cg);

	cg->regmap = dev_get_drvdata(pdev->dev.parent);
	cg->chip.label = "tps68470-gpio";
	cg->chip.owner = THIS_MODULE;
	cg->chip.direction_input = tps68470_gpio_input;
	cg->chip.direction_output = tps68470_gpio_output;
	cg->chip.get = tps68470_gpio_get;
	cg->chip.get_direction = tps68470_gpio_get_direction;
	cg->chip.set = tps68470_gpio_set;
	cg->chip.can_sleep = true;
	cg->chip.names = tps68470_names;
	cg->chip.ngpio = TPS68470_N_GPIO;
	cg->chip.base = -1;
	cg->chip.dev = &pdev->dev;

#if 0
	if (pdata && pdata->gpio_base)
		tps68470_->gc.base = pdata->gpio_base;
#endif
	ret = gpiochip_add(&cg->chip);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to register gpio_chip: %d\n", ret);
		return ret;
	}

	return ret;
}

static struct platform_driver tps68470_gpio_driver = {
	.driver = {
		   .name = "tps68470-gpio",
	},
	.probe = tps68470_gpio_probe,
};

builtin_platform_driver(tps68470_gpio_driver)
