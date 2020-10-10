#include "types.h"
#include "stat.h"
#include "user.h"
#include "fcntl.h"
#include "processInfo.h"

int main(void)
{
	struct processInfo pInfo;
	int pid;
	int max_pid;

	printf(1, "PID\tPPID\tSIZE\tNumber of Context Switch\n");
	max_pid = get_max_pid();
	for (pid = 0; pid <= max_pid; ++pid) {
		if (get_proc_info(pid, &pInfo) != -1) {
			printf(1, "%d\t%d\t%d\t%d\n", pid, pInfo.ppid, pInfo.psize, pInfo.numberContextSwitches);
		}
	}

    	exit();
}
