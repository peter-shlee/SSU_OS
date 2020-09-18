#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <utmpx.h>

#define BUFFER_SIZE 128
#define MAX_USER_NAME 32
#define INIT_LIST_SIZE 1024

void print_system_infos();
void print_current_time();
void print_running_time();
void print_user_count();
void print_load_average();
void print_mem_infos();

void print_process_infos();

int main(void) {

	print_system_infos();
	printf("\n");
	print_process_infos();

	return 0;
}

void print_system_infos() {
	printf("top - ");
	print_current_time();
	printf(" up ");
	print_running_time();
	printf(", ");
	print_user_count();
	printf(" user, ");
	printf("load average: ");
	print_load_average();


	printf("\n");

	printf("\n");

	print_mem_infos();
	printf("\n");

	return;
}

void print_current_time() {
	time_t raw_time;
	struct tm *time_info;
	char *time_text;
	char current_time_text[8];

	time(&raw_time);
	time_info = localtime(&raw_time);
	time_text = asctime(time_info);
	strncpy(current_time_text, time_text + 11, sizeof(current_time_text) / sizeof(current_time_text[0]));
	printf("%s", current_time_text);

	return;
}

void print_running_time() {
	// ~ min
	// ~:~~
	FILE *fp;
	const char* fname = "/proc/uptime";
	const char* mode = "r";
	float fuptime;
	int hour = 0;
	int minute = 0;

	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		exit(1);
	}
	fscanf(fp, "%f", &fuptime);
	fclose(fp);

	hour = fuptime / (60 * 60);
	minute = (fuptime - (hour *60 * 60)) / 60;

	if (hour == 0) {
		printf("%d min", minute);
	} else {
		printf("%d:%02d", hour, minute);
	}

	return;
}

void print_user_count() {
	struct utmpx *utmpxp;
	int logged_in_user_count = 0;

	setutxent();
	while ((utmpxp = getutxent()) != NULL) {
		if (utmpxp->ut_type == USER_PROCESS) {
			++logged_in_user_count;
		}
	}
	endutxent();

	printf("%d", logged_in_user_count);

	return;
}

void print_load_average() {
	FILE *fp;
	const char *fname = "/proc/loadavg";
	const char* mode = "r";
	float _1_min_avg, _5_min_avg, _15_min_avg;

	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		exit(1);
	}

	fscanf(fp, "%f%f%f", &_1_min_avg, &_5_min_avg, &_15_min_avg);
	fclose(fp);

	printf("%.2f, %.2f, %.2f", _1_min_avg, _5_min_avg, _15_min_avg);

	return;
}

void print_mem_infos() {
	FILE *fp;
	const char *fname = "/proc/meminfo";
	const char *mode = "r";
	int mem_total, mem_free, mem_used, mem_cached, mem_buffers, mem_SReclaimable, mem_available, mem_cache;
	int swap_total, swap_free, swap_used;
	char tmp[BUFFER_SIZE];
	int i;
	
	// used -> total - free - buffers - cache
	// buffers -> Buffers in /proc/meminfo
	// cache -> Cached and SReclaimable in /proc/meminfo

	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		exit(1);
	}

	fscanf(fp, "%s%d%s", tmp, &mem_total, tmp);
	fscanf(fp, "%s%d%s", tmp, &mem_free, tmp);
	fscanf(fp, "%s%d%s", tmp, &mem_available, tmp);
	fscanf(fp, "%s%d%s", tmp, &mem_buffers, tmp);
	fscanf(fp, "%s%d%s", tmp, &mem_cached, tmp);
	for (i = 0; i < 9; ++i)
		fscanf(fp, "%s%s%s", tmp, tmp, tmp);
	fscanf(fp, "%s%d%s", tmp, &swap_total, tmp);
	fscanf(fp, "%s%d%s", tmp, &swap_free, tmp);
	for (i = 0; i < 7; ++i)
		fscanf(fp, "%s%s%s", tmp, tmp, tmp);
	fscanf(fp, "%s%d%s", tmp, &mem_SReclaimable, tmp);

	fclose(fp);

	mem_cache = mem_cached + mem_SReclaimable;
	mem_used = mem_total - mem_free - mem_buffers - mem_cache;
	swap_used = swap_total - swap_free;
	printf("KiB Mem : %d total, %d free, %d used, %d buff/cache\n", mem_total, mem_free, mem_used, mem_buffers + mem_cache);
	printf("KiB Swap: %d total, %d free, %d used, %d avail Mem\n", swap_total, swap_free, swap_used, mem_available);

}
///////////////////////////////////////////////////////

