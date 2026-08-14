#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/mutex.h>
#include <linux/dmi.h>

#define DRVNAME "d30_hwmon"

#define D30_PAGE   0xA00
#define D30_INDEX  0xA01
#define D30_DATA   0xA02

static DEFINE_MUTEX(d30_lock);
static struct device *hwmon_dev;

static const struct dmi_system_id d30_dmi_table[] = {
	{
		.ident = "Lenovo ThinkStation D30",
		.matches = {
			DMI_MATCH(DMI_SYS_VENDOR, "LENOVO"),
			DMI_MATCH(DMI_PRODUCT_NAME, "4353"),
			DMI_MATCH(DMI_PRODUCT_VERSION, "ThinkStation D30"),
		},
	},
	{ }
};

MODULE_DEVICE_TABLE(dmi, d30_dmi_table);


/*
 * Low-level NCT6681 access used by Lenovo ThinkStation D30 BIOS.
 * This driver is intentionally READ-ONLY:
 * DATA port 0xA02 is never written.
 */
static u8 __d30_read(u8 page, u8 reg)
{
	outb(0xff, D30_PAGE);
	outb(page, D30_PAGE);
	outb(reg, D30_INDEX);

	return inb(D30_DATA);
}

static u8 d30_read(u8 page, u8 reg)
{
	u8 value;

	mutex_lock(&d30_lock);

	value = __d30_read(page, reg);
	outb(0xff, D30_PAGE);

	mutex_unlock(&d30_lock);

	return value;
}

static u16 d30_read16(u8 page, u8 reg)
{
	u16 hi, lo;

	mutex_lock(&d30_lock);

	hi = __d30_read(page, reg);
	lo = __d30_read(page, reg + 1);

	outb(0xff, D30_PAGE);

	mutex_unlock(&d30_lock);

	return (hi << 8) | lo;
}


/* ============================================================
 * Temperatures
 *
 * Active physical NCT channels observed on this D30:
 * 1, 2, 3, 4, 5, 7, 8
 * ============================================================
 */

static const u8 temp_regs[] = {
	0x00, /* NCT temperature channel 1 */
	0x02, /* channel 2 */
	0x04, /* channel 3 */
	0x06, /* channel 4 */
	0x08, /* channel 5 */
	0x0c, /* channel 7 */
	0x0e, /* channel 8 */
};

static const char * const temp_labels[] = {
	"NCT Temp1 - Ambient/Intake candidate",
	"NCT Temp2 - Board sensor",
	"NCT Temp3 - Board sensor",
	"NCT Temp4 - PCH candidate",
	"NCT Temp5 - CPU board sensor",
	"NCT Temp7 - Board sensor",
	"NCT Temp8 - Memory area candidate",
};

static ssize_t temp_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	unsigned int idx = sattr->index;
	u8 raw;

	if (idx >= ARRAY_SIZE(temp_regs))
		return -EINVAL;

	raw = d30_read(1, temp_regs[idx]);

	return sprintf(buf, "%u\n", raw * 1000);
}

static ssize_t temp_label_show(struct device *dev,
			       struct device_attribute *attr,
			       char *buf)
{
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	unsigned int idx = sattr->index;

	if (idx >= ARRAY_SIZE(temp_labels))
		return -EINVAL;

	return sprintf(buf, "%s\n", temp_labels[idx]);
}

static SENSOR_DEVICE_ATTR_RO(temp1_input, temp, 0);
static SENSOR_DEVICE_ATTR_RO(temp2_input, temp, 1);
static SENSOR_DEVICE_ATTR_RO(temp3_input, temp, 2);
static SENSOR_DEVICE_ATTR_RO(temp4_input, temp, 3);
static SENSOR_DEVICE_ATTR_RO(temp5_input, temp, 4);
static SENSOR_DEVICE_ATTR_RO(temp6_input, temp, 5);
static SENSOR_DEVICE_ATTR_RO(temp7_input, temp, 6);

static SENSOR_DEVICE_ATTR_RO(temp1_label, temp_label, 0);
static SENSOR_DEVICE_ATTR_RO(temp2_label, temp_label, 1);
static SENSOR_DEVICE_ATTR_RO(temp3_label, temp_label, 2);
static SENSOR_DEVICE_ATTR_RO(temp4_label, temp_label, 3);
static SENSOR_DEVICE_ATTR_RO(temp5_label, temp_label, 4);
static SENSOR_DEVICE_ATTR_RO(temp6_label, temp_label, 5);
static SENSOR_DEVICE_ATTR_RO(temp7_label, temp_label, 6);


