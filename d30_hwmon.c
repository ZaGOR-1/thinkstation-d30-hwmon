#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/io.h>
#include <linux/ioport.h>
#include <linux/hwmon.h>
#include <linux/hwmon-sysfs.h>
#include <linux/sysfs.h>
#include <linux/mutex.h>
#include <linux/dmi.h>

#define DRVNAME "d30_hwmon"

#define D30_SIO_INDEX       0x2e
#define D30_SIO_DATA        0x2f
#define D30_SIO_LDN_HWM     0x0b
#define D30_SIO_DEVID_HI    0xb2

#define D30_SIO_REG_LDSEL   0x07
#define D30_SIO_REG_DEVID   0x20
#define D30_SIO_REG_ENABLE  0x30
#define D30_SIO_REG_ADDR    0x60

#define D30_PAGE   0xA00
#define D30_INDEX  0xA01
#define D30_DATA   0xA02

#define D30_INTRUSION_PORT  0x466
#define D30_INTRUSION_MASK  BIT(0)

static DEFINE_MUTEX(d30_lock);
static struct device *hwmon_dev;
static bool intrusion_region_claimed;
static bool research_dump;

module_param(research_dump, bool, 0400);
MODULE_PARM_DESC(research_dump,
                 "Enable read-only NCT6681 fan-control research dump/readout");

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


/* ============================================================
 * Super-I/O detection
 *
 * Lenovo's A3KT70A BIOS checks for an NCT6681-class device by
 * comparing the high byte of the Super-I/O device ID with 0xB2.
 * It then verifies that logical device 0x0B is enabled at 0xA00.
 * ============================================================
 */

static inline void d30_superio_outb(u8 reg, u8 value)
{
	outb(reg, D30_SIO_INDEX);
	outb(value, D30_SIO_DATA);
}

static inline u8 d30_superio_inb(u8 reg)
{
	outb(reg, D30_SIO_INDEX);
	return inb(D30_SIO_DATA);
}

static int d30_detect_nct6681(void)
{
	u16 device_id, hwm_base;
	u8 enabled;

	if (!request_muxed_region(D30_SIO_INDEX, 2, DRVNAME)) {
		pr_err(DRVNAME ": Super-I/O ports 0x2e-0x2f are busy\n");
		return -EBUSY;
	}

	/* Enter Nuvoton extended-function mode. */
	outb(0x87, D30_SIO_INDEX);
	outb(0x87, D30_SIO_INDEX);

	device_id = (d30_superio_inb(D30_SIO_REG_DEVID) << 8) |
		    d30_superio_inb(D30_SIO_REG_DEVID + 1);

	d30_superio_outb(D30_SIO_REG_LDSEL, D30_SIO_LDN_HWM);
	enabled = d30_superio_inb(D30_SIO_REG_ENABLE);
	hwm_base = (d30_superio_inb(D30_SIO_REG_ADDR) << 8) |
		   d30_superio_inb(D30_SIO_REG_ADDR + 1);

	/* Leave extended-function mode. */
	outb(0xaa, D30_SIO_INDEX);
	release_region(D30_SIO_INDEX, 2);

	if ((device_id >> 8) != D30_SIO_DEVID_HI) {
		pr_err(DRVNAME ": unexpected Super-I/O device ID 0x%04x\n",
		       device_id);
		return -ENODEV;
	}

	if (!(enabled & BIT(0))) {
		pr_err(DRVNAME ": NCT6681 hardware-monitor logical device is disabled\n");
		return -ENODEV;
	}

	if (hwm_base != D30_PAGE) {
		pr_err(DRVNAME ": unexpected NCT6681 EC base 0x%04x (expected 0x%03x)\n",
		       hwm_base, D30_PAGE);
		return -ENODEV;
	}

	pr_info(DRVNAME ": NCT6681 device ID 0x%04x, EC base 0x%04x\n",
		device_id, hwm_base);

	return 0;
}


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
 * Fan-control research support
 *
 * This path is intentionally READ-ONLY. The register ranges below
 * were recovered from Lenovo A3KT70A fancontrolpei firmware.
 *
 * Access still selects PAGE and INDEX, exactly like normal sensor
 * reads, but D30_DATA (0xA02) is never written.
 * ============================================================
 */

struct d30_research_block {
	const char *name;
	u8 page;
	u8 start;
	u8 end;
};

static const struct d30_research_block d30_research_blocks[] = {
	{ "Fan1", 0x07, 0x00, 0x17 },
	{ "Fan3", 0x07, 0x30, 0x47 },
	{ "Fan7", 0x07, 0x90, 0xa7 },
};

struct d30_research_reg {
	const char *name;
	u8 page;
	u8 reg;
};

static const struct d30_research_reg d30_research_regs[] = {
	{ "config_5a", 0x01, 0x5a },
	{ "config_5b", 0x01, 0x5b },
	{ "config_5c", 0x01, 0x5c },
	{ "config_f8", 0x01, 0xf8 },
	{ "status_00", 0x06, 0x00 },
};

