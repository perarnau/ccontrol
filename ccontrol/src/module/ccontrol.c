/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Kernel Module to reserve physical address ranges for future mmap.
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/version.h>
// memory management
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <linux/slab.h>
#include <asm/page.h>
#include <asm/uaccess.h>
// devices
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/device.h>
// linked list
#include <linux/list.h>
// cache info
#include "cache_info.h"
#include "colorset.h"
#include "ioctls.h"
MODULE_AUTHOR("Swann Perarnau <swann.perarnau@imag.fr>");
MODULE_DESCRIPTION("Provides a debugfs file to mmap a physical address range.");
MODULE_LICENSE("GPL");

static unsigned long memory = 0;
static char *mem = "1k";
module_param(mem,charp,0);
MODULE_PARM_DESC(mem,"How much memory should I reserve in RAM.");
/* need it global because of cleanup code */
static unsigned int order = 0;
static struct class *ccontrol_class;
/* devices structures:
 * two types of devices are handled for ccontrol:
 *   - the control device which is always present.
 *   This device allow users to request or destroy
 *   a colored device using a simple syntax.
 *   - the colored devices: each time a user
 *   request a colored device it is created by this module. The user can then
 *   call mmap on it to access colored memory.
 *
 * Colored devices are short-lived where as the control one lives as long as the module.
 * Pages allocated to the module are saved in global memory (concurrent access are not handled
 * for now.
 * Permission to access the device are not implemented.
 */

/* this saves major and minor device numbers
 * used by all our devices.
 * The control device is minor 0, all other indices
 * are colored ones.
 */
#define MAX_DEVICES 64
#define DEVICES_DEFAULT_VALUE (MKDEV(MAJOR(MAJOR_NUM),MINOR(0)))
static dev_t devices_id = DEVICES_DEFAULT_VALUE;

/* colored devices are created with a fixed size (in pages).
 * Pages allocated to the device are saved into it (for fast retreival).
 * The struct also contain the colorset associated with this device and
 * the current number of pages associated with the device.
 * All colored devices are stored into a linked list.
 */
struct colored_dev {
	struct cdev cdev;
	unsigned int minor;
	unsigned int nbpages;
	struct page **pages;
	color_set colors;
	struct list_head devices;
};

/* the control device, structure only contains the head of the colored
 * devices list.*/
struct control_dev {
	struct cdev cdev;
	struct list_head devices;
};

struct control_dev control;
unsigned int nbdevices = 0;

/* Allocated Pages:
 * the kernel module reserves physical memory by making BIG allocations (BIG enough
 * to contain at least a page for each color in the last level cache.
 * Thoses bigs allocations are called heads and are of size 2**order pages.
 *
 * Once reserved, an head is split into pages and a list of all the pages of the same
 * color is saved into a global array.
 *
 * Heads need to be saved independently from colors because all heads do not start
 * at the same color.
 */

struct page** pages[LL_NUM_COLORS];
unsigned int nbpages[LL_NUM_COLORS];
struct page* *heads = NULL;
unsigned int nbheads  = 0;


/* Operations for the colored devices:
 * on each page fault, the corresponding physical page
 * is returned.
 * The page array is set on device creation.
 */

/* page fault handling. The offset in vmf tell us which page
 * in the array we should return, making a really fast page fault
 */
int colored_vma_fault(struct vm_area_struct *vma, struct vm_fault *vmf)
{
	unsigned long offset = 0;
	int ret = VM_FAULT_ERROR, err = 0;
	struct colored_dev *dev = vma->vm_private_data;
	struct page * page = NULL;
	vmf->page = NULL;

	offset =(unsigned int)vmf->pgoff;
	if(offset >= dev->nbpages)
		goto out;

	page = dev->pages[offset];

	// insert page into userspace
	err = vm_insert_page(vma,(unsigned long)vmf->virtual_address,page);
	if(err)
		goto out;
	ret = VM_FAULT_NOPAGE;
out:
	return ret;
}