/* ============================================================
 * Fans
 *
 * Active physical NCT channels:
 * fan1, fan3, fan7
 * ============================================================
 */

static const u8 fan_regs[] = {
	0x28, /* physical NCT fan 1 */
	0x2c, /* physical NCT fan 3 */
	0x34, /* physical NCT fan 7 */
};

static const char * const fan_labels[] = {
	"NCT Fan1",
	"NCT Fan3 - CPU-related",
	"NCT Fan7",
};

static ssize_t fan_show(struct device *dev,
			struct device_attribute *attr,
			char *buf)
{
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	unsigned int idx = sattr->index;
	u16 rpm;

	if (idx >= ARRAY_SIZE(fan_regs))
		return -EINVAL;

	rpm = d30_read16(1, fan_regs[idx]);

	return sprintf(buf, "%u\n", rpm);
}

static ssize_t fan_label_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	unsigned int idx = sattr->index;

	if (idx >= ARRAY_SIZE(fan_labels))
		return -EINVAL;

	return sprintf(buf, "%s\n", fan_labels[idx]);
}

static SENSOR_DEVICE_ATTR_RO(fan1_input, fan, 0);
static SENSOR_DEVICE_ATTR_RO(fan2_input, fan, 1);
static SENSOR_DEVICE_ATTR_RO(fan3_input, fan, 2);

static SENSOR_DEVICE_ATTR_RO(fan1_label, fan_label, 0);
static SENSOR_DEVICE_ATTR_RO(fan2_label, fan_label, 1);
static SENSOR_DEVICE_ATTR_RO(fan3_label, fan_label, 2);


/* ============================================================
 * Voltages
 *
 * Only channels which produced a real non-zero reading
 * during our D30 test are exposed.
 *
 * Units returned to hwmon: millivolts.
 * ============================================================
 */

static const u8 voltage_regs[] = {
	0x19, /* VIN1 */
	0x1a, /* VIN2 */
	0x1b, /* VIN3 */
	0x1c, /* EC_VIN0 */
	0x21, /* EC_VIN5 */
	0x24, /* 3VCC */
	0x26, /* 3VSB */
};

static const unsigned int voltage_scale[] = {
	8,  /* VIN1 */
	8,  /* VIN2 */
	8,  /* VIN3 */
	8,  /* EC_VIN0 */
	8,  /* EC_VIN5 */
	16, /* 3VCC */
	16, /* 3VSB */
};

static const char * const voltage_labels[] = {
	"VIN1",
	"VIN2",
	"VIN3",
	"EC_VIN0",
	"EC_VIN5",
	"3VCC",
	"3VSB",
};

static ssize_t voltage_show(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	unsigned int idx = sattr->index;
	u8 raw;
	unsigned int mv;

	if (idx >= ARRAY_SIZE(voltage_regs))
		return -EINVAL;

	raw = d30_read(1, voltage_regs[idx]);
	mv = raw * voltage_scale[idx];

	return sprintf(buf, "%u\n", mv);
}

static ssize_t voltage_label_show(struct device *dev,
				  struct device_attribute *attr,
				  char *buf)
{
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	unsigned int idx = sattr->index;

	if (idx >= ARRAY_SIZE(voltage_labels))
		return -EINVAL;

	return sprintf(buf, "%s\n", voltage_labels[idx]);
}

static SENSOR_DEVICE_ATTR_RO(in0_input, voltage, 0);
static SENSOR_DEVICE_ATTR_RO(in1_input, voltage, 1);
static SENSOR_DEVICE_ATTR_RO(in2_input, voltage, 2);
static SENSOR_DEVICE_ATTR_RO(in3_input, voltage, 3);
static SENSOR_DEVICE_ATTR_RO(in4_input, voltage, 4);
static SENSOR_DEVICE_ATTR_RO(in5_input, voltage, 5);
static SENSOR_DEVICE_ATTR_RO(in6_input, voltage, 6);

static SENSOR_DEVICE_ATTR_RO(in0_label, voltage_label, 0);
static SENSOR_DEVICE_ATTR_RO(in1_label, voltage_label, 1);
static SENSOR_DEVICE_ATTR_RO(in2_label, voltage_label, 2);
static SENSOR_DEVICE_ATTR_RO(in3_label, voltage_label, 3);
static SENSOR_DEVICE_ATTR_RO(in4_label, voltage_label, 4);
static SENSOR_DEVICE_ATTR_RO(in5_label, voltage_label, 5);
static SENSOR_DEVICE_ATTR_RO(in6_label, voltage_label, 6);


