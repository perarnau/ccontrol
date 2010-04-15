/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Malloc replacement for use with kcache
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define PAGE_SHIFT	(12U)
#define PAGE_SIZE	(1UL << PAGE_SHIFT)
#define PAGE_MASK	(PAGE_SIZE - 1)
#define PAGE_ROUND(x)	(((x) + (PAGE_MASK)) & ~(PAGE_MASK))

struct mmap_dev {
	const char *name;
	unsigned long size;
	void *p;
	int fd;
};

static pthread_mutex_t m_lock = PTHREAD_MUTEX_INITIALIZER;

/* you should edit this to be consistent
 * with the current kcache configuration.
 */
#define DEVICES		(4)
struct mmap_dev config[DEVICES] = {
	{"/dev/kcache0",32*PAGE_SIZE, NULL, -1},
	{"/dev/kcache1",32*PAGE_SIZE, NULL, -1},
	{"/dev/kcache2",32*PAGE_SIZE, NULL, -1},
	{"/dev/kcache3",32*PAGE_SIZE, NULL, -1}
};

struct free_region {
	unsigned long size;
	struct free_region* next;
};

#define MIN_SIZE	(sizeof(struct free_region))
#define HEADER_SIZE	(sizeof(unsigned long))
#define FR_TO_VOID(x)	(void *)((char *) x + HEADER_SIZE)
#define VOID_TO_FR(x)	(struct free_region *)((char *)x - HEADER_SIZE)

static unsigned long normalize(unsigned long size)
{
#ifdef DEBUG
	fprintf(stderr,"in %s, size is %lu\n",__func__,size);
#endif
	if(size % MIN_SIZE != 0)
		size += MIN_SIZE - (size % MIN_SIZE);
	return size;
}

/**************************************
 * FREE REGION CODE
 **************************************
 */
static void insert(int dev,struct free_region *fr)
{
	fr->next = (struct free_region *) config[dev].p;
	config[dev].p = (void *)fr;
}

static void extract(int dev, struct free_region *fr, struct free_region *prev)
{
	struct free_region *head;
	/* only one zone */
	head = (struct free_region *) config[dev].p;
#ifdef DEBUG
	fprintf(stderr,"in %s, head contains %p\n",__func__,(void*)head);
#endif
	if(head->next == NULL)
	{
		config[dev].p = NULL;
		fr->next = NULL;
	}
	else
	{
		if(prev == NULL)
		{
			config[dev].p = fr->next;
		}
		else
		{
			prev->next = fr->next;
		}
	}
}

static void find_fit(int dev, unsigned long size,
		struct free_region **found,struct free_region **prev)
{
	struct free_region *it,*old;
	it = (struct free_region *) config[dev].p;
	old = NULL;
	do
	{
		if(it->size >= size)
		{
			*found = it;
			*prev = old;
#ifdef DEBUG
			fprintf(stderr,"in %s,found %p, prev is %p\n",__func__,(void *)it,
					(void *)old);
#endif
			return;
		}
		old = it;
		it = it->next;
	} while( it != NULL);
	*found = NULL;
	*prev = NULL;
}

static struct free_region * find_previous(int dev, struct free_region *fr,
		struct free_region **prv)
{
	struct free_region *it,*prev;
	it = (struct free_region *) config[dev].p;
	prev = NULL;
	while(it != NULL)
	{
		if((struct free_region *)((char *)it + it->size) == fr)
		{
			*prv = prev;
			return it;
		}
		prev = it;
		it = it->next;
	}
	*prv = NULL;
	return NULL;
}

static struct free_region * find_next(int dev, struct free_region *fr,
		struct free_region **prv)
{
	struct free_region *it,*prev;
	it = (struct free_region *) config[dev].p;
	prev = NULL;
	while(it != NULL) {
		if(it == fr)
		{
			*prv = prev;
			return it;
		}
		prev = it;
		it = it->next;
	}
	*prv = NULL;
	return NULL;
}

static void *allocate(int dev,unsigned long size)
{
	struct free_region *it,*prev;
	void *p;
	size = normalize(size);
	size += HEADER_SIZE;

	if(size <= 0 || size > config[dev].size)
		return NULL;
#ifdef DEBUG
	fprintf(stderr,"in %s,real size is %lu, dev is %d\n",__func__,size,dev);
#endif
	/* find a fitting free zone */
	find_fit(dev,size,&it,&prev);
#ifdef DEBUG
	fprintf(stderr,"in %s,found %p,size %lu\n",__func__,(void *)it,it?it->size:0);
#endif
	if(it == NULL)
		return NULL;
	/* update the zone */
	if(it->size == size)
	{
		extract(dev,it,prev);
		p = FR_TO_VOID(it);
	}
	else
	{
		it->size -= size;
		it =(struct free_region *)((char *) it + it->size);
#ifdef DEBUG
		fprintf(stderr,"in %s,too big, split is at %p\n",__func__,(void *)it);
#endif
		it->size = size;
		p = FR_TO_VOID(it);
	}
#ifdef DEBUG
	fprintf(stderr,"in %s, will return %p\n",__func__,p);
#endif
	return p;
}