struct vm_operations_struct colored_vm_ops = {
	.fault = colored_vma_fault,
};

/* on open, we transfer the struct colored_dev to the file pointer */
int colored_open(struct inode *inode, struct file *filp)
{
	struct colored_dev *dev;
	dev = container_of(inode->i_cdev, struct colored_dev, cdev);
	filp->private_data = dev;
	return 0;
}

/* on mmap we check some arguments (size and no MAP_SHARED, then
 * we transfer control to vma operations and the struct colored_dev
 * is passed to vma info
 */
int colored_mmap(struct file *filp, struct vm_area_struct *vma)
{
	struct colored_dev *dev = filp->private_data;
	size_t size;
	// check for no offset
	if(vma->vm_pgoff != 0)
	{
		printk(KERN_ERR "ccontrol: no mmap offset is allowed, you asked for %lu.\n",vma->vm_pgoff);
		return -EPERM;
	}

	// check size is ok
	size = (vma->vm_end - vma->vm_start)/PAGE_SIZE;
	if(size > dev->nbpages)
	{
		printk(KERN_ERR "ccontrol: mmap too big, you asked %zu, available %d.\n",
			size, dev->nbpages);
		return -ENOMEM;
	}
	// check MAP_SHARED is not asked
	if(!(vma->vm_flags & VM_SHARED)) {
		printk(KERN_ERR "ccontrol: you should not ask for a private mapping");
		return -EPERM;
	}
	vma->vm_ops = &colored_vm_ops;
	vma->vm_flags |= VM_RESERVED | VM_CAN_NONLINEAR;
	vma->vm_private_data = filp->private_data;
	printk(KERN_INFO "ccontrol: mmap ok\n");
	return 0;
}

static struct file_operations colored_fops = {
	.owner = THIS_MODULE,
	.open = colored_open,
	.mmap = colored_mmap,
};

/* devices helpers:
 */
int create_colored(struct colored_dev **dev, color_set cset, size_t size)
{
	int i;
	size_t num = 0;
	unsigned long pfn;
	struct page * tmp;
	/* allocate device */
	*dev = kmalloc(sizeof(struct colored_dev),GFP_KERNEL);
	if(*dev == NULL)
	{
		printk(KERN_ERR "ccontrol: kmalloc failed in create_colored\n");
		return -ENOMEM;
	}
	/* convert size to num pages */
	if(size % PAGE_SIZE != 0)
		size += PAGE_SIZE - (size % PAGE_SIZE);
	size = size / PAGE_SIZE;

	(*dev)->pages = vmalloc(sizeof(struct page *)*size);
	if((*dev)->pages == NULL)
	{
		printk(KERN_ERR "ccontrol: vmalloc failed in create_colored, asked %zu page pointers\n",size);
		goto free_dev;
	}
	printk(KERN_INFO "ccontrol: allocating %zu pages to new device.\n",size);
	(*dev)->nbpages = 0;
	/* give it pages:
	 * WARNING: we fail to allocate a device if a single
	 * color has not enough pages. This is intended behavior:
	 * we want reproducible allocations, not something leading to
	 * a color to be too much represented (that would cause unecessary
	 * conflict misses in cache).*/
	while(1)
	{
		for(i = 0; i < LL_NUM_COLORS; i++)
			if(COLOR_ISSET(i,&cset))
			{
				if(nbpages[i] > 0)
				{
					nbpages[i]--;
					(*dev)->pages[num++] = pages[i][nbpages[i]];
					if(num == size)
						goto out;
				}
				else
				{
					printk(KERN_ERR "ccontrol: color %d unavailable\n",i);
					goto free_pages;
				}
			}
	}
out:
	(*dev)->nbpages = num;
	printk(KERN_INFO "ccontrol: new device ready, %zu pages in it.\n",num);
	return 0;

free_pages:
	for(i = 0; i < num; i++)
	{
		tmp = (*dev)->pages[i];
		pfn = page_to_pfn(tmp);
		pages[PFN_TO_COLOR(pfn)][nbpages[PFN_TO_COLOR(pfn)]++] = tmp;
	}
	vfree((*dev)->pages);
free_dev:
	kfree(*dev);
	printk(KERN_ERR "ccontrol: create_colored failed\n");
	return -ENOMEM;
}

