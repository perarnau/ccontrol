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
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
// memory management
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <asm/page.h>
// devices
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
// cache info
#include "cache.h"

MODULE_AUTHOR("Swann Perarnau <swann.perarnau@imag.fr>");
MODULE_DESCRIPTION("Provides a debugfs file to mmap a physical adress range.");
MODULE_LICENSE("GPL");

// module parameters
static unsigned int subdivs[MAX_SUBDIVISIONS];
static unsigned int subdivs_size;
module_param_array(subdivs,uint,&subdivs_size,0);
MODULE_PARM_DESC(subdivs,"array of cache subdivision sizes.");
static unsigned long mem = 1<<10;
module_param(mem,ulong,0);
MODULE_PARM_DESC(mem,"How much memory should I reserve in RAM.");
static unsigned int order = 0;
// maps each color to a subdivision
static int colormap[LL_NUM_COLORS];

/* Initializes the colormap.
 * If the L2 is the last cache, we do a simple
 * contiguous allocation of colors to subdivisions.
 * If both L3 and L2 are present, we sort the colors
 * by L2 affinity before giving them to subdivs.
 */
#ifdef L2_NUM_COLORS
void init_colormap(void)
{
	int i,j;
	int index = 0;
	int colors[LL_NUM_COLORS];
	for(i = 0; i < L2_NUM_COLORS; i++)
	{
		for(j = 0; j < LL_NUM_COLORS/L2_NUM_COLORS; j++)
			colors[index++] = j*L2_NUM_COLORS +i;
		if(j*L2_NUM_COLORS + i < LL_NUM_COLORS)
			colors[index++] = j*L2_NUM_COLORS +i;
	}
	// now register the subdiv mapping
	index = 0;
	for(i = 0; i < subdivs_size; i++)
		for(j = 0; j < subdivs[i]; j++)
			colormap[colors[index++]] = i;

	for(i = 0; i < LL_NUM_COLORS; i++)
		printk(KERN_DEBUG "ccontrol:colormap: color=%u,subdiv=%u.\n",
			i,colormap[i]);
}
#else
void init_colormap(void)
{
	int i,j;
	int index = 0;
	for(i = 0; i < subdivs_size; i++)
		for(j = 0; j < subdivs[i]; j++)
			colormap[index++] = i;

	for(i = 0; i < LL_NUM_COLORS; i++)
		printk(KERN_DEBUG "ccontrol:colormap: color=%u,subdiv=%u.\n",
			i,colormap[i]);
}
#endif

// devices
static dev_t parts_dev = 0;
struct ccontrol_dev {
	unsigned int size;
	struct page **pages;
	struct cdev cdev;
};

struct ccontrol_dev *ccontrol_devices = NULL;

// allocated pages
struct page* **pages = NULL;
struct page* *heads = NULL;
unsigned int *sizes = NULL;
unsigned int heads_size  = 0;

// VMA Operations
void ccontrol_vma_open(struct vm_area_struct *vma)
{
	//struct ccontrol_dev *dev = vma->vm_private_data;
	// WHAT TO DO ?
}

void ccontrol_vma_close(struct vm_area_struct *vma)
{
	//struct ccontrol_dev *dev = vma->vm_private_data;
	// WHAT TO DO ?
}

int ccontrol_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	unsigned long offset = 0;
	int ret = VM_FAULT_ERROR, err = 0;
	struct ccontrol_dev *dev = vma->vm_private_data;
	struct page * page = NULL;
	unsigned long pfn = 0;
	vmf->page = NULL;

	offset =(unsigned int)vmf->pgoff;
	printk(KERN_DEBUG "ccontrol:fault: off=%lu.\n",offset);
	if(offset >= dev->size)
		goto out;

	page = dev->pages[offset];
	pfn = page_to_pfn(page);
	printk(KERN_DEBUG "ccontrol:fault: pfn=%lu,color=%lu.\n",
			pfn,PFN_TO_COLOR(pfn));

	// insert page into userspace
	err = vm_insert_page(vma,(unsigned long)vmf->virtual_address,page);
	if(err)
		goto out;
	ret = VM_FAULT_NOPAGE;