/* ============================================================
 * hwmon attributes
 * ============================================================
 */

static struct attribute *d30_attrs[] = {
	/* temperatures */
	&sensor_dev_attr_temp1_input.dev_attr.attr,
	&sensor_dev_attr_temp2_input.dev_attr.attr,
	&sensor_dev_attr_temp3_input.dev_attr.attr,
	&sensor_dev_attr_temp4_input.dev_attr.attr,
	&sensor_dev_attr_temp5_input.dev_attr.attr,
	&sensor_dev_attr_temp6_input.dev_attr.attr,
	&sensor_dev_attr_temp7_input.dev_attr.attr,

	&sensor_dev_attr_temp1_label.dev_attr.attr,
	&sensor_dev_attr_temp2_label.dev_attr.attr,
	&sensor_dev_attr_temp3_label.dev_attr.attr,
	&sensor_dev_attr_temp4_label.dev_attr.attr,
	&sensor_dev_attr_temp5_label.dev_attr.attr,
	&sensor_dev_attr_temp6_label.dev_attr.attr,
	&sensor_dev_attr_temp7_label.dev_attr.attr,

	/* fans */
	&sensor_dev_attr_fan1_input.dev_attr.attr,
	&sensor_dev_attr_fan2_input.dev_attr.attr,
	&sensor_dev_attr_fan3_input.dev_attr.attr,

	&sensor_dev_attr_fan1_label.dev_attr.attr,
	&sensor_dev_attr_fan2_label.dev_attr.attr,
	&sensor_dev_attr_fan3_label.dev_attr.attr,

	/* voltages */
	&sensor_dev_attr_in0_input.dev_attr.attr,
	&sensor_dev_attr_in1_input.dev_attr.attr,
	&sensor_dev_attr_in2_input.dev_attr.attr,
	&sensor_dev_attr_in3_input.dev_attr.attr,
	&sensor_dev_attr_in4_input.dev_attr.attr,
	&sensor_dev_attr_in5_input.dev_attr.attr,
	&sensor_dev_attr_in6_input.dev_attr.attr,

	&sensor_dev_attr_in0_label.dev_attr.attr,
	&sensor_dev_attr_in1_label.dev_attr.attr,
	&sensor_dev_attr_in2_label.dev_attr.attr,
	&sensor_dev_attr_in3_label.dev_attr.attr,
	&sensor_dev_attr_in4_label.dev_attr.attr,
	&sensor_dev_attr_in5_label.dev_attr.attr,
	&sensor_dev_attr_in6_label.dev_attr.attr,

	NULL
};

static const struct attribute_group d30_group = {
	.attrs = d30_attrs,
};

static const struct attribute_group *d30_groups[] = {
	&d30_group,
	NULL
};


/* ============================================================
 * Module init / exit
 * ============================================================
 */

static int __init d30_init(void)
{
	if (!dmi_check_system(d30_dmi_table)) {
		pr_err(DRVNAME ": unsupported system; refusing to access NCT6681 I/O ports\n");
		return -ENODEV;
	}

	pr_info(DRVNAME ": Lenovo ThinkStation D30 detected via DMI\n");

	if (!request_region(D30_PAGE, 3, DRVNAME)) {
		pr_err(DRVNAME ": I/O ports 0xA00-0xA02 busy\n");
		return -EBUSY;
	}

	hwmon_dev = hwmon_device_register_with_groups(
		NULL, DRVNAME, NULL, d30_groups);

	if (IS_ERR(hwmon_dev)) {
		release_region(D30_PAGE, 3);
		return PTR_ERR(hwmon_dev);
	}

	pr_info(DRVNAME ": ThinkStation D30 NCT6681 sensors registered (read-only)\n");

	return 0;
}

static void __exit d30_exit(void)
{
	hwmon_device_unregister(hwmon_dev);
	release_region(D30_PAGE, 3);

	pr_info(DRVNAME ": unloaded\n");
}

module_init(d30_init);
module_exit(d30_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ThinkStation D30 hwmon");
MODULE_DESCRIPTION("Read-only Lenovo ThinkStation D30 NCT6681 hwmon driver");