void free_colored(struct colored_dev *dev)
{
	/* reclaim pages */
	unsigned int color,i;
	unsigned long pfn;
	printk(KERN_INFO "ccontrol: freeing device with %u pages.\n",dev->nbpages);
	for(i = 0; i < dev->nbpages; i++)
	{
		pfn = page_to_pfn(dev->pages[i]);
		color = PFN_TO_COLOR(pfn);
		pages[color][nbpages[color]++] = dev->pages[i];
	}
	/* free device */
	kfree(dev);
}

/* Control device operations:
 * only open, close and ioctl are allowed.
 *
 * This device receives commands asking for the creation
 * and deletion of colored devices. These commands
 * are defined as a set of ioctl.
 * See the ioctls.h header for their definition.
 */

/* on open, we transfer the struct colored_dev to the file pointer */
int control_open(struct inode *inode, struct file *filp)
{
	struct control_dev *dev;
	dev = container_of(inode->i_cdev, struct control_dev, cdev);
	filp->private_data = dev;
	return 0;
}

static int ioctl_new(ioctl_args *arg)
{

	int err;
	struct colored_dev *dev;
	dev_t devno;
	printk(KERN_INFO "ccontrol: ioctl, new device asked.\n");
	/* create colored device */
	err = create_colored(&dev,arg->c, arg->size);
	if(err) return err;

	/* register it */
	devno = MKDEV(MAJOR(devices_id),MINOR(devices_id) + nbdevices +1);
	cdev_init(&dev->cdev,&colored_fops);
	dev->cdev.owner = THIS_MODULE;
	dev->cdev.ops = &colored_fops;
	err = cdev_add(&dev->cdev, devno,1);
	if(err)
	{
		/* cleanup device */
		printk(KERN_ERR "ccontrol: cdev_add failed in ioctl_new, errcode %d\n",err);
		free_colored(dev);
		return err;
	}

	nbdevices++;
	/* add it to the list */
	dev->minor = MINOR(devices_id)+nbdevices;
	list_add(&(dev->devices),&control.devices);

	/* return device number */
	memset(arg,0,sizeof(ioctl_args));
	arg->major = MAJOR(devno);
	arg->minor = MINOR(devno);
	return 0;
}


static int ioctl_free(ioctl_args *arg)
{
	int found = 0;
	struct colored_dev *cur,*tmp;
	printk(KERN_INFO "ccontrol: ioctl, asked to remove device.\n");
	/* find colored device */
	list_for_each_entry_safe(cur,tmp,&control.devices,devices)
	{
		if(cur->minor == arg->minor)
		{
			list_del(&cur->devices);
			found = 1;
			break;
		}
	}
	if(!found)
	{
		printk(KERN_ERR "ccontrol: invalid device minor %d\n",arg->minor);
		return -EINVAL;
	}
	/* free it */
	free_colored(cur);
	return 0;
}

