#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "rand.h"

int main(void) {
	int i = 0;
	int max = 100;
	srand(20160548);

	for (i = 0; i < 100; ++i) {
		printf(1, "%d: %d\n", i, rand() % max);
	}

	exit();
}
