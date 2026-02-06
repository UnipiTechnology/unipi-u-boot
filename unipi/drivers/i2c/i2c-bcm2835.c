// SPDX-License-Identifier: GPL-2.0
/*
 * (C) Copyright 2023 Unipi Technology s.r.o.
 */

#include <errno.h>
#include <log.h>
#include <asm/io.h>
#include <dm.h>
#include <fdtdec.h>
#include <i2c.h>
#include <dm/device_compat.h>

#define LPI2C_TIMEOUT_MS 100

#define BCM2835_I2C_C		0x0
#define BCM2835_I2C_S		0x4
#define BCM2835_I2C_DLEN	0x8
#define BCM2835_I2C_A		0xc
#define BCM2835_I2C_FIFO	0x10
#define BCM2835_I2C_DIV		0x14
#define BCM2835_I2C_DEL		0x18

/*
 * 16-bit field for the number of SCL cycles to wait after rising SCL
 * before deciding the slave is not responding. 0 disables the
 * timeout detection.
 */
#define BCM2835_I2C_CLKT	0x1c

#define BCM2835_I2C_C_READ	BIT(0)
#define BCM2835_I2C_C_CLEAR	BIT(4) /* bits 4 and 5 both clear */
#define BCM2835_I2C_C_ST	BIT(7)
#define BCM2835_I2C_C_INTD	BIT(8)
#define BCM2835_I2C_C_INTT	BIT(9)
#define BCM2835_I2C_C_INTR	BIT(10)
#define BCM2835_I2C_C_I2CEN	BIT(15)

#define BCM2835_I2C_S_TA	BIT(0)
#define BCM2835_I2C_S_DONE	BIT(1)
#define BCM2835_I2C_S_TXW	BIT(2)
#define BCM2835_I2C_S_RXR	BIT(3)
#define BCM2835_I2C_S_TXD	BIT(4)
#define BCM2835_I2C_S_RXD	BIT(5)
#define BCM2835_I2C_S_TXE	BIT(6)
#define BCM2835_I2C_S_RXF	BIT(7)
#define BCM2835_I2C_S_ERR	BIT(8)
#define BCM2835_I2C_S_CLKT	BIT(9)
#define BCM2835_I2C_S_LEN	BIT(10) /* Fake bit for SW error reporting */

#define BCM2835_I2C_FEDL_SHIFT	16
#define BCM2835_I2C_REDL_SHIFT	0

#define BCM2835_I2C_CDIV_MIN	0x0002
#define BCM2835_I2C_CDIV_MAX	0xFFFE

struct bcm2835_i2c_bus {
	void __iomem *regs;
	struct udevice *bus;
	int    index;
	struct clk *bus_clk;
	u32  speed;
	u8* buf;
	int remain;
	u32 flags;
	unsigned int debug_num;
	unsigned int debug_num_msgs;
};


static inline void bcm2835_i2c_writel(struct bcm2835_i2c_bus *i2c_bus,
				      u32 reg, u32 val)
{
	writel(val, i2c_bus->regs + reg);
}

static inline u32 bcm2835_i2c_readl(struct bcm2835_i2c_bus *i2c_bus, u32 reg)
{
	return readl(i2c_bus->regs + reg);
}

/* Weak linked function for overridden by some SoC power function */
int __weak init_i2c_power(unsigned i2c_num)
{
	return 0;
}

static void bcm2835_fill_txfifo(struct bcm2835_i2c_bus *i2c_bus)
{
	u32 val;

	while (i2c_bus->remain) {
		val = bcm2835_i2c_readl(i2c_bus, BCM2835_I2C_S);
		if (!(val & BCM2835_I2C_S_TXD))
			break;
		bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_FIFO, *i2c_bus->buf);
		i2c_bus->buf++;
		i2c_bus->remain--;
	}
}