static const struct d30_research_block d30_research_candidate_blocks[] = {
	{ "pwm_current", 0x01, 0x60, 0x67 },
	{ "fanout_cfg", 0x01, 0xd0, 0xd7 },
	{ "pwm_write", 0x0a, 0x28, 0x2f },

	/* Lenovo A3KT70A fancontrolpei BIOS-confirmed ranges. */
	{ "bios_sel", 0x01, 0xc0, 0xc7 },
	{ "bios_06_08", 0x06, 0x08, 0x0a },
	{ "bios_06_0c", 0x06, 0x0c, 0x0f },
	{ "bios_06_38", 0x06, 0x38, 0x3a },
	{ "bios_06_3c", 0x06, 0x3c, 0x3f },
	{ "bios_06_60", 0x06, 0x60, 0x77 },

	/* Research4: bounded live monitor-page scan. */
	{ "monitor_01", 0x01, 0x10, 0x5f },

	/* Research5: bounded runtime/status probe. */
	{ "runtime_01", 0x01, 0x68, 0x80 },
};

static const struct d30_research_reg d30_research_candidate_regs[] = {
	{ "fan_cfg_ctrl", 0x0a, 0x01 },

	{ "bios_5a", 0x01, 0x5a },
	{ "bios_5b", 0x01, 0x5b },
	{ "bios_5c", 0x01, 0x5c },
	{ "bios_f8", 0x01, 0xf8 },
	{ "bios_f9", 0x01, 0xf9 },
	{ "bios_status", 0x06, 0x00 },
};

static ssize_t research_candidates_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t len = 0;
	unsigned int i, reg;

	for (i = 0; i < ARRAY_SIZE(d30_research_candidate_blocks); i++) {
		const struct d30_research_block *block =
			&d30_research_candidate_blocks[i];

		len += sysfs_emit_at(buf, len,
				    "%s page=0x%02x regs=0x%02x-0x%02x:",
				    block->name, block->page,
				    block->start, block->end);

		for (reg = block->start; reg <= block->end; reg++)
			len += sysfs_emit_at(buf, len, " %02x",
					     d30_read(block->page, reg));

		len += sysfs_emit_at(buf, len, "\n");
	}

	for (i = 0; i < ARRAY_SIZE(d30_research_candidate_regs); i++) {
		const struct d30_research_reg *candidate =
			&d30_research_candidate_regs[i];

		len += sysfs_emit_at(buf, len,
				    "%s page=0x%02x reg=0x%02x: %02x\n",
				    candidate->name, candidate->page,
				    candidate->reg,
				    d30_read(candidate->page, candidate->reg));
	}

	return len;
}

static DEVICE_ATTR_RO(research_candidates);


static void d30_research_dump_block(const struct d30_research_block *block)
{
	u8 data[0x18];
	unsigned int i;
	unsigned int len = block->end - block->start + 1;

	if (WARN_ON(len > ARRAY_SIZE(data)))
		return;

	for (i = 0; i < len; i++)
		data[i] = d30_read(block->page, block->start + i);

	pr_info(DRVNAME ": research %s page 0x%02x regs 0x%02x-0x%02x\n",
		block->name, block->page, block->start, block->end);

	for (i = 0; i < len; i += 8) {
		unsigned int count = min_t(unsigned int, 8, len - i);

		pr_info(DRVNAME ": research %s reg 0x%02x: %*ph\n",
			block->name, block->start + i,
			(int)count, &data[i]);
	}
}

static void d30_research_dump_registers(void)
{
	unsigned int i;

	pr_info(DRVNAME ": research dump BEGIN (read-only)\n");

	for (i = 0; i < ARRAY_SIZE(d30_research_blocks); i++)
		d30_research_dump_block(&d30_research_blocks[i]);

	for (i = 0; i < ARRAY_SIZE(d30_research_regs); i++) {
		const struct d30_research_reg *reg = &d30_research_regs[i];
		u8 value = d30_read(reg->page, reg->reg);

		pr_info(DRVNAME ": research %s page 0x%02x reg 0x%02x = 0x%02x\n",
			reg->name, reg->page, reg->reg, value);
	}

	pr_info(DRVNAME ": research dump END\n");
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
	"System temperature1",
	"System temperature2",
	"System temperature3",
	"System temperature4",
	"CPU Package (EC mirror)",
	"System temperature7",
	"System temperature8",
};

static ssize_t temp_show(struct device *dev,
			 struct device_attribute *attr,
			 char *buf)
{
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	unsigned int idx = sattr->index;
	s16 raw;

	if (idx >= ARRAY_SIZE(temp_regs))
		return -EINVAL;

	raw = (s16)d30_read16(1, temp_regs[idx]);

	/* NCT668x temperature format: signed 16-bit value in 0.5 C units. */
	return sprintf(buf, "%d\n", (raw / 128) * 500);
}

static ssize_t temp_raw_low_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct sensor_device_attribute *sattr = to_sensor_dev_attr(attr);
	unsigned int idx = sattr->index;
	u8 raw_low;

	if (idx >= ARRAY_SIZE(temp_regs))
		return -EINVAL;

	raw_low = d30_read(1, temp_regs[idx] + 1);

	return sprintf(buf, "%u\n", raw_low);
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