/* handles ioctl on the device, see ioctls.h for available values */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
long control_ioctl(struct file *filp, unsigned int code, unsigned long val)
#else
int control_ioctl(struct inode *inode, struct file *filp, unsigned int code, unsigned long val)
#endif
{
	void __user *argp = (void __user *)val;
	ioctl_args local;
	int err;
	switch(code) {
		case IOCTL_NEW:
			/* create a new colored device: val contain a pointer
			 * to the colorset and the wanted device size
			 */
			err = copy_from_user(&local,argp,sizeof(ioctl_args));
			if(err)
			{
				printk(KERN_ERR "ccontrol: copy_from_user failed %p, errcode : %d\n",argp,err);
				return -EFAULT;
			}
			/* now that the params are ok, do real work */
			err = ioctl_new(&local);
			if(err) return err;

			/* push back the return value */
			err = copy_to_user(argp,(void *)&local,sizeof(ioctl_args));
			if(err)
			{
				printk(KERN_ERR "ccontrol: copy_to_user failed %p, errcode : %d\n",argp,err);
				/* special case: if we can't give info to the user we
				 * free the device immediately
				 */
				ioctl_free(&local);
				return err;
			}

			break;
		case IOCTL_FREE:
			/* deletes a device, reclaiming its pages for the module
			 */
			err = copy_from_user(&local,argp,sizeof(ioctl_args));
			if(err)
			{
				printk(KERN_ERR "ccontrol: copy_from_user failed %p, errcode : %d\n",argp,err);
				return -EFAULT;
			}

			/* now that the params are ok, do real work */
			err = ioctl_free(&local);
			if(err) return err;

			break;
		default:
			printk(KERN_ERR "ccontrol: invalid opcode %u\n",code);
			return -EINVAL;
	}
	return 0;
}

static struct file_operations control_fops = {
	.owner = THIS_MODULE,
	.open = control_open,
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,36)
	.unlocked_ioctl = control_ioctl,
#else
	.ioctl = control_ioctl,
#endif
};


/* module functions,
 * handle initialization, cleanup, etc
 */

/* allocates the pages and heads arrays,
 * uses the number of heads that will be reserved.
 * NOTE: at most two pages of the same color can appear
 * in an head.
 */
int alloc_pagetable(unsigned int nbh)
{
	int i;
	// just to make sure everything is ok
	for(i = 0; i < LL_NUM_COLORS; i++)
		pages[i] = NULL;

	for(i = 0; i < LL_NUM_COLORS; i++)
	{
		pages[i] = (struct page **) vmalloc(nbh*2*sizeof(struct page *));
		if(!pages[i])
			return -ENOMEM;
	}

	heads = (struct page **) kmalloc(nbh*sizeof(struct page *),GFP_KERNEL);
	if(!heads)
		return -ENOMEM;

	return 0;
}

/* free in kernel memory used for saving page information.
 * WARNING: this code expects heads to have already been freed.
 */
void clean_pagetable(void)
{
	int i;
	if(heads)
		kfree((void *)heads);

	for(i = 0; i < LL_NUM_COLORS; i++)
		if(pages[i] != NULL)
			vfree((void *)pages[i]);
}

/* allocates the char device numbers and creates the first device */
int alloc_devices(void)
{
	int err;
	dev_t devno;

	/* allocate character device numbers */
	err = alloc_chrdev_region(&devices_id,0,MAX_DEVICES,"ccontrol");
	if(err)
		return err;

	printk(KERN_INFO "ccontrol: Got %d device major.\n",MAJOR(devices_id));
	printk(KERN_INFO "ccontrol: Allocated %d minor devices, starting from %d.\n",
		MAX_DEVICES,MINOR(devices_id));

	/* create first device */
	devno = MKDEV(MAJOR(devices_id),MINOR(devices_id));
	cdev_init(&control.cdev,&control_fops);
	control.cdev.owner = THIS_MODULE;
	control.cdev.ops = &control_fops;
	err = cdev_add(&control.cdev, devno,1);
	if(err)
	{
		printk(KERN_ERR "Error %d adding control device.\n",err);
		goto error;
	}
	ccontrol_class = class_create(THIS_MODULE,"ccontrol");
	if(IS_ERR(ccontrol_class))
	{
		printk(KERN_ERR "Error create ccontrol class.\n");
		cdev_del(&control.cdev);
		err = PTR_ERR(ccontrol_class);
		goto error;
	}
	device_create(ccontrol_class,NULL, devno,NULL,"ccontrol");
	return 0;
error:
	unregister_chrdev_region(devices_id,MAX_DEVICES);
	devices_id = DEVICES_DEFAULT_VALUE;
	return err;
}

