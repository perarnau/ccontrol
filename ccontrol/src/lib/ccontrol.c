/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */
#include"ccontrol.h"
#include"freelist.h"
#include"ioctls.h"

#include<fcntl.h>
#include<stdio.h>
#include<string.h>
#include<sys/ioctl.h>
#include<sys/mman.h>
#include<sys/types.h>
#include<sys/stat.h>
#include<unistd.h>

#define DEVICE_NAMELENGTH 80
#define DEVICE_NAMEPREFIX MODULE_CONTROL_DEVICE
/* this library could possibly be made faster if it wasn't opening the control
 * device each time you want to create a zone.
 */

struct ccontrol_zone {
	int fd; /* the file description associated with the mmap */
	void *p; /* the pointer to the beginning of the mmap */
	size_t size; /* the size of the mmap */
	dev_t dev; /* the device number of the zone */
};

/* needed by libc_bypass code */
struct ccontrol_zone local_zone = { -1, NULL, 0, 0};

struct ccontrol_zone * ccontrol_new(void)
{
	struct ccontrol_zone *z;
	z = (struct ccontrol_zone *) malloc(sizeof(struct ccontrol_zone));
	if(z == NULL)
		return NULL;
	z->fd = -1;
	z->p = NULL;
	z->size = 0;
	return z;
}

void ccontrol_delete(struct ccontrol_zone *p)
{
	free(p);
}

size_t ccontrol_memsize2zonesize(unsigned int nballoc, size_t memsize)
{
	return memsize + ALLOCATOR_OVERHEAD + HEADER_SIZE*(nballoc-1);
}

int ccontrol_create_zone(struct ccontrol_zone *z, color_set *c, size_t size)
{
	int fd_cc,err = 0;
	ioctl_args io_args;
	char filename[DEVICE_NAMELENGTH];
	dev_t dev;
	if(z == NULL || c == NULL)
		return 1;
	/* open the module control device */
	fd_cc = open(MODULE_CONTROL_DEVICE, O_RDWR | O_NONBLOCK);
	if(fd_cc == -1)
	{
		perror("module control device open:");
		return 1;
	}
	/* tell him to create a new zone */
	io_args.size = size;
	io_args.c = *c;
	err = ioctl(fd_cc,IOCTL_NEW,&io_args);
	if(err == -1)
	{
		perror("module control device ioctl:");
		err = 1;
		goto close_control;
	}
	/* on succes, we need to create the device and mmap to it */
	/* create a name */
	snprintf(filename,DEVICE_NAMELENGTH,"%s%d",DEVICE_NAMEPREFIX,io_args.minor);
	dev = makedev(io_args.major,io_args.minor);
	err = mknod(filename,S_IFCHR | S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP,dev);
	if(err == -1)
	{
		perror("module color device mknod:");
		err = 1;
		goto clean_ioctl;
	}
	z->fd = open(filename,O_RDWR);
	if(z->fd == -1)
	{
		perror("module color device open:");
		err = 1;
		goto clean_node;
	}
	z->p = mmap(NULL,size,PROT_READ | PROT_WRITE, MAP_SHARED, z->fd,0);
	if(z->p == MAP_FAILED)
	{
		perror("module color device mmap:");
		err = 1;
		goto close_color;
	}
	/* initialize the freelist */
	fl_init(z->p,size);
	z->size = size;
	z->dev = dev;
	err = 0;
	goto close_control;

close_color:
	close(z->fd);
clean_node:
	unlink(filename);
clean_ioctl:
	/* if something went wrong after ioctl, we need to tell the kernel to destroy
	 * the new device
	 */
	ioctl(fd_cc,IOCTL_FREE,&io_args);
close_control:
	close(fd_cc);

	return err;
}

/* this function destroys a zone and its associated device.
 * Since most errors are unrecoverable, we just fall through
 * each error code, trying to clean everything whatever happens.
 */
int ccontrol_destroy_zone(struct ccontrol_zone *z)
{
	int fd_cc,err = 0;
	ioctl_args io_args;
	char filename[DEVICE_NAMELENGTH];
	if(z == NULL)
		return 1;
	/* unmap the device */
	err = munmap(z->p,z->size);
	if(err == -1)
		perror("module color device munmap:");
	/* close the device */
	err = close(z->fd);
	if(err == -1)
		perror("module color device close:");
	/* open the module control device */
	fd_cc = open(MODULE_CONTROL_DEVICE, O_RDWR | O_NONBLOCK);
	if(fd_cc == -1)
	{
		perror("module control device open:");
		return 1;
	}
	/* create a name */
	snprintf(filename,DEVICE_NAMELENGTH,"%s%d",DEVICE_NAMEPREFIX,minor(z->dev));
	/* unlink it */
	unlink(filename);
	/* now destroy the device */
	io_args.major = major(z->dev);
	io_args.minor = minor(z->dev);
	err = ioctl(fd_cc,IOCTL_FREE,&io_args);
	if(err == -1)
	{
		perror("module control device ioctl:");
		err = 1;
	}
	close(fd_cc);
	return err;
}

/* allocates memory inside the zone, use the freelist backend */
void *ccontrol_malloc(struct ccontrol_zone *z, size_t size)
{
	if(z == NULL || z->p == NULL)
		return NULL;
	return fl_allocate(z->p,size);
}

void ccontrol_free(struct ccontrol_zone *z, void *ptr)
{
	if(z == NULL || z->p == NULL)
		return;
	fl_free(z->p,ptr);
}

void *ccontrol_realloc(struct ccontrol_zone *z, void *ptr, size_t size)
{
	if(z == NULL || z->p == NULL)
		return NULL;
	return fl_realloc(z->p,ptr,size);
}

