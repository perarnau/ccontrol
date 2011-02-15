/* random writes inside a colored memory region */
#include <stdlib.h>
#include <stdio.h>
#include <error.h>
#include <assert.h>

#include <ccontrol.h>
/* random number generator: a bad one but good enough for
 * our use. Why a custom one ? to speed up tests !
 */
static unsigned long state = 1;
int myrand(unsigned int size) {
	state = state* 1103515245 + 12345;
	return (state%size);
}

int myseed(unsigned int s) {
	state = s;
}

/* we are forced to print a sum to stop gcc from removing the whole code */
void writes(char *array, unsigned int nbwrites, unsigned int size) {
	char *p;
	volatile unsigned long sum = 0;
	unsigned int i,j;
	myseed(1);
	for(j= 0;j < nbwrites;j++) {
		unsigned int t = myrand(size);
		sum += array[t];
	}
	//fprintf(stderr,"sum: %lu.\n",sum);
}

int main(int argc, char** argv) {
	unsigned long arg;
	unsigned long size;
	unsigned int accesses;
	char *t;
	int i;
	struct ccontrol_zone *z;
	color_set c;
	if(argc != 2) {
		fprintf(stderr,"usage: %s <sizein2**>\n",argv[0]);
		exit(1);
	}
	arg = atoi(argv[1]);
	size = 1 << arg;
	/* make enough accesses to ensure the whole region is tested */
	accesses = size*arg;

	/* allocate region */
	COLOR_ZERO(&c);
	for(i = 0; i < 32; i++)
		COLOR_SET(i,&c);

	z = ccontrol_new();
	assert(z!=NULL);
	i = ccontrol_create_zone(z,&c,size);
	assert(i==0);
	t = ccontrol_malloc(z,size);
	assert(t!=NULL);
	writes(t,accesses,size);

	ccontrol_free(z,t);
	ccontrol_destroy_zone(z);
	ccontrol_delete(z);
	return 0;
}
