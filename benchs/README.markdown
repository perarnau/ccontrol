Benchmarks
==========

This directory contains code snippets to demonstrate how ccontrol can be used
to measure and optimize cache performance.

The exp.h file
==============

This file contains macros do use [PAPI](http://icl.cs.utk.edu/papi/) or the
`librt` to measure the performance of small code regions. See the `stencil`
benchmark for example use.

Multigrid Stencil
=================

This code is explained in details in the
[paper](http://moais.imag.fr/membres/swann.perarnau/pdfs/cacheics11.pdf).  The
basic idea behind this is to build an application comprised of 4 data
structures having different cache requirements.

Algorithm
---------

The algorithm implemented is the following : three inputs matrices are
allocated and filled with random data. Each cell of these matrices is a cache
line wide. This cell contains only a double value and padding data.

The matrix `M3` is the biggest one, its size is specified as `H` rows and
`L3_SIZE` columns. `M2` is a fourth of `M3` : `H/2` and `L3_SIZE/2`. `M1` is a
fourth of `M2`. A fourth matrix `R` is the same size as `M3`.

The only code region that matters and is analysed in the paper is similar to a
stencil algorithm : a _cross_ of 9 cells are summed in each of the M matrices
and the result is written to R. By _cross_ we mean something like that :

        ---------------------
       	|   |   | x |   |   |
       	---------------------
       	|   |   | x |   |   |
       	---------------------
       	| x | x | x | x | x |
       	---------------------
       	|   |   | x |   |   |
       	---------------------
       	|   |   | x |   |   |
       	---------------------

The cache requirement of each matrix is easy to visualise : if 5 lines fit in
cache, the program achieves maximum reuse. But since the result of a cell is
computed at once, in a standard setting the cache must save 3x5 lines
simultaneously. Note that there is no advantage to caching `R`, as it is only
written a line at a time.

Cache Optimization
------------------

Using CControl however it is possible to isolate each input matrix inside a
distinct cache partition, so that they all achieve maximum performance and do
not thrash each other.

In this application, we understand perfectly our cache requirement and could
thus just deduce the size (in colors) of each partition and be done with it.
The following only demonstrate what should be done if the requirement of a data
structure is less obvious.

Say we want to determine how much cache `M3` requires. One solution is to measure
the cache misses triggered by the application, starting with little cache for
`M3`, and increasing progressively the cache given to it.  As we increase the
cache given to the matrix,it will progressively fit better in cache until achieving
maximum performance. To provide a baseline against which a cache misses
variation can be seen, we isolate all the other data structure of fixed size (small) so that
the cache misses triggered by the rest of the application stay stable.
We will obtain something like that :


        Cache Misses
        ^
        |
        |
        |---------
        |         \
        |          \
        |           \
        |            --------------
        -----------------------------> Colors given to M3 partition

                    ^
                 This is the optimal partition size for M3










