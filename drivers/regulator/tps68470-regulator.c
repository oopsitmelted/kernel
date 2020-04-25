#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/err.h>
#include <linux/regmap.h>
#include <linux/of.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/of_regulator.h>
#include <linux/mfd/core.h>
#include <linux/mfd/tps68470.h>

struct tps68470_regulator{
    struct regmap *regmap;
    struct regulator_dev *regulator;
};

static int tps68470_regulator_enable(struct regulator_dev *rdev)
{
	struct tps68470_regulator *tps68470 = rdev_get_drvdata(rdev);
	int ret;
	dev_err(&rdev->dev,"enable\n");
	/* Activate voltage mode */
	ret = regmap_update_bits(tps68470->regmap, TPS68470_REG_VAUX1CTL,
		TPS68470_VAUX1CTL_EN_MASK,
		1);
	if (ret)
		return ret;

	return 0;
}

static int tps68470_regulator_disable(struct regulator_dev *rdev)
{
	struct tps68470_regulator *tps68470 = rdev_get_drvdata(rdev);
	int ret;
	dev_err(&rdev->dev,"disable\n");
	/* Set into shutdown mode */
	ret = regmap_update_bits(tps68470->regmap, TPS68470_REG_VAUX1CTL,
		TPS68470_VAUX1CTL_EN_MASK,
		0);
	if (ret)
		return ret;

	return 0;
}

static int tps68470_regulator_is_enabled(struct regulator_dev *rdev)
{
	struct tps68470_regulator *tps68470 = rdev_get_drvdata(rdev);
	unsigned int regval;
	int ret;
	dev_err(&rdev->dev,"is enabled?\n");
	ret = regmap_read(tps68470->regmap, TPS68470_REG_VAUX1CTL, &regval);
	if (ret)
		return ret;
	regval &= TPS68470_VAUX1CTL_EN_MASK;

	if (regval)
		return 1;

	return 0;
}

static int tps68470_regulator_get_voltage(struct regulator_dev *rdev)
{
	dev_err(&rdev->dev,"get_voltage\n");
	return 3300000;
}

static struct regulator_ops tps68470_regulator_ops = {
	.enable		= tps68470_regulator_enable,
	.disable	= tps68470_regulator_disable,
	.is_enabled	= tps68470_regulator_is_enabled,
	.get_voltage = tps68470_regulator_get_voltage,
	//.set_voltage_sel = tps68470_regulator_set_voltage_sel,
	//.list_voltage	= regulator_list_voltage_table,
};

static const struct regulator_desc tps68470_regulator_desc = {
	.name		= "tps68470-3v3",
	.ops		= &tps68470_regulator_ops,
	.type		= REGULATOR_VOLTAGE,
	.id		    = 0,
	.owner		= THIS_MODULE,
    .fixed_uV   = 3300000,
};

static struct of_regulator_match tps68470_matches[] = {
	{ .name = "fixed", },
};

/*
 * Registers the chip as a voltage regulator
 */
static int tps68470_regulator_probe(struct platform_device *pdev)
{
	struct tps68470_regulator *tps68470;
	struct regulator_config config = { };
	struct device_node *parent, *np;
	int ret;

	dev_err(&pdev->dev, "probe\n");
	tps68470 = devm_kzalloc(&pdev->dev, sizeof(*tps68470),
				     GFP_KERNEL);
	if (!tps68470)
		return -ENOMEM;

	/* Get the device (PMIC) node */
	np = of_node_get(pdev->dev.of_node);
	if (!np)
		return -EINVAL;

	/* Get 'regulators' subnode */

	parent = of_get_child_by_name(np, "regulators");
	if (!parent) {
		dev_err(&pdev->dev, "regulators node not found\n");
		return -EINVAL;
	}

	ret = of_regulator_match(&pdev->dev, parent, tps68470_matches, 
		ARRAY_SIZE(tps68470_matches));
	of_node_put(parent);

	if (ret < 0) {
		dev_err(&pdev->dev, "Error parsing regulator init data: %d\n",ret);
		return ret;
	}

	config.dev = &pdev->dev;
	config.driver_data = tps68470;
	config.of_node = tps68470_matches[0].of_node;
	config.init_data = tps68470_matches[0].init_data;

    tps68470->regmap = dev_get_drvdata(pdev->dev.parent);
	/* Register regulator with framework */
	tps68470->regulator = devm_regulator_register(&pdev->dev,
						      &tps68470_regulator_desc,
						      &config);
	if (IS_ERR(tps68470->regulator)) {
		ret = PTR_ERR(tps68470->regulator);
		dev_err(&pdev->dev,
			"failed to register regulator\n");
		return ret;
	}
	platform_set_drvdata(pdev, tps68470);

	return 0;
}

static struct platform_driver tps68470_regulator_driver = {
	.driver = {
		.name  = "tps68470-regulator",
	},
	.probe = tps68470_regulator_probe,
};

static __init int tps68470_regulator_init(void)
{
	return platform_driver_register(&tps68470_regulator_driver);
}
subsys_initcall(tps68470_regulator_init);

static __exit void tps68470_regulator_exit(void)
{
	platform_driver_unregister(&tps68470_regulator_driver);
}
module_exit(tps68470_regulator_exit);

MODULE_AUTHOR("Linus Walleij <linus.walleij@linaro.org>");
MODULE_DESCRIPTION("tps68470 regulator driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:tps68470-regulator");