/* freelist test code */
#include"freelist.h"

#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<assert.h>

int main()
{
	void *a,*b,*c;
	char t[500];
	void *mem = (void *)&t[0];
	size_t size = 500, max = 500 -ALLOCATOR_OVERHEAD;
	fprintf(stderr,"sizeof(size_t): %u\n",sizeof(size_t));
	fprintf(stderr,"sizeof(fl): %u\n",sizeof(fl));
	fprintf(stderr,"fl:size is %u, max should be %u\n",size,max);
	fl_init(mem,500);

	/* we don't have exactly 500 bytes available
	 * because of allocator overhead */
	fprintf(stderr,"fl:test too big\n");
	a = fl_allocate(mem,size);
	assert(a == NULL);

	/* one byte too big */
	fprintf(stderr,"fl:test one byte too much\n");
	a = fl_allocate(mem,max+1);
	assert(a == NULL);

	/* full alloc plus one alloc */
	fprintf(stderr,"fl:test alloc max, alloc\n");
	a = fl_allocate(mem,max);
	assert(a != NULL);
	memset(a,'a',max);
	b = fl_allocate(mem,1);
	assert(b == NULL);
	fl_free(mem,a);

	/* just alloc & free a single cell */
	fprintf(stderr,"fl:test alloc,free,alloc\n");
	a = fl_allocate(mem,max);
	assert(a != NULL);
	memset(a,'a',max);
	fl_free(mem,a);
	a = fl_allocate(mem,400);
	assert(a != NULL);
	memset(a,'a',400);

	/* alloc 3, free the middle one and realloc it */
	fprintf(stderr,"fl:test middle free\n");
	b = fl_allocate(mem,max-440);
	assert(b != NULL);
	memset(b,'b',max -440);
	c = fl_allocate(mem,20);
	assert(c != NULL);
	memset(c,'c',20);
	fl_free(mem,b);
	b = fl_allocate(mem,max-440);
	assert(b != NULL);
	memset(b,'b',max -440);
	fl_free(mem,a);
	fl_free(mem,b);
	fl_free(mem,c);
	return 0;
}
