#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"

int main(int argc, char *argv[])
{
	if (argc < 2) {
		printf(2, "usage: <%s> name\n", argv[0]);
		exit();
	}

	hello_name(argv[1]);
	exit();
}
