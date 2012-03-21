#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <gsl/gsl_rng.h>
#include <gsl/gsl_randist.h>

#ifdef USE_CCONTROL
#include <ccontrol.h>
#endif

struct elem {
	int v;
	struct elem *n;
	char pad[64-sizeof(int)-sizeof(struct elem *)];
};

int main(int argc, char *argv[])
{
	unsigned long i,max;
	assert(argc >= 2);

	/* memory size as a power of 2 */
	unsigned int log = atoi(argv[1]);
	unsigned int size = 1<<log;
	assert(size > 0  && size < (1<<27));
	max = 3L*log*size;
	struct timespec start;
	struct timespec stop;
	cpu_set_t cset;
#ifdef USE_CCONTROL
	struct ccontrol_zone *z;
	color_set c;
	assert(argc == 3);
	size_t zone_size = size *sizeof(struct elem) + 64;

	/* use the first colors */
	COLOR_ZERO(&c);
	for(i = 0; i < atoi(argv[2]); i++)
		COLOR_SET(i,&c);

	z = ccontrol_new();
	ccontrol_create_zone(z,&c,zone_size);

	struct elem *tab  = ccontrol_malloc(z,size*sizeof(struct elem));
#else
	assert(argc == 2);
	struct elem *tab = malloc(size * sizeof(struct elem));
#endif
	assert(tab != NULL);
	/* linked list randomization */
	gsl_rng *r = gsl_rng_alloc(gsl_rng_mt19937);
	FILE *f = fopen("./rng_state","r");
	if(f)
	{
		assert(gsl_rng_fread(f,r) == 0);
		fclose(f);
	}
	for(i = 0; i < size; i++)
		tab[i].v = i;
	gsl_ran_shuffle(r,(void *)tab, size,sizeof(struct elem));
	struct elem *cur = &(tab[tab[0].v]);
	for(i= 0; i < size; i++)
	{
		cur->n = &(tab[tab[i].v]);
		cur = cur->n;
	}
	cur->n = &(tab[tab[0].v]);
	f = fopen("./rng_state","w+");
	assert(f);
	assert(gsl_rng_fwrite(f,r) == 0);
	fclose(f);
	gsl_rng_free(r);

	/* lock the program of first CPU */
	CPU_ZERO(&cset);
	CPU_SET(0,&cset);
	int err = sched_setaffinity(0,sizeof(cpu_set_t),&cset);
	if(err)
	{
		perror("setafstopity");
		exit(EXIT_FAILURE);
	}
	/* get sched_fifo priority, making us the sole process
	 * running on this CPU */
	struct sched_param p;
	p.sched_priority = sched_get_priority_min(SCHED_FIFO);
	err = sched_setscheduler(0,SCHED_FIFO,&p);
	if(err)
	{
		perror("setscheduler");
		exit(EXIT_FAILURE);
	}
#ifndef USE_CCONTROL
	/* lock memory pages in RAM */
	err = mlock((void *)tab,size*sizeof(struct elem));
	if(err)
	{
		perror("mlock");
		exit(EXIT_FAILURE);
	}
#endif
	/* volatile avoid dead code removal by gcc */
	volatile int somme=0;
	cur = &(tab[tab[0].v]);
	clock_gettime(CLOCK_REALTIME, &start);
	for (i = 0; i < max; ++i)
	{
		somme += cur->v;
		cur = cur->n;
	}
	clock_gettime(CLOCK_REALTIME, &stop);

	long long int time_nano=0;
	time_nano = (stop.tv_nsec - start.tv_nsec) +
		1e9* (stop.tv_sec - start.tv_sec);
#ifdef USE_CCONTROL
	ccontrol_free(z,tab);
	ccontrol_destroy_zone(z);
	ccontrol_delete(z);
#else
	munlock((void *)tab, size*sizeof(struct elem));
	free(tab);
#endif
	printf("%d %lu %lld\n", size, max, time_nano);
	return 0;
}
