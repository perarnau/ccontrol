#include "exp.h"
#include<ccontrol.h>
#include<stdlib.h>

#define CACHE_LINESIZE (64)
//#define D (419430)
//#define D (1677721)
#define D (335488)
#define H 300
#define H1 (H/4)
#define H2 (H/2)
#define H3 (H)

#ifndef REPEATS
#define REPEATS 10
#endif

typedef struct {
	double value;
	char pad[CACHE_LINESIZE -sizeof(double)];
} cell;

#define L3_SIZE (D/sizeof(cell))
#define R_SIZE (L3_SIZE)
#define L2_SIZE (L3_SIZE/2)
#define L1_SIZE (L3_SIZE/4)

cell *l1 = NULL;
cell *l2 = NULL;
cell *l3 = NULL;
cell *r = NULL;


void stencil()
{
	int i,j;
	int p,q;
	double t;
	cell *c;
	for(i = 8; i < H-8; i++)
	{
		for(j = 8; j < R_SIZE-8; j++)
		{
			c = &r[i*R_SIZE+j];
			t=0;
			/* access L1 */
			p = i/4;
			q = j/4;
			t += l1[(p-2)*L1_SIZE+q].value;
			t += l1[(p-1)*L1_SIZE+q].value;
			t += l1[p*L1_SIZE+q].value;
			t += l1[(p+1)*L1_SIZE+q].value;
			t += l1[(p+2)*L1_SIZE+q].value;
			t += l1[p*L1_SIZE+q-2].value;
			t += l1[p*L1_SIZE+q-1].value;
			t += l1[p*L1_SIZE+q+1].value;
			t += l1[p*L1_SIZE+q+2].value;
			/*value access L1 */ 
			p = i/2;
			q = j/2;
			t += l2[(p-2)*L2_SIZE+q].value;
			t += l2[(p-1)*L2_SIZE+q].value;
			t += l2[p*L2_SIZE+q].value;
			t += l2[(p+1)*L2_SIZE+q].value;
			t += l2[(p+2)*L2_SIZE+q].value;
			t += l2[p*L2_SIZE+q-2].value;
			t += l2[p*L2_SIZE+q-1].value;
			t += l2[p*L2_SIZE+q+1].value;
			t += l2[p*L2_SIZE+q+2].value;
			/*value access L1 */
			p = i;
			q = j;
			t += l3[(p-2)*L3_SIZE+q].value;
			t += l3[(p-1)*L3_SIZE+q].value;
			t += l3[p*L3_SIZE+q].value;
			t += l3[p*L3_SIZE+q-2].value;
			t += l3[p*L3_SIZE+q-1].value;
			t += l3[p*L3_SIZE+q+1].value;
			t += l3[p*L3_SIZE+q+2].value;
			t += l3[(p+1)*L3_SIZE+q].value;
			t += l3[(p+2)*L3_SIZE+q].value;
			c->value = t;
		}
	}
}


int main(int argc, char **argv)
{
	int i;
	double sum;
	int mapping[4];
	size_t sizes[4];
	struct ccontrol_zone *z[4];
	color_set cset[4];
	size_t total_size[4];
	unsigned int nballoc[4];
	BEGIN_MAIN
	if(sizeof(cell) != CACHE_LINESIZE)
	{
		fprintf(stderr,"error with cell size");
		exit(1);
	}
	/* init sizes */
	for(i=0; i < 4; i++)
	{
		mapping[i] = atoi(argv[1+i]);
		sizes[i] = atoi(argv[5+i]);
		total_size[i] = 0;
		nballoc[i] = 0;
		COLOR_ZERO(&cset[i]);
	}
	int j,k =0;
	for(i=0; i < 4; i++)
	{
		for(j = k; j < k + sizes[i]; j++)
			COLOR_SET(j,&cset[i]);
		k += sizes[i];
	}
	total_size[mapping[0]] += L1_SIZE*H1*sizeof(cell);
	total_size[mapping[1]] += L2_SIZE*H2*sizeof(cell);
	total_size[mapping[2]] += L3_SIZE*H3*sizeof(cell);
	total_size[mapping[3]] += R_SIZE*H*sizeof(cell);
	nballoc[mapping[0]]++;
	nballoc[mapping[1]]++;
	nballoc[mapping[2]]++;
	nballoc[mapping[3]]++;
	for(i = 0; i < 4; i++)
	{
		if(total_size[i] != 0)
		{
			total_size[i] = ccontrol_memsize2zonesize(nballoc[i],total_size[i]);
			z[i] = ccontrol_new();
			ccontrol_create_zone(z[i],&cset[i],total_size[i]);
		}
	}
	/* allocate structs */
	l1 = ccontrol_malloc(z[mapping[0]],L1_SIZE*H1*sizeof(cell));
	l2 = ccontrol_malloc(z[mapping[1]],L2_SIZE*H2*sizeof(cell));
	l3 = ccontrol_malloc(z[mapping[2]],L3_SIZE*H3*sizeof(cell));
	r = ccontrol_malloc(z[mapping[3]],R_SIZE*H*sizeof(cell));

	/* do our experiment */
	srand(0);
	for(i = 0; i < H1*L1_SIZE; i++)
		l1[i].value = ((double) rand())/((double)RAND_MAX);
	for(i = 0; i < H2*L2_SIZE; i++)
		l2[i].value = ((double) rand())/((double)RAND_MAX);
	for(i = 0; i < H3*L3_SIZE; i++)
		l3[i].value = ((double) rand())/((double)RAND_MAX);
	for(i = 0; i < H*R_SIZE; i++)
		r[i].value = ((double) rand())/((double)RAND_MAX);

	stencil();
	for(i = 1; i < REPEATS; i++)
	{
	BEGIN_EXPERIMENT
	stencil();
	END_EXPERIMENT
	}
	/* finish */
	// just to avoid gcc removing all our code */
	sum = 0;
	for(i= 0; i < R_SIZE*H; i++)
		sum += r[i].value;
	fprintf(stdout,"value: %lf\n",sum);
	for(i = 0; i< 4; i++)
	{
		if(total_size[i] != 0)
		{
			ccontrol_destroy_zone(z[i]);
			ccontrol_delete(z[i]);
		}
	}	
	END_MAIN
	return 0;
}