/* deletes all devices */
void clean_devices(void)
{
	/* clear all colored devices */
	struct colored_dev *cur,*tmp;
	list_for_each_entry_safe(cur,tmp,&control.devices,devices)
	{
		/* free each entry and remove it from list */
		list_del(&cur->devices);
		cdev_del(&cur->cdev);
		free_colored(cur);
	}

	/* free the device number region */
	if(devices_id != DEVICES_DEFAULT_VALUE)
	{
		device_destroy(ccontrol_class,MKDEV(MAJOR_NUM,0));
		class_destroy(ccontrol_class);
		cdev_del(&control.cdev);
		unregister_chrdev_region(devices_id,MAX_DEVICES);
	}
}

/* reserves physical memory */
int reserve_memory(unsigned int nbh)
{
	int i,j;
	struct page *page,*nth;
	unsigned long pfn;
	unsigned int color;
	unsigned int nids[MAX_NUMNODES];
	for(i = 0; i < MAX_NUMNODES; i++)
		if(node_online(i)) nids[i] = 0;

	for(i = 0; i < nbh; i++)
	{
		// allocate an head
		page = alloc_pages(GFP_HIGHUSER | __GFP_COMP,order);
		if(!page) {
			printk(KERN_INFO
				"ccontrol: failed to get a page");
			return -ENOMEM;
		}
		heads[nbheads++] = page;
		nids[page_to_nid(page)]++;
		// split the head into colors
		for(j = 0; j < 1<<order; j++)
		{
			nth = nth_page(page,j);
			pfn = page_to_pfn(nth);
			color = PFN_TO_COLOR(pfn);
			pages[color][nbpages[color]++] = nth;
		}
	}
	for(i = 0; i < MAX_NUMNODES; i++)
		if(node_online(i))
			printk(KERN_INFO "ccontrol: numa node %d gave us %u blocks\n",i,nids[i]);

	return 0;
}

/* frees physical memory */
void free_memory(void)
{
	int i;
	if(heads)
		for(i = 0; i < nbheads; i++)
			__free_pages(heads[i],order);
}

void cleanup(void)
{
	clean_devices();
	free_memory();
	clean_pagetable();
}

static int __init init(void)
{
	int err = 0;
	unsigned int blocks;
	printk("ccontrol: started !\n");
	order = get_order(LL_NUM_COLORS*PAGE_SIZE);
	if(order < 0)
	{
		printk(KERN_ERR "ccontrol: cannot find a good physical page block size.\n");
		return -EPERM;
	}
	printk(KERN_INFO "ccontrol: each block is %lu ko wide.\n",(PAGE_SIZE * (1<<order))/1024);

	/* parse mem into a memory size */
	memory = memparse(mem,NULL);

	// compute the number of blocks we should allocate to reserve enough memory.
	blocks = memory / (PAGE_SIZE * (1<<order));
	if(blocks * (PAGE_SIZE * (1<<order)) < memory)
		blocks++;

	printk(KERN_DEBUG "ccontrol: will allocate %d blocks of order %d.\n",blocks,order);

	/* clear control struct, this must be done before any errors */
	memset(&control,0,sizeof(struct control_dev));
	INIT_LIST_HEAD(&control.devices);

	err = alloc_pagetable(blocks);
	if(err)
		goto error;
	printk(KERN_DEBUG "ccontrol: pages table correctly allocated.\n");

	err = reserve_memory(blocks);
	if(err)
		goto error;

	err = alloc_devices();
	if(err)
		goto error;
	printk("ccontrol: correctly initialized.\n");
	return 0;
error:
	cleanup();
	return err;
}

static void __exit exit(void)
{
	cleanup();
	printk("ccontrol: stopped !\n");
}

/* Kernel Macros
 */

module_init(init);
module_exit(exit);
