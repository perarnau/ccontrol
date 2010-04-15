#!/bin/sh
#
#
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, version 2 of the
# License.
#
# Kernel Module to reserve physical adress ranges for future mmap.
#
# Copyright (C) 2010 Swann Perarnau
# Author: Swann Perarnau <swann.perarnau@imag.fr>
#
# This program generates a header containing all necessary information
# using /sys to discover cache info.
# this program could be improved, especially to discover and define enough
# info on architecture having less levels
rm cache.h
cpu=0
sys_path="/sys/devices/system/cpu/cpu$cpu/cache"
last_level=`ls $sys_path | sort -n | tail -n 1`
line_size=`cat $sys_path/$last_level/coherency_line_size`
echo "line size: $line_size"
cat >> cache.h <<END
/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Kernel Module to reserve physical adress ranges for future mmap.
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */

/* this header should be regenerated each time the machine changes */
#ifndef PAGE_SIZE
#define PAGE_SIZE (1<<12)
#endif

#define CACHE_LINE_SIZE ($line_size)
END
ll_cache_size=`cat $sys_path/$last_level/size | sed -e 's/K//g'`
ll_cache_size=`echo "$ll_cache_size * 1024" | bc`
ll_assoc=`cat $sys_path/$last_level/ways_of_associativity`
echo "ll cache size: $ll_cache_size"
echo "ll assoc: $ll_assoc"
cat >> cache.h <<END

#define LL_CACHE_SIZE ($ll_cache_size)
#define LL_CACHE_ASSOC ($ll_assoc)
#define LL_NUM_COLORS (LL_CACHE_SIZE/(PAGE_SIZE*LL_CACHE_ASSOC))
END

ll_level=`cat $sys_path/$last_level/level`
if [ $ll_level -ne 2 ]; then
	for dir in $sys_path/*; do
		lvl=`cat $dir/level`;
		if [ $lvl -eq 2 ]; then
			l2_dir=$dir
		fi
	done
	l2_cache_size=`cat $l2_dir/size | sed -e 's/K//g'`
	l2_cache_size=`echo "$l2_cache_size * 1024" | bc`
	l2_assoc=`cat $l2_dir/ways_of_associativity`
	echo "l2 cache size: $l2_cache_size"
	echo "l2 assoc: $l2_assoc"
	cat >> cache.h <<END
#define L2_CACHE_SIZE ($l2_cache_size)
#define L2_CACHE_ASSOC ($l2_assoc)
#define L2_NUM_COLORS (L2_CACHE_SIZE/(PAGE_SIZE*L2_CACHE_ASSOC))
END
fi

cat >> cache.h <<END
/* the number of colors is defined by C/AP */
#define MAX_SUBDIVISIONS LL_NUM_COLORS

#define PFN_TO_COLOR(pfn) (pfn%LL_NUM_COLORS)
END