static void bcm2835_drain_rxfifo(struct bcm2835_i2c_bus *i2c_bus)
{
	u32 val;

	while (i2c_bus->remain) {
		val = bcm2835_i2c_readl(i2c_bus, BCM2835_I2C_S);
		if (!(val & BCM2835_I2C_S_RXD))
			break;
		*i2c_bus->buf = bcm2835_i2c_readl(i2c_bus, BCM2835_I2C_FIFO);
		i2c_bus->buf++;
		i2c_bus->remain--;
	}
}


static void bcm2835_i2c_start_transfer(struct bcm2835_i2c_bus *i2c_bus, int chip, int len, u32 flags, bool last_msg)
{
	u32 c = BCM2835_I2C_C_ST | BCM2835_I2C_C_I2CEN;

	if (flags & I2C_M_RD)
		c |= BCM2835_I2C_C_READ/* | BCM2835_I2C_C_INTR*/;

	bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_C, BCM2835_I2C_C_CLEAR | BCM2835_I2C_C_I2CEN);
	bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_S, BCM2835_I2C_S_CLKT |
			   BCM2835_I2C_S_ERR | BCM2835_I2C_S_DONE);

	bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_A, chip);
	bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_DLEN, len);
	bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_C, c);
	i2c_bus->flags = flags;
}

/*
 * Note about I2C_C_CLEAR on error:
 * The I2C_C_CLEAR on errors will take some time to resolve -- if you were in
 * non-idle state and I2C_C_READ, it sets an abort_rx flag and runs through
 * the state machine to send a NACK and a STOP. Since we're setting CLEAR
 * without I2CEN, that NACK will be hanging around queued up for next time
 * we start the engine.
 */

static u32 bcm2835_i2c_transfer(struct bcm2835_i2c_bus *i2c_bus)
{
	u32 val, err;
	ulong start_time;

	start_time = get_timer(0);
	while (true) {
		val = bcm2835_i2c_readl(i2c_bus, BCM2835_I2C_S);

		err = val & (BCM2835_I2C_S_CLKT | BCM2835_I2C_S_ERR);
		if (err) {
			if ((err & BCM2835_I2C_S_CLKT) ||
			    (!(val & BCM2835_I2C_S_TA))) goto complete;
		}

		if (val & BCM2835_I2C_S_DONE) {
			if (i2c_bus->flags & I2C_M_RD) {
				bcm2835_drain_rxfifo(i2c_bus);
				val = bcm2835_i2c_readl(i2c_bus, BCM2835_I2C_S);
			}

			if ((val & BCM2835_I2C_S_RXD) || i2c_bus->remain)
				err = BCM2835_I2C_S_LEN;
			else
				err = 0;
			goto complete;
		}

		if (val & BCM2835_I2C_S_TXW) {
			if (!i2c_bus->remain) {
				err = val | BCM2835_I2C_S_LEN;
				goto complete;
			}

			bcm2835_fill_txfifo(i2c_bus);

		} else if (val & BCM2835_I2C_S_RXR) {
			if (!i2c_bus->remain) {
				err = val | BCM2835_I2C_S_LEN;
				goto complete;
			}

			bcm2835_drain_rxfifo(i2c_bus);
		}

		if (get_timer(start_time) > LPI2C_TIMEOUT_MS) {
			debug("i2c: wait for tx ready: timeout\n");
			goto complete;
		}
	}

complete:
	bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_C, BCM2835_I2C_C_CLEAR | BCM2835_I2C_C_I2CEN);

	bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_S, BCM2835_I2C_S_CLKT |
			   BCM2835_I2C_S_ERR | BCM2835_I2C_S_DONE);
	bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_C, 0);

	return err;
}


u32 __weak imx_get_i2cclk(u32 i2c_num)
{
	return 0;
}

static int bus_i2c_set_bus_speed(struct udevice *bus, int speed)
{
	struct bcm2835_i2c_bus *i2c_bus = dev_get_priv(bus);
	if (speed == 400000) {
		bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_DIV, 1500);
	} else {
		bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_DIV, 6000);
	}
	return 0;
}

