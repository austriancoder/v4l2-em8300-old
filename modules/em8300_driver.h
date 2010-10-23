/*
 * em8300_driver.h
 *
 * Copyright (C) 2000 Henrik Johansson <lhj@users.sourceforge.net>
 *           (C) 2000 Ze'ev Maor <zeev@users.sourceforge.net>
 *           (C) 2001 Rick Haines <rick@kuroyi.net>
 *           (C) 2001 Edward Salley <drawdeyellas@hotmail.com>
 *           (C) 2001 Jeremy T. Braun <jtbraun@mmit.edu>
 *           (C) 2001 Ralph Zimmermann <rz@ooe.net>
 *           (C) 2001 Daniel Chassot <Daniel.Chassot@vibro-meter.com>
 *           (C) 2002 Michael Hunold <michael@mihu.de>
 *           (C) 2002-2003 David Holm <mswitch@users.sourceforge.net>
 *           (C) 2003-2008 Nicolas Boullis <nboullis@debian.org>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef EM8300_DRIVER_H
#define EM8300_DRIVER_H

#define EM8300_MAX 4

#define EM8300_MAJOR 121
#define EM8300_LOGNAME "em8300"
extern int major;

#include <linux/version.h>
#include <linux/types.h> /* ulong, uint32_t */
#include <linux/i2c.h> /* struct i2c_adapter */
#include <linux/i2c-algo-bit.h> /* struct i2c_algo_bit_data */
#include <linux/time.h> /* struct timeval */
#include <linux/wait.h> /* wait_queue_head_t */
#include <linux/list.h> /* struct list_head */
#include <linux/semaphore.h> /* struct semaphore */

#if defined(CONFIG_SND) || defined(CONFIG_SND_MODULE)
#define snd_card_t struct snd_card
#endif

struct dicom_s {
	int luma;
	int chroma;
	int frametop;
	int framebottom;
	int frameleft;
	int frameright;
	int visibletop;
	int visiblebottom;
	int visibleleft;
	int visibleright;
	int tvout;
};

struct displaybuffer_info_s {
	int xsize;
	int ysize;
	int xsize2;
	int flag1,flag2;
	int buffer1;
	int buffer2;
	int unk_present;
	int unknown1;
	int unknown2;
	int unknown3;
};

struct em8300_audio_s {
	int channels;
	int format;
	int speed;
	int slotsize;
	int enable_bits;
};

struct em8300_model_config_s {
	int use_bt865;
	int dicom_other_pal;
	int dicom_fix;
	int dicom_control;
	int bt865_ucode_timeout;
	int activate_loopback;
};

struct adv717x_model_config_s {
	int pixelport_16bit;
	int pixelport_other_pal;
	int pixeldata_adjust_ntsc;
	int pixeldata_adjust_pal;
};

struct bt865_model_config_s {
};

struct em8300_config_s {
	struct em8300_model_config_s model;
	struct adv717x_model_config_s adv717x_model;
	struct bt865_model_config_s bt865_model;
};

struct em8300_s
{
	char name[40];
	
	int chip_revision;
	int pci_revision;
	
	int inuse[4];
	int nonblock[4];
	int ucodeloaded;
	
	struct pci_dev *dev;
	ulong adr;
	volatile unsigned *mem;
	ulong memsize;
	
	int playmode;

#if defined(CONFIG_SND) || defined(CONFIG_SND_MODULE)
	snd_card_t *alsa_card;
#endif

	/* Fifos */
	struct fifo_s *mvfifo;
	struct fifo_s *mafifo;
	struct fifo_s *spfifo;
	int mtrr_reg;
	
	/* DICOM */
	int dicom_vertoffset;
	int dicom_horizoffset;
	int dicom_brightness;
	int dicom_contrast;
	int dicom_saturation;
	int dicom_tvout;
	struct displaybuffer_info_s dbuf_info;
	
	/* I2C */
	int i2c_pin_reg;
	int i2c_oe_reg;
	struct i2c_adapter i2c_adap[2];
	struct i2c_algo_bit_data i2c_algo[2];
	struct i2c_client i2c_client;
	
	/* different between revision 1 and revision 2 boards */
	int mystery_divisor;
	
	/* I2C clients */
	int encoder_type;
	struct i2c_client *encoder;
	struct i2c_client *eeprom;
	
