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
#include"zone.h"
#include"freelist.h"

struct ccontrol_zone {
	int fd; /* the file description associated with the mmap */
	void *p; /* the pointer to the beginning of the mmap */
};

int ccontrol_create_zone(struct ccontrol_zone *z, color_set *c)
{
	return 0;
}

int ccontrol_destroy_zone(struct ccontrol_zone *z)
{
	return 0;
}

/* allocates memory inside the zone, use the freelist backend */
void *ccontrol_malloc(struct ccontrol_zone *z, size_t size)
{
	return fl_allocate(z->p,size);
}

void ccontrol_free(struct ccontrol_zone *z, void *ptr)
{
	return fl_free(z->p,ptr);
}