out:
	printk(KERN_DEBUG "ccontrol:fault: ret=%d.\n",ret);
	printk(KERN_DEBUG "ccontrol:fault: page count: %d(%d)\n",page_count(page),page->_count);
	return ret;
}

struct vm_operations_struct ccontrol_vm_ops = {
	.open = ccontrol_vma_open,
	.close = ccontrol_vma_close,
	.fault = ccontrol_vma_fault,
};

// Device operations

int ccontrol_open(struct inode *inode, struct file *filp)
{
	struct ccontrol_dev *dev;
	dev = container_of(inode->i_cdev, struct ccontrol_dev, cdev);
	filp->private_data = dev;
	return 0;
}

int ccontrol_release(struct inode *inode, struct file *filp)
{
	return 0;
}

int ccontrol_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct ccontrol_dev *dev = filp->private_data;
	unsigned int size;
	// check for no offset
	printk(KERN_INFO "ccontrol: mmap offset %lu.\n",vma->vm_pgoff);
	if(vma->vm_pgoff != 0)
		return -EPERM;

	// check size is ok
	size = (vma->vm_end - vma->vm_start)/PAGE_SIZE;
	printk(KERN_INFO "ccontrol: mmap size %u, available %d.\n",
			size, dev->size);
	if(size > dev->size )
		return -ENOMEM;
	// check MAP_SHARED is not asked
	if(!(vma->vm_flags & VM_SHARED)) {
		printk(KERN_ERR "ccontrol: you should not ask for a private mapping");
		return -EPERM;
	}
	vma->vm_ops = &ccontrol_vm_ops;
	vma->vm_flags |= VM_RESERVED | VM_CAN_NONLINEAR;
	vma->vm_private_data = filp->private_data;
	ccontrol_vma_open(vma);
	return 0;
}

static struct file_operations ccontrol_fops = {
	.owner = THIS_MODULE,
	.open = ccontrol_open,
	.release = ccontrol_release,
	.mmap = ccontrol_mmap,
};

