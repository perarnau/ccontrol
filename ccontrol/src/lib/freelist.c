/*
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2 of the
 * License.
 *
 * Copyright (C) 2010 Swann Perarnau
 * Author: Swann Perarnau <swann.perarnau@imag.fr>
 */
#include"freelist.h"

/* this freelist is a classical in memory allocator:
 * the start of the memory contains a dummy fl:
 * head.size gives the available size.
 * it saves the size of available memory and the head
 * of the fl.
 *
 * This allocator applies a first fit to decide which memory
 * to allocate.
 */


/* due to the struct size, an allocation must have a minimum
 * size and be aligned on the struct. */
static size_t fl_adjustsize(size_t size)
{
	/* we need a header for free */
	size += HEADER_SIZE;
	/* align alloc on sizeof(fl) */
	if(size % sizeof(fl) != 0)
		size += sizeof(fl) - (size % sizeof(fl));
	return size;
}

/* find an adequate region for allocation,
 * if a region is found, prev has a value */
static void fl_findfit(fl * head, size_t size, fl **res,fl **prev)
{
	fl *it,*old;
	it = head->next;
	old = head;
	while(it != NULL)
	{
		if(it->size >= size)
		{
			*res = it;
			*prev = old;
			return;
		}
		old = it;
		it = it->next;
	}
	*res = NULL;
	*prev = NULL;
}

/* find the elt before f, f isn't inside the freelist. */
static fl* fl_findprevious(fl *head, fl *f)
{
	fl *it,*prev;
	it = head->next;
	prev = head;
	while(it != NULL)
	{
		if(it >  f)
			return prev;
		prev = it;
		it = it->next;
	}
	/* since we didn't find it before a fl, it must be after the last one */
	return prev;
}

/* initialize the memory zone as if it was the beginning of the memory region,
 * no argument checking.*/
int fl_init(void *z, size_t size)
{
	fl *head,*next;
	head = (fl *)z;
	next = head+1;
	next->size = size - sizeof(*head);
	next->next = NULL;
	head->size = size - sizeof(*head);
	head->next = next;
	return 0;
}

void *fl_allocate(void *z, size_t size)
{
	fl *f,*prev,*head;
	void *p;
	if(size == 0)
		return NULL;

	size = fl_adjustsize(size);
	head = (fl*)z;
	if(size > head->size)
		return NULL;

	/* find a fitting free zone */
	fl_findfit(head,size,&f,&prev);
	if(f == NULL)
		return NULL;

	/* update the zone */
	if(f->size - size < sizeof(fl))
	{
		prev->next = f->next;
		p = FL_TO_VOID(f);
	}
	else
	{
		/* resize the region, then jump to the new one */
		f->size -= size;
		f =(fl *)((char *) f + f->size);
		f->size = size;
		p = FL_TO_VOID(f);
	}
	/* update head size */
	head->size -= size;
	return p;
}


void fl_free(void *z, void *p)
{
	fl *f,*prev,*next,*head;
	if(p == NULL)
		return;

	f = VOID_TO_FL(p);
	f->next = NULL;
	head = (fl *)z;

	head->size += f->size;
	/* this function also merge the newly free region
	 * with the other ones
	 */

	/* find previous region  */
	prev = fl_findprevious(head,f);
	if(prev != head && (fl *)((char *)prev + prev->size) == f)
	{
		/* merge regions */
		prev->size += f->size;
		f = prev;
	}
	else
	{
		f->next = prev->next;
		prev->next = f;
	}
	/* merge next region if necessary */
	next = f->next;
	if(next != NULL && (fl *)((char *)f + f->size) == next)
	{
		f->size += next->size;
		f->next = next->next;
	}
}