typedef struct _task_info {
	pid_t pid;
	char user_name[MAX_USER_NAME + 1];
	int pr;
	int ni;
	int virt;
	int res;
	int shr;
	int s;
	float cpu;
	float mem;
	int time;
	char command[BUFFER_SIZE];
} Task_info;

struct _task_list {
	Task_info **list;
	int len;
	int size;
} Task_list;

typedef struct _simple_task_info {
	pid_t pid;
	float cpu;
} Simple_task_info;

struct _simple_task_list {
	Simple_task_info **list;
	int len;
	int size;
} Simple_task_list;

void free_task_list() {
	if (Task_list.list != NULL) {
		int i;
		for (i = 0; i < Task_list.len; ++i) {
			free(Task_list.list[i]);
		}
		free(Task_list.list);

		Task_list.list = NULL;
		Task_list.len = 0;
		Task_list.size = 0;
	}

	return;
}

void init_task_list() {
	free_task_list();
	Task_list.list = (Task_info **)malloc(INIT_LIST_SIZE * sizeof(Task_info));
	if (Task_list.list == NULL) {
		fprintf(stderr, "malloc error in init_task_list\n");
		exit(1);
	}
	Task_list.len = 0;
	Task_list.size = INIT_LIST_SIZE;

	return;
}

void append_to_task_list(Task_info* new_info) {
	if (Task_list.len == Task_list.size) {
		Task_list.size *= 2;
		Task_list.list = (Task_info **)realloc(Task_list.list, Task_list.size * sizeof(Task_info *));
		if (Task_list.list == NULL) {
			fprintf(stderr, "realloc error in append_to_task_list\n");
			exit(1);
		}
	}
	Task_list.list[(Task_list.len)++] = new_info;

	return;
}

void free_simple_task_list() {
	if (Simple_task_list.list != NULL) {
		int i;
		for (i = 0; i < Simple_task_list.len; ++i) {
			free(Simple_task_list.list[i]);
		}
		free(Simple_task_list.list);

		Simple_task_list.list = NULL;
		Simple_task_list.len = 0;
		Simple_task_list.size = 0;
	}

	return;
}

void init_simple_task_list() {
	free_simple_task_list();
	Simple_task_list.list = (Simple_task_info **)malloc(INIT_LIST_SIZE * sizeof(Simple_task_info *));
	if (Simple_task_list.list == NULL) {
		fprintf(stderr, "malloc error in init_simple_task_list\n");
		exit(1);
	}
	Simple_task_list.len = 0;
	Simple_task_list.size = INIT_LIST_SIZE;

	return;
}

void append_to_simple_task_list(Simple_task_info *new_info) {
	if (Simple_task_list.len == Simple_task_list.size) {
		Simple_task_list.size *= 2;
		Simple_task_list.list = (Simple_task_info **)realloc(Simple_task_list.list, Simple_task_list.size * sizeof(Simple_task_info *));
		if (Simple_task_list.list == NULL) {
			fprintf(stderr, "realloc error in append_to_simple_task_list\n");
			exit(1);
		}
	}
	Simple_task_list.list[(Simple_task_list.len)++] = new_info;

	return;
}

Task_info *make_new_task_info(pid_t pid) {
	Task_info *new_info = (Task_info *)malloc(sizeof(Task_info));
	if (new_info == NULL) {
		fprintf(stderr, "malloc error in make_new_task_info\n");
		exit(1);
	}
	new_info->pid = pid;

	return new_info;
}

Simple_task_info *make_new_simple_task_info(pid_t pid) {
	Simple_task_info *new_info = (Simple_task_info *)malloc(sizeof(Simple_task_info));
	if (new_info == NULL) {
		fprintf(stderr, "malloc error in make_new_simple_task_info\n");
		exit(1);
	}
	new_info->pid = pid;

	return new_info;
}
//////////////////////////////////////////////////////////////


void update_tasks_status() {
	//PID - directory name
	//USER - getpwnam(/proc/$pid/loginuid)
	//PR - stat
	//NI - stat
	//VIRT - status (VmSize)
	//RES - status (VmRSS)
	//SHR - statm (shared * pagesize(getpagesize()) / 1024(KiB))
	//S - stat
	//%CPU - (timeTotalAfter - timeTotalBefore (from /proc/stat)) - (usageSystemMode + usageUserMode)
	//%MEM - (RES - SHR) / mem_total(/proc/meminfo) * 100
	//TIME+ - stime + utime
	//COMMAND - stat

}

void print_process_infos() {

}

void print_proecss_info(pid_t pid) {

}
