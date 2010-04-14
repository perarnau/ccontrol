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
#include <linux/module.h>

static int __init init(void)
{
	printk("kcache init'd !\n");
	return 0;
}

static void __exit exit(void)
{
	printk("kcache exit'd !\n");
}

/* Kernel Macros
 */

module_init(init);
module_exit(exit);

MODULE_AUTHOR("Swann Perarnau <swann.perarnau@imag.fr>");
MODULE_DESCRIPTION("Provides a debugfs file to mmap a physical adress range.");
MODULE_LICENSE("GPL");
