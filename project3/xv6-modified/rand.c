#include "rand.h"

static unsigned int x;
static unsigned int y;
static unsigned int z;
static unsigned int w;

unsigned int 
rand() {
	unsigned int t = x;
	unsigned int s = w;
	x = y;
	y = z;
	z = s;

	t ^= t << 11;
	t ^= t >> 8;
	
	return w = t ^ s ^ (s >> 19);
}

void 
srand(unsigned int seed) {
	int i = 0;
	x = seed;

	for (i = 0; i < 100; ++i) {
		rand();
	}
}