/*
 * Diagnostic attributes. Lenovo's BIOS displays only the high byte as
 * whole degrees Celsius. The adjacent low byte is exposed without applying
 * an unverified conversion so its behaviour can be studied safely.
 */
static SENSOR_DEVICE_ATTR_RO(temp1_raw_low, temp_raw_low, 0);
static SENSOR_DEVICE_ATTR_RO(temp2_raw_low, temp_raw_low, 1);
static SENSOR_DEVICE_ATTR_RO(temp3_raw_low, temp_raw_low, 2);
static SENSOR_DEVICE_ATTR_RO(temp4_raw_low, temp_raw_low, 3);
static SENSOR_DEVICE_ATTR_RO(temp5_raw_low, temp_raw_low, 4);
static SENSOR_DEVICE_ATTR_RO(temp6_raw_low, temp_raw_low, 5);
static SENSOR_DEVICE_ATTR_RO(temp7_raw_low, temp_raw_low, 6);

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
	"Fan1 Speed",
	"Fan3 Speed",
	"Fan7 Speed",
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
 * Chassis intrusion
 *
 * Lenovo's ChassisIntrusionS3 BIOS module reads I/O port 0x466 bit 0.
 * Reading does not acknowledge or clear the latched alarm. Clearing remains
 * under BIOS control.
 * ============================================================
 */

static ssize_t intrusion_show(struct device *dev,
			      struct device_attribute *attr,
			      char *buf)
{
	unsigned int alarm;

	if (!intrusion_region_claimed)
		return -ENODEV;

	alarm = !!(inb(D30_INTRUSION_PORT) & D30_INTRUSION_MASK);

	return sprintf(buf, "%u\n", alarm);
}

static SENSOR_DEVICE_ATTR_RO(intrusion0_alarm, intrusion, 0);


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

	&sensor_dev_attr_temp1_raw_low.dev_attr.attr,
	&sensor_dev_attr_temp2_raw_low.dev_attr.attr,
	&sensor_dev_attr_temp3_raw_low.dev_attr.attr,
	&sensor_dev_attr_temp4_raw_low.dev_attr.attr,
	&sensor_dev_attr_temp5_raw_low.dev_attr.attr,
	&sensor_dev_attr_temp6_raw_low.dev_attr.attr,
	&sensor_dev_attr_temp7_raw_low.dev_attr.attr,

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

	/* research-only candidate registers; hidden unless research_dump=1 */
	&dev_attr_research_candidates.attr,

	/* chassis intrusion */
	&sensor_dev_attr_intrusion0_alarm.dev_attr.attr,

	NULL
};

static umode_t d30_is_visible(struct kobject *kobj,
			      struct attribute *attr,
			      int index)
{
	if (attr == &dev_attr_research_candidates.attr && !research_dump)
		return 0;

	if (attr == &sensor_dev_attr_intrusion0_alarm.dev_attr.attr &&
	    !intrusion_region_claimed)
		return 0;

	return attr->mode;
}

static const struct attribute_group d30_group = {
	.attrs = d30_attrs,
	.is_visible = d30_is_visible,
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
	int err;

	if (!dmi_check_system(d30_dmi_table)) {
		pr_err(DRVNAME ": unsupported system; refusing to access NCT6681 I/O ports\n");
		return -ENODEV;
	}

	pr_info(DRVNAME ": Lenovo ThinkStation D30 detected via DMI\n");

	err = d30_detect_nct6681();
	if (err)
		return err;

	if (!request_region(D30_PAGE, 3, DRVNAME)) {
		pr_err(DRVNAME ": I/O ports 0xA00-0xA02 busy\n");
		return -EBUSY;
	}

	if (research_dump)
		d30_research_dump_registers();

	if (request_region(D30_INTRUSION_PORT, 1, DRVNAME)) {
		intrusion_region_claimed = true;
	} else {
		pr_warn(DRVNAME ": chassis-intrusion port 0x466 busy; alarm will not be exposed\n");
	}

	hwmon_dev = hwmon_device_register_with_groups(
		NULL, DRVNAME, NULL, d30_groups);

	if (IS_ERR(hwmon_dev)) {
		if (intrusion_region_claimed) {
			release_region(D30_INTRUSION_PORT, 1);
			intrusion_region_claimed = false;
		}
		release_region(D30_PAGE, 3);
		return PTR_ERR(hwmon_dev);
	}

	pr_info(DRVNAME ": ThinkStation D30 NCT6681 sensors registered (read-only, BIOS-verified map)\n");

	return 0;
}

static void __exit d30_exit(void)
{
	hwmon_device_unregister(hwmon_dev);
	if (intrusion_region_claimed) {
		release_region(D30_INTRUSION_PORT, 1);
		intrusion_region_claimed = false;
	}
	release_region(D30_PAGE, 3);

	pr_info(DRVNAME ": unloaded\n");
}

module_init(d30_init);
module_exit(d30_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("ThinkStation D30 hwmon");
MODULE_DESCRIPTION("Read-only Lenovo ThinkStation D30 NCT6681 hwmon driver using BIOS-verified registers");
MODULE_VERSION("1.5-research5");
