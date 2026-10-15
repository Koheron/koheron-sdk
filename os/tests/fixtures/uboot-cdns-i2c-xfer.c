// SPDX-License-Identifier: GPL-2.0+
/* Unmodified Xilinx U-Boot 2026.1 transfer loop. */
static int cdns_i2c_xfer(struct udevice *dev, struct i2c_msg *msg,
			 int nmsgs)
{
	struct i2c_cdns_bus *i2c_bus = dev_get_priv(dev);
	int ret = 0;
	int count;
	bool hold_quirk;
	struct i2c_msg *message = msg;
	int num_msgs = nmsgs;

	hold_quirk = !!(i2c_bus->quirks & CDNS_I2C_BROKEN_HOLD_BIT);

	if (nmsgs > 1) {
		/*
		 * This controller does not give completion interrupt after a
		 * master receive message if HOLD bit is set (repeated start),
		 * resulting in SW timeout. Hence, if a receive message is
		 * followed by any other message, an error is returned
		 * indicating that this sequence is not supported.
		 */
		for (count = 0; (count < nmsgs - 1) && hold_quirk; count++) {
			if (msg[count].flags & I2C_M_RD) {
				printf("Can't do repeated start after a receive message\n");
				return -EOPNOTSUPP;
			}
		}

		i2c_bus->hold_flag = 1;
		setbits_le32(&i2c_bus->regs->control, CDNS_I2C_CONTROL_HOLD);
	} else {
		i2c_bus->hold_flag = 0;
	}

	debug("i2c_xfer: %d messages\n", nmsgs);
	for (u8 retry = 0; retry < CDNS_I2C_ARB_LOST_MAX_RETRIES &&
	     nmsgs > 0;) {
		debug("i2c_xfer: chip=0x%x, len=0x%x\n", msg->addr, msg->len);
		if (msg->flags & I2C_M_RD) {
			ret = cdns_i2c_read_data(i2c_bus, msg->addr, msg->buf,
						 msg->len);
		} else {
			ret = cdns_i2c_write_data(i2c_bus, msg->addr, msg->buf,
						  msg->len);
		}

		if (ret == -ETIMEDOUT)
			return ret;

		if (ret == -EAGAIN) {
			msg = message;
			nmsgs = num_msgs;
			retry++;
			printf("%s,arbitration lost, retrying:%d\n", __func__,
			       retry);
			continue;
		}
		nmsgs--;
		msg++;
		if (ret) {
			debug("i2c_write: error sending\n");
			return -EREMOTEIO;
		}
	}

	return ret;
}