	/* Microcode registers */
	unsigned ucode_regs[MAX_UCODE_REGISTER];
	int var_ucode_reg1; /* These are registers that differ */
	int var_ucode_reg2; /* between versions 1 and 2 of the board */
	int var_ucode_reg3; /* " */
	
	/* Interrupt */
	unsigned irqmask;
	
	/* Clockgenerator */
	int clockgen;
	int clockgen_overlaymode;
	int clockgen_tvmode;
	
	/* Timing measurement */
	struct timeval tv, last_status_time;
	long irqtimediff;
	int irqcount;
	int frames;
	int scr;
	
	/* Audio */
	struct em8300_audio_s audio;
	int audio_mode;
        int pcm_mode;
	int dsp_num;
	/* Channel status for S/PDIF */
	unsigned int channel_status_pos;
	unsigned char channel_status[24];
	enum { NONE, OSS, ALSA } audio_driver_style;
	struct semaphore audio_driver_style_lock;

	/* Video */
	int video_mode;
	int video_playmode;
	int aspect_ratio;
	int zoom;
	uint32_t video_pts;
	uint32_t video_lastpts;
	int video_ptsvalid,video_offset,video_count;
	int video_ptsfifo_ptr;
#if LINUX_VERSION_CODE < 0x020314    
	struct wait_queue *video_ptsfifo_wait;
	struct wait_queue *vbi_wait;
#else
	wait_queue_head_t video_ptsfifo_wait;
	wait_queue_head_t vbi_wait;
#endif
	int video_ptsfifo_waiting;
	int video_first;
	int var_video_value;
	
	/* Sub Picture */
	int sp_pts, sp_ptsvalid, sp_count;
	int sp_ptsfifo_ptr;
#if LINUX_VERSION_CODE < 0x020314    
	struct wait_queue *sp_ptsfifo_wait;
#else
	wait_queue_head_t sp_ptsfifo_wait;
#endif
	int sp_ptsfifo_waiting;
	int sp_mode;
	
	int linecounter;
	
	/* EM9010 overlay processor */
	int overlay_enabled;
	int overlay_mode;
	int overlay_gamma_enable;
	int overlay_xres;
	int overlay_yres;
	int overlay_frame_xpos;
	int overlay_frame_ypos;
	int overlay_frame_width;
	int overlay_frame_height;
	int overlay_a[EM9010_ATTRIBUTE_MAX+1];
	int overlay_double_y;
	int overlay_xcorr_default;
	int overlay_70;
	int overlay_dword_24bb8;

	/* Memory exported via mmap() */
	struct list_head  memory;

	/* Checksum for the on-board eeprom */
	u8 *eeprom_checksum;

	int model;

	struct em8300_config_s config;

	/* To support different options for different cards */
	unsigned int card_nr;
};

#if defined(CONFIG_SND) || defined(CONFIG_SND_MODULE)
#undef snd_card_t
#endif

#define TIMEDIFF(a,b) a.tv_usec - b.tv_usec + \
	    1000000 * (a.tv_sec - b.tv_sec)


/*
  Prototypes
*/

/* em8300_i2c.c */
int em8300_i2c_init1(struct em8300_s *em);
int em8300_i2c_init2(struct em8300_s *em);
void em8300_i2c_exit(struct em8300_s *em);
void em8300_clockgen_write(struct em8300_s *em, int abyte);

void em9010_write(struct em8300_s *em, int reg, int data);
int em9010_read(struct em8300_s *em, int reg);
int em9010_read16(struct em8300_s *em, int reg);
void em9010_write16(struct em8300_s *em, int reg, int value);

/* em8300_audio.c */
int em8300_audio_ioctl(struct em8300_s *em,unsigned int cmd, unsigned long arg);
int em8300_audio_flush(struct em8300_s *em);
int em8300_audio_open(struct em8300_s *em);
int em8300_audio_release(struct em8300_s *em);
int em8300_audio_setup(struct em8300_s *em);
ssize_t em8300_audio_write(struct em8300_s *em, const char * buf,
		       size_t count, loff_t *ppos);
int mpegaudio_command(struct em8300_s *em, int cmd);

/* em8300_ucode.c */
void em8300_ucode_upload(struct em8300_s *em, void *ucode, int ucode_size);
void em8300_require_ucode(struct em8300_s *em);

