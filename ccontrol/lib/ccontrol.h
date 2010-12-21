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

/* Creates a new memory colored zone.
 * Needs a color set and a total size.
 * Return 0 on success. */
int ccontrol_create_zone(struct ccontrol_zone *, color_set *, unsigned int size);

/* Destroys a zone.
 * Any allocation done inside it will no longer work.
 */
int ccontrol_destroy_zone(struct ccontrol_zone *);

/* Allocates memory inside the zone. Similar to POSIX malloc
 */
void *ccontrol_malloc(struct ccontrol_zone *, size_t);

/* Frees memory from the zone. */
void ccontrol_free(struct ccontrol_zone *, void *);

#endif /* CCONTROL_H */
