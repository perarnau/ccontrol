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

#include<ctype.h>
#include<errno.h>
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<sys/mman.h>
#include<unistd.h>

/* Dynamic memory allocations bypass : this code
 * redefines malloc, calloc, realloc and free to provide
 * LD_PRELOAD features.
 * All memory allocations will go in a single zone using
 * this code.
 *
 * Two environment variables must be defined:
 * CCONTROL_PSET: gives the color set to use.
 * CCONTROL_SIZE: gives the allocation size to ask.
 */

extern struct ccontrol_zone local_zone;
unsigned short init_ok = 0;
unsigned short in_init = 0;

static void cleanup()
{
	int err;
	if(init_ok) {
		err = ccontrol_destroy_zone(&local_zone);
		if(err)
			_exit(EXIT_FAILURE);
		init_ok = 0;
	}
}

static void init()
{
	char *env_pset, *env_size;
	color_set c;
	size_t size;
	int err;

	in_init = 1;

	/* setup cleanup code */
	err = atexit(&cleanup);
	if(err)
	{
		perror("ccontrol: installing cleanup code");
		exit(EXIT_FAILURE);
	}

	/* load environment variables */
	env_pset = getenv(CCONTROL_ENV_PARTITION_COLORSET);
	if(env_pset == NULL)
	{
		fprintf(stderr,"ccontrol: missing env variable %s\n",CCONTROL_ENV_PARTITION_COLORSET);
		exit(EXIT_FAILURE);
	}

	env_size = getenv(CCONTROL_ENV_SIZE);
	if(env_size == NULL)
	{
		fprintf(stderr,"ccontrol: missing env variable %s\n",CCONTROL_ENV_SIZE);
		exit(EXIT_FAILURE);
	}

	/* parse colors into colorset */
	err = ccontrol_str2cset(&c,env_pset);
	if(err)
	{
		fprintf(stderr,"ccontrol: invalid colorset in %s\n",CCONTROL_ENV_PARTITION_COLORSET);
		exit(EXIT_FAILURE);
	}

	/* parse size */
	err = ccontrol_str2size(&size,env_size);
	if(err)
	{
		fprintf(stderr,"ccontrol: invalid size in %s, %s\n",CCONTROL_ENV_SIZE,
				strerror(err));
		exit(EXIT_FAILURE);
	}

	/* allocate zone */
	err = ccontrol_create_zone(&local_zone,&c,size);
	if(err)
	{
		fprintf(stderr,"ccontrol: failed to allocate global zone\n");
		exit(EXIT_FAILURE);
	}
	in_init = 0;
	init_ok = 1;
}

#define MMAP(s) mmap(NULL,s,PROT_READ|PROT_WRITE|PROT_EXEC,MAP_PRIVATE|MAP_ANONYMOUS,-1,0)
void * malloc(size_t size)
{
	if(in_init)
		return MMAP(size);
	if(!init_ok)
		init();
	return ccontrol_malloc(&local_zone,size);
}

void free(void * ptr)
{
	if(in_init)
		return;
	if(!init_ok)
		init();
	ccontrol_free(&local_zone,ptr);
}

void * realloc(void *ptr, size_t size)
{
	if(in_init)
	{
		void *r = MMAP(size);
		return memcpy(r,ptr,size);
	}
	if(!init_ok)
		init();
	return ccontrol_realloc(&local_zone,ptr,size);
}

void * calloc(size_t nm, size_t size)
{
	void *p = NULL;
	if(nm == 0 || size == 0)
		return NULL;

	if(in_init)
		return MMAP(nm*size);
	if(!init_ok)
		init();


	p = malloc(nm*size);
	if(p != NULL)
		p = memset(p,0,size*nm);
	return p;
}
