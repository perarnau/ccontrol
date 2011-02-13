/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */

/* ioctl codes availables in ccontrol:
 * IOCTL_NEW: creates a new device, given a size in pages and a colorset
 * IOCTL_FREE: destroy a device, its memory is given back to the kernel module.
 */

#ifndef IOCTLS_H
#define IOCTLS_H

#include<linux/ioctl.h>

/* ioctl needs a fixed major number, unfortunately */
#define MAJOR_NUM 250

/* the data structure passed to ioctl:
 * - _new: contains size and colorset on input
 *         dev on output
 * - free: contains dev on input
 */

typedef struct cc_args {
	int major;
	int minor;
	unsigned int size;
	color_set c;
} ioctl_args;

#define IOCTL_NEW _IOWR(MAJOR_NUM,0,ioctl_args *)
#define IOCTL_FREE _IOR(MAJOR_NUM,0,ioctl_args *)

#endif /* IOCTLS_H */
