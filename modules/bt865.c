/*
 *  BT865A - Brook Tree BT865A video encoder driver version 0.0.4
 *
 * Henrik Johannson <henrikjo@post.utfors.se>
 * As modified by Chris C. Hoover <cchoover@home.com>
 *
 * Modified by Luis Correia <lfcorreia@users.sourceforge.net>
 * added rgb_mode (requires hacking DXR3 hardware)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <linux/module.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/videodev2.h>
#include <media/v4l2-device.h>
#include <media/v4l2-chip-ident.h>

MODULE_DESCRIPTION("Brook Tree BT865A video encoder driver");
MODULE_LICENSE("GPL");

static int debug;
module_param(debug, int, 0);
MODULE_PARM_DESC(debug, "Debug level (0-1)");

/* ----------------------------------------------------------------------- */

struct bt865 {
	struct v4l2_subdev sd;
	v4l2_std_id norm;
};

static inline struct bt865 *to_bt865(struct v4l2_subdev *sd)
{
	return container_of(sd, struct bt865, sd);
}

static inline int bt865_write(struct v4l2_subdev *sd, u8 reg, u8 value)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return i2c_smbus_write_byte_data(client, reg, value);
}

static inline int bt865_read(struct v4l2_subdev *sd, u8 reg)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	return i2c_smbus_read_byte_data(client, reg);
}

static int bt865_write_block(struct v4l2_subdev *sd,
		     const u8 *data, unsigned int len)
{
	int ret = -1;
	u8 reg;

	while (len >= 2) {
		reg = *data++;
		ret = bt865_write(sd, reg, *data++);
		if (ret < 0)
			break;
		len -= 2;
	}

	return ret;
}

/* ----------------------------------------------------------------------- */

#define RESET		0xa6
#define DACOFF		0xbc
#define EACTIVE		0xce

static const unsigned char init_ntsc[] = {
	0xbc, 0xc1,
	0xce, 0x4,
};

static const unsigned char init_pal[] = {
	0xcc, 0xe4,
	0xce, 0x2,
};

/* ----------------------------------------------------------------------- */

static int bt865_s_std_output(struct v4l2_subdev *sd, v4l2_std_id std)
{
	struct bt865 *encoder = to_bt865(sd);

	if (std & V4L2_STD_NTSC) {
		bt865_write_block(sd, init_ntsc, sizeof(init_ntsc));
	} else if (std & V4L2_STD_PAL) {
		bt865_write_block(sd, init_pal, sizeof(init_pal));
	} else {
		v4l2_dbg(1, debug, sd, "illegal norm: %llx\n",
				(unsigned long long)std);
		return -EINVAL;
	}
	v4l2_dbg(1, debug, sd, "switched to %llx\n", (unsigned long long)std);
	encoder->norm = std;
	return 0;
}

static int bt865_g_chip_ident(struct v4l2_subdev *sd,
		     struct v4l2_dbg_chip_ident *chip)
{
	struct i2c_client *client = v4l2_get_subdevdata(sd);

	/* reserve id */
	return v4l2_chip_ident_i2c_client(client, chip, 70000, 0);
}

static int bt865_s_power(struct v4l2_subdev *sd, int on)
{
	u8 val = bt865_read(sd, 0xce);

	if (on) {
		bt865_write(sd, DACOFF, val & ~0x8);
		bt865_write(sd, EACTIVE, val | 0x2);
	} else {
		bt865_write(sd, EACTIVE, val & ~0x2);
		bt865_write(sd, DACOFF, val | 0x8);
	}

	return 0;
}

/* ----------------------------------------------------------------------- */

static const struct v4l2_subdev_core_ops bt865_core_ops = {
	.g_chip_ident = bt865_g_chip_ident,
	.s_power = bt865_s_power,
};

static const struct v4l2_subdev_video_ops bt865_video_ops = {
	.s_std_output = bt865_s_std_output,
};

static const struct v4l2_subdev_ops bt865_ops = {
	.core = &bt865_core_ops,
	.video = &bt865_video_ops,
};

/* ----------------------------------------------------------------------- */

static int bt865_probe(struct i2c_client *client,
		       const struct i2c_device_id *id)
{
	struct bt865 *encoder;
	struct v4l2_subdev *sd;

	/* Check if the adapter supports the needed features */
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_SMBUS_BYTE_DATA))
		return -ENODEV;

	v4l_info(client, "chip found @ 0x%x (%s)\n",
			client->addr << 1, client->adapter->name);

	encoder = kmalloc(sizeof(struct bt865), GFP_KERNEL);
	if (encoder == NULL)
		return -ENOMEM;
	sd = &encoder->sd;
	v4l2_i2c_subdev_init(sd, client, &bt865_ops);
	encoder->norm = V4L2_STD_PAL;

	/* reset all registers to 0 */
	bt865_write(sd, RESET, 0x80);

	return 0;
}

static int bt865_remove(struct i2c_client *client)
{
	struct v4l2_subdev *sd = i2c_get_clientdata(client);

	v4l2_device_unregister_subdev(sd);
	kfree(to_bt865(sd));
	return 0;
}

/* ----------------------------------------------------------------------- */

static struct i2c_device_id bt865_id[] = {
	{ "bt865", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bt865_id);

/* This is the driver that will be inserted */
static struct i2c_driver bt865_driver = {
	.driver = {
		.owner	=	THIS_MODULE,
		.name	=	"bt865",
	},
	.id_table =		bt865_id,
	.probe =		&bt865_probe,
	.remove =		&bt865_remove,
};

int __init init_bt865(void)
{
	return i2c_add_driver(&bt865_driver);
}

void __exit exit_bt865(void)
{
	i2c_del_driver(&bt865_driver);
}

module_init(init_bt865);
module_exit(exit_bt865);