static int bus_i2c_init(struct udevice *bus, int speed)
{
	int ret;
	struct bcm2835_i2c_bus *i2c_bus = dev_get_priv(bus);

	/*
	 * Disable the hardware clock stretching timeout. SMBUS
	 * specifies a limit for how long the device can stretch the
	 * clock, but core I2C doesn't.
	 */
	bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_CLKT, 0);
	bcm2835_i2c_writel(i2c_bus, BCM2835_I2C_C, 0);


	ret = bus_i2c_set_bus_speed(bus, speed);
	debug("i2c : controller bus %d, speed %d:\n", dev_seq(bus), speed);

	return ret;
}

static int i2c_bcm2835_probe_chip(struct udevice *bus, u32 chip,
				u32 chip_flags)
{
	int result;
	u32 reg;
	struct bcm2835_i2c_bus *i2c_bus = dev_get_priv(bus);

	bcm2835_i2c_start_transfer(i2c_bus, chip, 0, 0, true);
	i2c_bus->remain = 0;
	i2c_bus->buf = (u8*)&reg;
	result = bcm2835_i2c_transfer(i2c_bus);

	bus_i2c_init(bus, I2C_SPEED_STANDARD_RATE);
	return result;
}

static int i2c_bcm2835_xfer(struct udevice *bus, struct i2c_msg *msg, int nmsgs)
{
	int ret = 0;
	struct bcm2835_i2c_bus *i2c_bus = dev_get_priv(bus);

	for (; nmsgs > 0; nmsgs--, msg++) {
		debug("i2c_xfer: chip=0x%x, len=0x%x\n", msg->addr, msg->len);
		bcm2835_i2c_start_transfer(i2c_bus, msg->addr, msg->len, msg->flags, true);
		i2c_bus->remain = msg->len;
		i2c_bus->buf = msg->buf;
		ret = bcm2835_i2c_transfer(i2c_bus);
		if (ret)
				break;
	}

	if (ret)
		debug("i2c_write: error sending\n");
	return ret;
}

static int i2c_bcm2835_set_bus_speed(struct udevice *bus, unsigned int speed)
{
	return bus_i2c_set_bus_speed(bus, speed);
}

__weak int enable_i2c_clk(unsigned char enable, unsigned int i2c_num)
{
	return 0;
}

static int i2c_bcm2835_probe(struct udevice *bus)
{
	struct bcm2835_i2c_bus *i2c_bus = dev_get_priv(bus);
	fdt_addr_t addr;
	int ret;

	addr = dev_read_addr(bus);
	if (addr == FDT_ADDR_T_NONE)
		return -EINVAL;

	i2c_bus->regs = (void*)addr;
	i2c_bus->index = dev_seq(bus);
	i2c_bus->bus = bus;

	/* power up i2c resource */
	ret = init_i2c_power(dev_seq(bus));
	if (ret) {
		debug("init_i2c_power err = %d\n", ret);
		return ret;
	}
	ret = bus_i2c_init(bus, I2C_SPEED_STANDARD_RATE);
	if (ret < 0)
		return ret;

	debug("i2c : controller bus %d at 0x%lx , speed %d: ",
	      dev_seq(bus), (ulong) i2c_bus->regs,
	      i2c_bus->speed);

	return 0;
}

static const struct dm_i2c_ops i2c_bcm2835_ops = {
	.xfer		= i2c_bcm2835_xfer,
	.probe_chip	= i2c_bcm2835_probe_chip,
	.set_bus_speed	= i2c_bcm2835_set_bus_speed,
};

static const struct udevice_id i2c_bcm2835_ids[] = {
	{ .compatible = "brcm,bcm2711-i2c", },
	{ .compatible = "brcm,bcm2835-i2c", },
	{}
};

U_BOOT_DRIVER(i2c_bcm2835) = {
	.name = "i2c_bcm2835",
	.id = UCLASS_I2C,
	.of_match = i2c_bcm2835_ids,
	.probe = i2c_bcm2835_probe,
	.priv_auto = sizeof(struct bcm2835_i2c_bus),
	.ops = &i2c_bcm2835_ops,
};