static void ccontrol_setup_dev(struct ccontrol_dev *dev, int index)
{
	int err, devno = MKDEV(MAJOR(parts_dev),MINOR(parts_dev) + index);

	dev->size = sizes[index];
	dev->pages = pages[index];

	cdev_init(&dev->cdev,&ccontrol_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &ccontrol_fops;
	err = cdev_add(&dev->cdev, devno,1);
	if(err)
		printk(KERN_NOTICE "Error %d adding cdev %d.\n",err,index);
}

static void cleanup(void)
{
	int i;
	if(heads)
	{
		for(i = 0; i < heads_size; i++)
			__free_pages(heads[i],order);

		kfree(heads);
	}

	if(pages)
	{
		for(i = 0; i < subdivs_size; i++)
		{
			if(pages[i])
				vfree(pages[i]);
		}
		kfree(pages);
	}
	if(sizes)
		kfree(sizes);

	if(ccontrol_devices)
		kfree(ccontrol_devices);

	if(parts_dev)
		unregister_chrdev_region(parts_dev,subdivs_size);
}

static int __init init(void)
{
	int i,j,k;
	int err = 0;
	unsigned long pfn;
	struct page *page,*nth;
	unsigned int sum;
	unsigned int color;
	unsigned int blocks;
	printk("ccontrol: started !\n");
	// check parameters
	order = get_order(MAX_SUBDIVISIONS*PAGE_SIZE);
	if(order < 0)
	{
		printk(KERN_ERR "ccontrol: cannot find a good physical page block size.\n");
		err = -EPERM;
		goto error;
	}
	printk(KERN_ERR "ccontrol: each block is %lu ko wide.\n",(PAGE_SIZE * (1<<order))/1024);
	// the sum of subdivs must be MAX_SUBDIVS
	sum = 0;
	for(i= 0; i< subdivs_size; i++)
		sum += subdivs[i];

	if(sum != MAX_SUBDIVISIONS)
	{
		printk(KERN_ERR "ccontrol: wrong subdivs parameter, must sum to %lu.\n",MAX_SUBDIVISIONS);
		err = -EPERM;
		goto error;
	}
	// compute the number of blocks we should allocate to reserve enough memory.
	blocks = mem / (PAGE_SIZE * (1<<order));
	if(blocks * (PAGE_SIZE * (1<<order)) < mem)
		blocks++;

	printk(KERN_DEBUG "ccontrol: will allocate %d blocks of order %d.\n",blocks,order);

	// create devices
	err = alloc_chrdev_region(&parts_dev,0,subdivs_size,"ccontrol");
	if(err)
		goto error;

	printk(KERN_INFO "ccontrol: Got %d device major.\n",MAJOR(parts_dev));
	printk(KERN_INFO "ccontrol: Created %d minor devices, starting from %d.\n",
		subdivs_size,MINOR(parts_dev));

	// allocate pages table
	sizes = kmalloc(subdivs_size*sizeof(unsigned int),GFP_KERNEL);
	if(!sizes)
	{
		err = -ENOMEM;
		goto error;
	}
	for(i = 0; i < subdivs_size; i++)
		sizes[i] = 0;

	pages = kmalloc(subdivs_size*sizeof(struct page**),GFP_KERNEL);
	if(!pages)
	{
		err = -ENOMEM;
		goto error;
	}

	for(i = 0; i< subdivs_size; i++)
	{
		pages[i] = vmalloc(blocks*2*subdivs[i]*sizeof(struct page*));
		if(!pages[i])
		{
			err = -ENOMEM;
			for(i = i-1;i>=0; i--)
				vfree(pages[i]);
			goto error;
		}
	}
	heads = kmalloc(blocks*sizeof(struct page *),GFP_KERNEL);
	if(!heads)
	{
		err = -ENOMEM;
		goto error;
	}
	printk(KERN_DEBUG "ccontrol: pages table correctly allocated.\n");
	init_colormap();
	for(i = 0; i < blocks; i++)
	{
		// allocate a page
		page = alloc_pages(GFP_HIGHUSER | __GFP_COMP,order);
		if(!page) {
			printk(KERN_INFO
				"ccontrol: failed to get a page");
			err = -ENOMEM;
			goto error;
		}
		heads[heads_size++] = page;
		pfn = page_to_pfn(page);
		color = PFN_TO_COLOR(pfn);
		printk(KERN_DEBUG "ccontrol: pfn %lu obtained, color %u.\n",pfn,color);
		// distribute the whole order among subdivisions
		for(j = 0; j < 1<<order; j++)
		{
			nth = nth_page(page,j);
			pfn = page_to_pfn(nth);
			color = PFN_TO_COLOR(pfn);
			// find to which subdiv is this color
			k = colormap[color];
			pages[k][sizes[k]] = nth;
			sizes[k]++;
			printk(KERN_DEBUG "ccontrol: %lu is %dth page of part %d, color %u.\n",
						pfn,sizes[k]-1,k,color);
		}
	}

	// allocate dev structs
	ccontrol_devices = kmalloc(subdivs_size*sizeof(struct ccontrol_dev),GFP_KERNEL);
	if(!ccontrol_devices)
	{
		err = -ENOMEM;
		goto error;
	}
	memset(ccontrol_devices,0,subdivs_size*sizeof(struct ccontrol_dev));
	for(i = 0; i < subdivs_size; i++)
		ccontrol_setup_dev(ccontrol_devices+i,i);

	// display memory info
	printk(KERN_INFO "ccontrol: %d devices available.\n",subdivs_size);
	for(i = 0; i< subdivs_size; i++)
	{
		printk(KERN_INFO "ccontrol: %d %lu %lu (subdiv,KB avail,KB cachesize).\n",i,
				sizes[i]*PAGE_SIZE/1024,
				subdivs[i]*PAGE_SIZE*LL_CACHE_ASSOC/1024);
	}
	return 0;
error:
	cleanup();
	return err;
}

static void __exit exit(void)
{
	cleanup();
	printk(KERN_INFO "ccontrol: stoped !\n");
}

/* Kernel Macros
 */

module_init(init);
module_exit(exit);
