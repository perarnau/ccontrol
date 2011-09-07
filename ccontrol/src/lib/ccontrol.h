/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */
#ifndef CCONTROL_H
#define CCONTROL_H 1

#include<stdlib.h>

#include"colorset.h"
#include"ioctls.h"
/* CControl library: provides colored memory allocations.
 * Tighly coupled with its Linux kernel module (in case of errors,
 * check that the library and module are in sync).
 * Warning: this library is NOT thread-safe.
 */
#ifndef MODULE_CONTROL_DEVICE
#define MODULE_CONTROL_DEVICE "/dev/ccontrol"
#endif
/* a colored zone, contains library internal info on the colors
 * authorized and how to discuss with its module */
struct ccontrol_zone;

/* environment variables names */
#define CCONTROL_ENV_COLORS "CCONTROL_COLORS"
#define CCONTROL_ENV_SIZE "CCONTROL_SIZE"

/* allocates a zone */
struct ccontrol_zone * ccontrol_new(void);

/* frees a zone */
void ccontrol_delete(struct ccontrol_zone *);

/* Convert a memory size requirement to a zone size
 * @nballoc is the number of malloc call required
 * @memsize is the total size of all required malloc
 */
size_t ccontrol_memsize2zonesize(unsigned int nballoc, size_t memsize);

/* Creates a new memory colored zone.
 * Needs a color set and a total size.
 * WARNING: the memory allocator needs space for itself, make
 * sure size is enough for him as well (see ccontrol_memsize2zonesize).
 * Return 0 on success. */
int ccontrol_create_zone(struct ccontrol_zone *, color_set *, size_t);

/* Destroys a zone.
 * Any allocation done inside it will no longer work.
 */
int ccontrol_destroy_zone(struct ccontrol_zone *);

/* Allocates memory inside the zone. Similar to POSIX malloc
 */
void *ccontrol_malloc(struct ccontrol_zone *, size_t);

/* Frees memory from the zone. */
void ccontrol_free(struct ccontrol_zone *, void *);

/* realloc memory */
void *ccontrol_realloc(struct ccontrol_zone *, void *, size_t);

/* translate string to color_set
 * format is like cpusets : "1-4,5"
 */
int ccontrol_str2cset(color_set *, char *);

/* translate string to size
 * format is similar to kernel args : 1k 1M 1G
 * upper and lower cases work
 */
int ccontrol_str2size(size_t *, char *);

#endif /* CCONTROL_H */
