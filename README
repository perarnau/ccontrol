CControl : a software cache partitionning tool
==============================================

CControl (Cache Control) is a Linux kernel module and accompanying libraries to
control the amount of memory cache data structures inside an application can
use.

TL;DR
-----

Because a complete explanation of CControl requires knowledge of OS and
hardware cache internals, you should probably read what follows. For the lazy
bastards, here is one short explanation : 
	
	CControl provides a software cache partitioning library for Linux by
	implementing page coloring in kernel. The standard ./configure; make; 
	make install should work. Look at ccontrol.h for API doc.

You _must_ be root to install and load/unload ccontrol. This requirement is
lifted once ccontrol is loaded in kernel.


Background Knowledge
--------------------

During execution, a program makes a number of accesses to physical memory
(RAM). To increase global performance, small and fast memory banks called
caches are placed on the path between CPU and RAM. These caches store recently
accessed memory locations.

To understand how ccontrol work and what can (and cannot) be done with it, some
knowledge of cache internals are required.

* Virtual memory : on most architectures/OS a process only manipulates virtual
memory. Physical memory (RAM) is abstracted and shared by several processes at
the same time, but each process can only touch its own memory.  The OS
maintains the mapping between virtual and physical memory, with the help of
dedicated hardware to speed things up.

* Pages : both physical and virtual memory are split into pages, contiguous
blocks of memory. This pages are traditionally of 4 KB in size.

* Indexing : a cache identifies the memory locations accessed by computing a
hash function over their address (the _index_). Since both a virtual address
and a physical one can be used to refer to the same memory location, caches are
either _virtually_ or _physically_ indexed.

* Lines : saving memory one byte or one word at a time would be completely
inefficient for a cache. Instead, it saves a set of contiguous bytes in memory
under the same index. This set is called a line, and on most architectures it
is either 32 or 64 bytes wide.

* Associativity : each time the CPU accesses memory, the cache must find if 
the line in question is already known. Three configurations for this search
can be found nowadays : 
	* _direct-mapped_, an address has only one designated line it can be 
	saved to. Very fast to search, but not very efficient regarding some
	 memory access patterns.
	* _fully-associative_, an address can be saved in all the cache. Very
	slow to search but very efficient from a memory-optimization point of
	view.
	* _set-associative_, a mix of the two. An address in memory can be 
	saved in a set of lines in cache, allowing more flexibility regarding
	the memory access patterns benefiting from the cache and costing less
	than a fully-associative one.

* LRU : since a cache is smaller than RAM, each time a new memory location is
accessed (and thus a new line fetched) an old line must be evicted (of course
it only applies to associative caches). The most widely-known and efficient
algorithm to do that is Least-Recently-Used.

Page Coloring
-------------

The goal of ccontrol is to split the cache in several parts, allowing the user
to give some of its data structures more cache than others. Since hardware
cache partitioning is restricted to a few specific architectures, ccontrol does
that in software. The method used is called page coloring and is quite known is
the OS community.

	For any cache that is _not_ fully-associative, a color is 
	defined as the set of pages that occupy the same associative sets.

Put simply, in a set-associative cache, multiple lines map to the same
associative set. Since these lines are also part of pages, we can group these
pages by the associative sets they map to.

In the case of physically indexed caches, the OS is sole responsible for the
physical pages (and thus the colors) that a process touches, as it is in charge
of the mapping between virtual and physical pages.

CControl inject code inside the Linux kernel (module), reserve a part of the
RAM for itself, identify the colors of each page and redistribute them to
applications.  The only caveat is that an application can specify the colors it
wants to get back.

By limiting the colors available to the application, you limit the amount of
cache available. You can also split the cache in disjoint sets of colors  and
give some data structures their own partition to avoid cache pollution by bad
access patterns.

Using CControl
---------------

First, as root, load the kernel module :

	ccontrol load --mem 1G

This will reserve 1 GB of RAM for ccontrol and initialize page coloring.
You can look at `dmesg` for additional info.

If your application use the libccontrol library, you're done. Otherwise, you
can limit the total amount of cache used by _dynamically allocated data structures_
by using :

	ccontrol exec --ld-preload ./myapp