/* em8300_misc.c */
int em8300_setregblock(struct em8300_s *em, int offset, int val, int len);
int em8300_writeregblock(struct em8300_s *em, int offset, unsigned *buf, int len);
int em8300_waitfor(struct em8300_s *em, int reg, int val, int mask);
int em8300_waitfor_not(struct em8300_s *em, int reg, int val, int mask);

/* em8300_dicom.c */
void em8300_dicom_setBCS(struct em8300_s *em, int brightness, int contrast, int saturation);
void em8300_dicom_enable(struct em8300_s *em);
void em8300_dicom_disable(struct em8300_s *em);
int em8300_dicom_update(struct em8300_s *em);
void em8300_dicom_update_aspect_ratio(struct em8300_s *em);
void em8300_dicom_init(struct em8300_s *em);
int em8300_dicom_get_dbufinfo(struct em8300_s *em);
void em8300_dicom_fill_dispbuffers(struct em8300_s *em, int xpos, int ypos, int xsize,
				  int ysize, unsigned int pat1, unsigned int pat2);

/* em8300_video.c */
void em8300_video_open(struct em8300_s *em);
int em8300_video_setplaymode(struct em8300_s *em, int mode);
int em8300_video_sync(struct em8300_s *em);
int em8300_video_flush(struct em8300_s *em);
int em8300_video_setup(struct em8300_s *em);
int em8300_video_release(struct em8300_s *em);
void em8300_video_setspeed(struct em8300_s *em, int speed);
ssize_t em8300_video_write(struct em8300_s *em, const char * buf,
		       size_t count, loff_t *ppos);
int em8300_video_ioctl(struct em8300_s *em, unsigned int cmd, unsigned long arg);
void em8300_video_check_ptsfifo(struct em8300_s *em);

/* em8300_spu.c */
ssize_t em8300_spu_write(struct em8300_s *em, const char * buf,
		       size_t count, loff_t *ppos);
int em8300_spu_open(struct em8300_s *em);
int em8300_spu_ioctl(struct em8300_s *em, unsigned int cmd, unsigned long arg);
int em8300_spu_init(struct em8300_s *em);
void em8300_spu_check_ptsfifo(struct em8300_s *em);
int em8300_ioctl_setspumode(struct em8300_s *em, int mode);
void em8300_spu_release(struct em8300_s *em);

/* em8300_ioctl.c */
int em8300_control_ioctl(struct em8300_s *em, int cmd, unsigned long arg);
int em8300_ioctl_setvideomode(struct em8300_s *em, int mode);
int em8300_ioctl_setaspectratio(struct em8300_s *em, int ratio);
int em8300_ioctl_getstatus(struct em8300_s *em, char *usermsg);
int em8300_ioctl_init(struct em8300_s *em, em8300_microcode_t *useruc);
void em8300_ioctl_enable_videoout(struct em8300_s *em, int mode);
int em8300_ioctl_setplaymode(struct em8300_s *em, int mode);
int em8300_ioctl_setaudiomode(struct em8300_s *em, int mode);
int em8300_ioctl_getaudiomode(struct em8300_s *em, long int mode);
int em8300_ioctl_overlay_calibrate(struct em8300_s *em, em8300_overlay_calibrate_t *c);
int em8300_ioctl_overlay_setwindow(struct em8300_s *em,em8300_overlay_window_t *w);
int em8300_ioctl_overlay_setscreen(struct em8300_s *em,em8300_overlay_screen_t *s);
int em8300_ioctl_overlay_setmode(struct em8300_s *em,int val);

/* em9010.c */
int em9010_cabledetect(struct em8300_s *em);
int em9010_calibrate_xoffset(struct em8300_s *em);
int em9010_calibrate_yoffset(struct em8300_s *em);
int em9010_init(struct em8300_s *em);
int em9010_overlay_set_signalmode(struct em8300_s *em, int val);
int em9010_overlay_update(struct em8300_s *em);
int em9010_overlay_set_res(struct em8300_s *em, int xres, int yres);
void sub_4288c(struct em8300_s *em, int pa, int pb, int pc, int pd, int pe, int pf,
	       int pg, int ph);
int em9010_get_attribute(struct em8300_s *em, int attribute);
int em9010_set_attribute(struct em8300_s *em, int attribute, int value);

#endif /*EM8300_DRIVER_H*/
