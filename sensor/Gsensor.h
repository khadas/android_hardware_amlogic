/*
 * Definitions for kxtj3 chip.
 */
#ifndef GSENSOR_H
#define GSENSOR_H

#include <linux/ioctl.h>


#define GSENSOR_IOCTL_MAGIC				'a'

/* IOCTLs for GSENSOR library */
#define GSENSOR_IOCTL_INIT                  _IO(GSENSOR_IOCTL_MAGIC, 0x01)
#define GSENSOR_IOCTL_RESET      	        _IO(GSENSOR_IOCTL_MAGIC, 0x04)
#define GSENSOR_IOCTL_CLOSE		        _IO(GSENSOR_IOCTL_MAGIC, 0x02)
#define GSENSOR_IOCTL_START		        _IO(GSENSOR_IOCTL_MAGIC, 0x03)
#define GSENSOR_IOCTL_GETDATA               _IOR(GSENSOR_IOCTL_MAGIC, 0x08, char[RBUFF_SIZE+1])

/* IOCTLs for APPs */
#define GSENSOR_IOCTL_APP_SET_RATE		_IOW(GSENSOR_IOCTL_MAGIC, 0x10, short)
#define GSENSOR_IOCTL_GET_CALIBRATION      _IOR(GSENSOR_IOCTL_MAGIC, 0x11, int[3])


#define  GSENSOR_DEV_PATH    "/dev/gsensor"


#endif

