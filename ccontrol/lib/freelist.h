/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */
#ifndef FREELIST_H
#define FREELIST_H 1

#include<stdlib.h>
#include"zone.h"
/* a free list elt, indicating a free slot indise
 * a zone
 */
struct fl_elt {
	size_t size;
	struct fl_elt* next;
};
typedef struct fl_elt fl;

#define HEADER_SIZE	(sizeof(size_t))
#define FL_TO_VOID(x)	(void *)((char *) x + HEADER_SIZE)
#define VOID_TO_FL(x)	(fl *)((char *)x - HEADER_SIZE)

/* free_list code: a free_list is a list of free memory regions
 * inside a zone. It is managed inside the zone memory.
 */

void *fl_allocate(void *z, size_t size);

void fl_free(void *z, void *p);

#endif /* FREELIST_H */
