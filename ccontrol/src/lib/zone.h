/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */
#ifndef ZONE_H
#define ZONE_H 1
/* this struct records the memory available inside a zone
 */
struct {
	int fd; /* file descriptor for the mmaped device */
	void *p; /* pointer to the beginning of the mmap */
	size_t size; /* size of the mmap */
} ccontrol_zone;

#endif /* ZONE_H */