When using `LD_PRELOAD`, all standard memory allocation functions are
redirected to a single cache partition, of which you must specify the size (in
virtual memory) and the color set to use : corresponding options are `--pset`
and `--size`.

The `pset` option is a bitmask : a comma separated list of values.  For example
`--pset=0,1,8-10` will activate colors 0,1,8,9 and 10.

The `size` explains to ccontrol how much memory it should ask to the kernel
module. This must be lower than the amount of RAM allocated and fit the amount
of pages corresponding to the pset.

Once you're done with ccontrol, unload the module :

	ccontrol unload

If you want additional info on the cache characteristics detected by ccontrol,
you can use :

	ccontrol info

Library
-------

The `libccontrol` provides a easy interface to the cache control facilities.
It works as a custom memory allocation library : you create a _zone_ by asking
the kernel module for a set of colored pages. But first, you need a bookkeeping
structure :

	ccontrol.h

	/* allocates a zone */
	struct ccontrol_zone * ccontrol_new(void);

	/* frees a zone */
	void ccontrol_delete(struct ccontrol_zone *);

You then ask for zone creation :

	/* Creates a new memory colored zone.
	 * Needs a color set and a total size.
	 * WARNING: the memory allocator needs space for itself, make
	 * sure size is enough for him as well.
	 * Return 0 on success. */
	int ccontrol_create_zone(struct ccontrol_zone *, color_set *, size_t);

	/* Destroys a zone.
	 * Any allocation done inside it will no longer work.
	 */
	int ccontrol_destroy_zone(struct ccontrol_zone *);

Then you allocate memory inside a zone :

	/* Allocates memory inside the zone. Similar to POSIX malloc
	 */
	void *ccontrol_malloc(struct ccontrol_zone *, size_t);

	/* Frees memory from the zone. */
	void ccontrol_free(struct ccontrol_zone *, void *);

	/* realloc memory */
	void *ccontrol_realloc(struct ccontrol_zone *, void *, size_t);

The `color_set` structure is a bitmask indicating authorized colors :

	colorset.h

	color_set c;
	COLOR_ZERO(&c);
	COLOR_SET(1,&c);
	COLOR_CLR(1,&c);
	if(COLOR_ISSET(1,&c)) { }

Installing
---------

The classical `./configure ; make ; make install` should work
This project has no dependencies except for the Linux kernel headers
necessary to compile the kernel module.

The only supported install path (prefix) is `/usr` with root privileges.

The kernel module install rule does not understand the install prefix,
but there is a special variable named `INSTALL_MOD_PATH`, so the following will
work:

`INSTALL_MOD_PATH=prefix make install`

This is not recommended anyway, as modprobe will not find the module...

Bugs and Limitations
--------------------

Some architecture do not use the standard modulo hash function to index lines in cache. This function
is assumed by ccontrol when identifying the color of each page. If the architecture use something else,
the real color of what ccontrol gives to a process will be wrong. Unfortunately, that includes
Intel Sandy Bridge and newer cores. I'll buy a drink to anyone who can find the exact hash function
used in these.

The number of page of a given color is determined by the amount of RAM you give to the module. Since
ccontrol does not support swapping, this number of pages also determines the maximal size of an allocation
in a colored zone. Be careful not asking too much memory with too few pages available.

References
----------

This tool was the subject of a publication in the International Conference on
Supercomputing (ICS) in 2011. You can find the paper [here](http://moais.imag.fr/membres/swann.perarnau/pdfs/cacheics11.pdf).
This paper describe several possible uses of ccontrol to measure and optimize the cache performance of HPC applications.

If you want to cite this paper :

	Swann Perarnau, Marc Tchiboukdjian, and Guillaume Huard.
	Controlling cache utilization of hpc applications.
	In International Conference on Supercomputing (ICS), 2011.

You can also use this Bibtex :

	@inproceedings{ics11,
		title = {Controlling Cache Utilization of HPC Applications},
		author = {Swann Perarnau and Marc Tchiboukdjian and Guillaume Huard},
		booktitle = {International Conference on Supercomputing (ICS)},
		year = {2011}
	}