static void destroy(int dev, void *p)
{
	struct free_region *fr,*it,*prev,*bfr_prev;
#ifdef DEBUG
	fprintf(stderr,"in %s,received %p\n",__func__,p);
#endif
	fr = VOID_TO_FR(p);
	fr->next = NULL;
#ifdef DEBUG
	fprintf(stderr,"in %s,fr is %p\n",__func__,(void *)fr);
#endif
	it = fr;

	/* find previous zone  */
	prev = find_previous(dev,fr,&bfr_prev);
#ifdef DEBUG
	fprintf(stderr,"in %s,prev of %p is %p\n",__func__,(void *)fr,(void *)prev);
#endif
	if(prev != NULL)
	{
		extract(dev,prev,bfr_prev);
		prev->size += fr->size;
		it = prev;
	}
	/* find next zone */
	prev = find_next(dev,it+it->size,&bfr_prev);
#ifdef DEBUG
	fprintf(stderr,"in %s,next of %p is %p\n",__func__,(void *)(it+it->size),(void *)prev);
#endif
	if(prev != NULL)
	{
		extract(dev,prev,bfr_prev);
		it->size += prev->size;
	}
#ifdef DEBUG
	fprintf(stderr,"in %s,inserting %p\n",__func__,(void *)it);
#endif
	insert(dev,it);
}

/**************************************
 * LIBRARY CODE
 **************************************
 */

static void cleanup()
{
	int i,err;
#ifdef DEBUG
	fprintf(stderr,"in %s\n",__func__);
#endif
	for(i = 0; i < DEVICES; i++)
	{
		if(config[i].p != NULL)
		{
			err = munmap(config[i].p, config[i].size);
			if(err)
				perror("Munmap");
		}
		if(config[i].fd != -1)
		{
			err = close(config[i].fd);
			if(err)
				perror("Close");
		}
	}
}

static int init()
{
	int i,fd;
	void *m;
	struct free_region *f;
#ifdef DEBUG
	fprintf(stderr,"in %s\n",__func__);
#endif
	/* register cleanup to be called on program exiting */
	i = atexit(&cleanup);
	if(i)
	{
		fprintf(stderr,"cannot set cleanup function\n");
		exit(EXIT_FAILURE);
	}

	/* mmap all devices
	 * if any error, all config is aborted
	 */
	for(i = 0; i < DEVICES; i++)
	{
		fd = open(config[i].name, O_RDWR);
		if(fd == -1)
		{
			perror("Open for mmap");
			goto error;
		}
		config[i].fd = fd;
		m = mmap(NULL,config[i].size, PROT_READ | PROT_WRITE,
				MAP_SHARED,config[i].fd, 0);
		if(m == NULL)
		{
			perror("Mmap");
			goto error;
		}
		config[i].p = m;
		f = (struct free_region *)m;
		f->size = config[i].size;
		f->next = NULL;

	}
	return 0;
error:
	return 1;
}

/* checks if library is in a usable state
 * calls init if necessary
 */
static int test_init()
{
#ifdef DEBUG
	fprintf(stderr,"in %s\n",__func__);
#endif
	if(config[0].fd == -1)
		if(init())
			return 1;

	return 0;
}

/* computes the device to use for the current allocation */
static int get_device()
{
	return pthread_self()%DEVICES;
}

void * malloc(size_t size)
{
	void *p = NULL;
	pthread_mutex_lock(&m_lock);
#ifdef DEBUG
	fprintf(stderr,"in %s\n",__func__);
#endif
	if(test_init())
		goto ret;

	p = allocate(get_device(),(unsigned long) size);
#ifdef DEBUG
	fprintf(stderr,"in %s, ask %u, got %p\n",__func__,size,p);
#endif
ret:
	pthread_mutex_unlock(&m_lock);
	return p;
}

void free(void * ptr)
{
	pthread_mutex_lock(&m_lock);
#ifdef DEBUG
	fprintf(stderr,"in %s\n",__func__);
#endif
	if(ptr == NULL)
		goto ret;

	if(test_init())
		goto ret;

	destroy(get_device(),ptr);
ret:
	pthread_mutex_unlock(&m_lock);
}

void * realloc(void *ptr, size_t size)
{
	int dev;
	void *p = NULL;
	struct free_region *fr;
#ifdef DEBUG
	fprintf(stderr,"in %s\n",__func__);
#endif
	if(ptr == NULL)
		return malloc(size);

	if(size == 0)
	{
		free(ptr);
		return NULL;
	}

	pthread_mutex_lock(&m_lock);
	if(test_init())
		goto ret;

	dev = get_device();
	/* retrieve old size */
	fr = VOID_TO_FR(ptr);
#ifdef DEBUG
	fprintf(stderr,
		"in %s, ptr is %p, fr is %p of size %lu\n",__func__,ptr,(void *)fr,fr->size);
#endif
	if(fr->size == size)
	{
		p = ptr;
		goto ret;
	}

	p = allocate(dev,size);
	if(p != NULL)
	{
		memcpy(p,ptr,fr->size);
		destroy(dev,ptr);
#ifdef DEBUG
		fprintf(stderr,"in %s, copied %p to %p\n",__func__,ptr,p);
#endif
	}
ret:
	pthread_mutex_unlock(&m_lock);
	return p;
}

void * calloc(size_t nm, size_t size)
{
	void *p = NULL;
	if(nm == 0 || size == 0)
		return NULL;

	pthread_mutex_lock(&m_lock);
#ifdef DEBUG
	fprintf(stderr,"in %s\n",__func__);
#endif
	if(test_init())
		goto ret;

	p = allocate(get_device(),(unsigned long) size * (unsigned long)nm);
	if(p != NULL)
		p = memset(p,0,size*nm); /* MAY BE OVERFLOWED */
#ifdef DEBUG
	fprintf(stderr,"in %s, will return %p\n",__func__,p);
#endif
ret:
	pthread_mutex_unlock(&m_lock);
	return p;
}
