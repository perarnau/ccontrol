/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */

/* the color set structure, used to create a partition
 * we use an array of unsigned long to create a bit mask.
 * Inspired by glibc implementation of FD_SET (select(3)).
 */

#ifndef COLOR_SET_H
#define COLOR_SET_H 1

typedef unsigned long __color_mask;

#define __NBITS		(8*sizeof(__color_mask))
#define __COLORELT(c)	((c) / __NBITS )
#define __COLORMASK(c)	((__color_mask) 1 << ((c) % __NBITS))
/* 1024 colors in a cache is a very large value, it should be enough
 * for the next 5 years
 */
#define __MAX_COLORS	(1024)
#define __CSET_SIZE	(__MAX_COLORS / __NBITS)
typedef struct {
	__color_mask colors_bits[ __CSET_SIZE ];
} color_set;
#define  __COLORS_BITS(set) ((set)->colors_bits)

/* Number of colors in color_set. */
#define COLOR_SETSIZE	__MAX_COLORS

/* Access macros for color_set. */
#define COLOR_SET(c,setp) \
	(__COLORS_BITS (setp)[ __COLORELT(c) ] |= __COLORMASK(c))
#define COLOR_CLR(c,setp) \
	(__COLORS_BITS (setp)[ __COLORELT(c) ] &= ~__COLORMASK(c))
#define COLOR_ZERO(setp)  \
	do {                                             \
		unsigned int __i;                        \
		color_set *__arr = (setp);               \
		for (__i = 0; __i < __CSET_SIZE; ++__i)  \
			__COLORS_BITS (__arr)[__i] = 0;  \
	} while (0)
#define COLOR_ISSET(c, setp) \
	  ((__COLORS_BITS (setp)[__COLORELT(c)] & __COLORMASK(c)) != 0)

#endif /* COLOR_SET_H */
