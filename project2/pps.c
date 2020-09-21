#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/sysmacros.h>
#include <dirent.h>
#include <pwd.h>
#include <time.h>

#define INIT_LIST_SIZE 1024
#define BUFFER_SIZE 128

unsigned long uptime;

const int NANOS = 1000000000;
const int MILLIS = 1000;

typedef struct _Task_info {
	char user[10]; // 8글자 이상이면 7번째 문자까지만 표시하고 뒤에 +
	pid_t pid;
	float cpu;
	float mem;
	unsigned long vsz;
	unsigned long rss;
	char tty[20];
	char stat[10];
	char start[10];
	char time[10];
	char command[128];

//	int is_background_process;
//	int is_terminal_process;
} Task_info;

struct _task_list {
	Task_info **list;
	int len;
	int size;
} Task_list;

void update_uptime();
void print_list();
void sort_list_by_pid();
int compare_by_pid(const void *a, const void *b);
Task_info *make_new_task_info(pid_t pid);
void append_to_task_list(Task_info* new_info);
void init_task_list();
void free_task_list();
char *convert_time_format(unsigned long time);
char *convert_start_time(unsigned long long time);
void update_task_status();
unsigned long get_current_time();
void print_u_format(Task_info *t);
void print_a_x_format(Task_info *t);
void print_selected_options();
void print_default_format(Task_info *t);
unsigned long get_current_time();

typedef struct _Device_info {
	char name[MAXNAMLEN + 1];
	unsigned int major_nr;
	unsigned int minor_nr;
} Device_info;

struct _Device_list {
	Device_info **list;
	int len;
	int size;
} Device_list;

char cur_usr_name[BUFFER_SIZE];

void get_devices();
void free_device_list();
void init_device_list();
void append_to_device_list(Device_info* new_info);
void print_device_list();
void get_cur_usr_name();

void init_screen();
void check_options(int argc, char *argv[]);

int option_a = 0;
int option_u = 0;
int option_x = 0;

int page_size_in_KiB;

int main(int argc, char *argv[]) {
	int opt;

	//init_screen();

	page_size_in_KiB = getpagesize() / 1024;
	check_options(argc, argv);
	print_selected_options();

	get_cur_usr_name();
	update_uptime();
	init_device_list();
	get_devices();
	print_device_list();
	init_task_list();
	update_task_status();

	print_list();

	free_device_list();
	free_task_list();
	//endwin();

	return 0;
}

void init_screen() {
	initscr();

	return;
}

void check_options(int argc, char *argv[]) {
	int i, j;

	for (i = 1; i < argc; ++i) {
		int arg_len = strlen(argv[i]);
		for (j = 0; j < arg_len; ++j) {
			char arg_chr = argv[i][j];
			switch(arg_chr) {
				case 'a':
					option_a = 1;
					break;
				case 'u':
					option_u = 1;
					break;
				case 'x':
					option_x = 1;
					break;
				default:
					fprintf(stderr, "error: unsupported option\n");
					endwin();
					exit(1);
			}
		}
	}

	return;
}

void print_selected_options() {

	printf("selected options: ");
	if (option_a) {
		printf("a");
	}

	if (option_u) {
		printf("u");
	}

	if (option_x) {
		printf("x");
	}
	printf("\n");

	return;
}

////////////////////////////////////////////// task_info
void free_task_list() {
	if (Task_list.list != NULL) {
		int i;
		for (i = 0; i < Task_list.len; ++i) {
			if (Task_list.list[i] != NULL) {
				free(Task_list.list[i]);
			}
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
		endwin();
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
			endwin();
			exit(1);
		}
	}
	Task_list.list[(Task_list.len)++] = new_info;

	return;
}

Task_info *make_new_task_info(pid_t pid) {
	FILE *fp;
	char fname[MAXNAMLEN + 1];
	char tmp[BUFFER_SIZE];
	const char *mode = "r";
	float mem_total;
	int i;
	struct passwd *result;
	uid_t uid;
	pid_t session, pgrp, tpgid;
	long ni, num_thread, VmLck;
	unsigned long stime, utime;
	unsigned long long starttime;
	unsigned long user, nice, system, idle, lowait, irq, softirq, steal, guest, guest_nice;
	unsigned long cur_cpu_nonidle, cur_cpu_idle, cur_cpu_time;
	char *time_string;
	dev_t tty_nr;
	unsigned int major_nr, minor_nr;

	//printf("1111\n");
	Task_info *new_info = (Task_info *)malloc(sizeof(Task_info));
	if (new_info == NULL) {
		fprintf(stderr, "malloc error in make_new_task_info\n");
		endwin();
		exit(1);
	}
	new_info->pid = pid; // pid

	//printf("2222\n");
	/////
	sprintf(fname, "/proc/%d/stat", pid);
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}

	for(i = 0; i < 2; ++i)
		fscanf(fp, "%s", tmp);
	fscanf(fp, "%s", new_info->stat); // s
	fscanf(fp, "%s", tmp);
	fscanf(fp, "%d", &pgrp);
	fscanf(fp, "%d", &session);
	fscanf(fp, "%ld", &tty_nr);
	major_nr = major(tty_nr);
	minor_nr = minor(tty_nr);

	int tty_find_flag = 0;
	for(i = 0; i < Device_list.len; ++i) {
		if (Device_list.list[i]->major_nr == major_nr) {
			if (Device_list.list[i]->minor_nr == minor_nr) {
				tty_find_flag = 1;
				strncpy(new_info->tty, Device_list.list[i]->name, 18);
				new_info->tty[19] = '\0';
			}
		}
		//printf("ma: %d ,mi: %d, ma2: %d, mi2: %d\n", Device_list.list[i]->major_nr, Device_list.list[i]->minor_nr, major_nr, minor_nr);
	}
	if (!tty_find_flag) {
		strcpy(new_info->tty, "?");
	}

	fscanf(fp, "%d", &tpgid);
	for(i = 0; i < 5; ++i)
		fscanf(fp, "%s", tmp);
	fscanf(fp, "%lu%lu", &utime, &stime); // time
	time_string = convert_time_format(stime + utime);
	strcpy(new_info->time, time_string);
	free(time_string);
	for(i = 0; i < 3; ++i)
		fscanf(fp, "%s", tmp);
	fscanf(fp, "%ld", &ni); // ni
	fscanf(fp, "%ld", &num_thread); // num_thread
	fscanf(fp, "%s", tmp);
	fscanf(fp, "%llu", &starttime); // starttime
	fclose(fp);
	
	//printf("3333\n");
	/////
	sprintf(fname, "/proc/%d/loginuid", pid);
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%d", &uid);
	fclose(fp);

	if ((result = getpwuid(uid)) == NULL) {
		strcpy(new_info->user, "root");
	} else {
		if (strlen(result->pw_name) > 7) {
			strncpy(new_info->user, result->pw_name, 7);
			new_info->user[7] = '+';
			new_info->user[8] = '\0';
		} else {
			strcpy(new_info->user, result->pw_name);
		}
	} // USER

	//printf("4444\n");
	/////
	sprintf(fname, "/proc/%d/status", pid);
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%s%s", tmp, new_info->command); // COMMAND
	for (i = 0; i < 17; ++i)
		fgets(tmp, BUFFER_SIZE, fp);
	fscanf(fp, "%s%ld", tmp, &VmLck); // VmLck

	fclose(fp);

	//printf("5555\n");
	/////
	sprintf(fname, "/proc/%d/statm", pid);
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%lu%lu", &(new_info->vsz), &(new_info->rss));
	fclose(fp);
	new_info->vsz *= page_size_in_KiB; // virt
	new_info->rss *= page_size_in_KiB; // rss

	//printf("6666\n");
	/////
	sprintf(fname, "/proc/meminfo");
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%s%f", tmp, &mem_total); // mem
	fclose(fp);

	new_info->mem = (new_info->rss) / mem_total * 100.0;

	// S 정보 추가
	if (ni > 0) {
		strcat(new_info->stat, "N");
	} else if (ni < 0) {
		strcat(new_info->stat, "<");
	}

	if (VmLck > 0) {
		strcat(new_info->stat, "L"); // ---------------> 잘못됨
	}

	if (pid == session) {
		strcat(new_info->stat, "s");
	}

	if (num_thread > 1) {
		strcat(new_info->stat, "l");
	}

	if (pgrp == tpgid) {
		strcat(new_info->stat, "+");
	}

	// cpu 계산
	sprintf(fname, "/proc/stat");
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%s%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu", tmp, &user, &nice, &system, &idle, &lowait, &irq, &softirq, &steal, &guest, &guest_nice);
	fclose(fp);
	cur_cpu_nonidle = user + nice + system + irq + softirq + steal;
	cur_cpu_idle = idle + lowait;
	cur_cpu_time = cur_cpu_nonidle + cur_cpu_idle;
	new_info->cpu = ((float)(stime + utime) / (uptime * sysconf(_SC_CLK_TCK) - starttime)) * 100.0; 

	// start 계산
	time_string = convert_start_time(starttime);
	strcpy(new_info->start, time_string);
	free(time_string);

	//%CPU - (usageSystemMode + usageUserMode) / tick_count - ok
	//START - ok
	//TTY
	//S - stat - ok
	//       <, N -> NI - stat 이용
	//       L -> status-VmLck 이용
	//       s - 세션리더 stat-(6)session pid 이용
	//       l - 멀티스레드 - stat-(20)num_thread 이용
	//       + - foreground 프로세스 - stat-(5)pgrp과 stat-(8)tpgid 이용
	//PID - directory name - ok
	//TIME+ - stat - stime + utime - ok
	//USER - getpwnam(/proc/$pid/loginuid) - 7글자 수정 필요
	//COMMAND - status - ok
	//VIRT - statm (VmSize - 1) * pagesize(getpagesize()) / 1024(KiB)) - ok
	//RES - statm (VmRSS - 2) * pagesize(getpagesize()) / 1024(KiB)) - ok
	//%MEM - (RES - SHR) / mem_total(/proc/meminfo) * 100 - ok
	
//	char user[10]; // 8글자 이상이면 7번째 문자까지만 표시하고 뒤에 +
//	pid_t pid;
//	float cpu;
//	float mem;
//	unsigned long vsz;
//	unsigned long rss;
//	char tty[20];
//	char stat[10];
//	char start[10];
//	char time[10];
//	char command[128];

	return new_info;
}

int compare_by_pid(const void *a, const void *b) {
	return (*(Task_info **)a)->pid - (*(Task_info **)b)->pid;
}

void sort_list_by_pid() {
	qsort(Task_list.list, Task_list.len, sizeof(Task_info *), compare_by_pid);
}
//
//Task_info **find_element(pid_t pid) {
//	Task_info tmp_info;
//	Task_info *key;
//	if (!Task_list.is_sorted_by_pid) {
//		sort_list_by_pid();
//	}
//
//	tmp_info.pid = pid;
//	key = &tmp_info;
//	return bsearch(&key, Task_list.list, Task_list.len, sizeof(Task_info *), compare_by_pid);
//}
//
void print_list() {
	int i;
	char ts[1024];

	if (option_u) {
		sprintf(ts, "\n%-8s%5s%5s%5s %7s %6s %-7s %-4s%6s%8s %s", "USER", "PID", "%CPU", "%MEM", "VSZ", "RSS", "TTY", "STAT", "START", "TIME", "COMMAND");
		printf("%s", ts);
	} else if (option_a || option_x) {
		sprintf(ts, "\n%5s %-7s %-4s%8s %s", "PID", "TTY", "STAT", "TIME", "COMMAND");
		printf("%s", ts);

	} else {
		sprintf(ts, "\n%5s %-7s %8s %s", "PID", "TTY", "TIME", "COMMAND");
		printf("%s", ts);
	}

	for (i = 0; i < Task_list.len; ++i) {
		if(option_a && !option_x) {
			if (strcmp(Task_list.list[i]->tty, "?")) {
				if (option_u) {
					print_u_format(Task_list.list[i]);
				} else {
					print_a_x_format(Task_list.list[i]);
				}
			}
		} else if (!option_a && option_x) {
			if (!strcmp(Task_list.list[i]->user, cur_usr_name)) {
				if (option_u) {
					print_u_format(Task_list.list[i]);
				} else {
					print_a_x_format(Task_list.list[i]);
				}
			}
		} else if (option_a && option_x) {
			if (option_u) {
				print_u_format(Task_list.list[i]);
			} else {
				print_a_x_format(Task_list.list[i]);
			}
		} else {
			if (option_u) {

			} else {
				print_default_format(Task_list.list[i]);
			}
		}
	}
	printf("\n");

	return;
}

char *convert_time_format(unsigned long time){
	char *time_string;
	unsigned long tmp_seconds = time / sysconf(_SC_CLK_TCK);
	unsigned long minutes = tmp_seconds / 60;
	int seconds = (time - minutes * 60 * sysconf(_SC_CLK_TCK)) / sysconf(_SC_CLK_TCK);
	time_string = (char *)malloc(16 * sizeof(char));

	sprintf(time_string, "%lu:%02d", minutes, seconds);
	return time_string;
}

char *convert_start_time(unsigned long long starttime){
	time_t cur_time = time(NULL); 
	time_t running_time;
	time_t start_time;
	char *time_string;
	char *start_time_str;

	running_time = (time_t)(uptime - (starttime / sysconf(_SC_CLK_TCK)));
//	printf("runningtime : %ld\n", running_time);
//	printf("uptime : %ld\n", uptime);
//	printf("start time:%ld\n", starttime / sysconf(_SC_CLK_TCK));

	start_time = cur_time - running_time;
	start_time_str = ctime(&start_time);
	time_string = (char *)malloc(16 * sizeof(char));
	strncpy(time_string, start_time_str + 11, 5);
	time_string[5] = '\0';

	return time_string;
}

void update_uptime() {
	FILE *fp;
        const char* fname = "/proc/uptime";
        const char* mode = "r";
        float fuptime;

        if ((fp = fopen(fname, mode)) == NULL) {
                fprintf(stderr, "fopen error for %s\n", fname);
                endwin();
                exit(1);
        }
        fscanf(fp, "%f", &fuptime);
	uptime = (unsigned long)fuptime;

	return;
}

void update_task_status() {
	struct dirent *dentry;
	struct stat statbuf;
	char filename[MAXNAMLEN + 1];
	DIR *dirp;
	int i;

	if ((dirp = opendir("/proc")) == NULL) {
		fprintf(stderr, "opendir error for /proc\n");
		endwin();
		exit(1);
	}

	//printf("1111\n");

	while ((dentry = readdir(dirp)) != NULL) {
		//printf("2222\n");
		pid_t pid;
		if (dentry->d_ino == 0)
			continue;

		memcpy(filename, dentry->d_name, MAXNAMLEN);
		if ((pid = atoi(filename))) {
			Task_info *new_info;

			new_info = make_new_task_info(pid);
			append_to_task_list(new_info);
		} else {
			continue;
		}
	}

	sort_list_by_pid();
	//printf("8888\n");
	return;
}

unsigned long get_current_time() {
        struct timespec ts;

        if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
                fprintf(stderr, "clock_gettime error\n");
                endwin();
                exit(1);
        }

        return (NANOS * ts.tv_sec + ts.tv_nsec) / (NANOS / MILLIS);
}

void print_u_format(Task_info *t) {
	char ts[1024];
	sprintf(ts, "\n%-8s%5d%5.1f%5.1f %7lu %6lu %-7s %-4s%6s%8s %s", t->user, t->pid, t->cpu, t->mem, t->vsz, t->rss, t->tty, t->stat, t->start, t->time, t->command);
	printf("%s", ts);

	return;
}

void print_a_x_format(Task_info *t) {
	char ts[1024];
	sprintf(ts, "\n%5d %-7s %-4s%8s %s", t->pid, t->tty, t->stat, t->time, t->command);
	printf("%s", ts);

	return;
}

void print_default_format(Task_info *t) {
	char ts[1024];
	sprintf(ts, "\n%5d %-7s %8s %s", t->pid, t->tty, t->time, t->command);
	printf("%s", ts);

	return;
}

void get_devices() {
	struct dirent *dentry;
	struct stat statbuf;
	char filename[MAXNAMLEN + 128];
	DIR *dirp;
	int i;

	if ((dirp = opendir("/dev")) == NULL) {
		fprintf(stderr, "opendir error for /dev\n");
		endwin();
		exit(1);
	}

	printf("1111\n");

	while ((dentry = readdir(dirp)) != NULL) {
		if (dentry->d_ino == 0)
			continue;

		if (!strcmp(dentry->d_name, ".") || !strcmp(dentry->d_name, ".."))
			continue;


		sprintf(filename, "/dev/%s", dentry->d_name);
		printf("%s\n", filename);
		if (stat(filename, &statbuf) == -1) {
			fprintf(stderr, "stat error for %s\n", filename);
			break;
		}

		if (S_ISDIR(statbuf.st_mode) || S_ISLNK(statbuf.st_mode))
			continue;

		if (S_ISCHR(statbuf.st_mode) || S_ISBLK(statbuf.st_mode)) {
			Device_info *new_info = (Device_info *)malloc(sizeof(Device_info));
			strcpy(new_info->name, filename + 5);
			new_info->major_nr = major(statbuf.st_rdev);
			new_info->minor_nr = minor(statbuf.st_rdev);

			append_to_device_list(new_info);
		}
	}

	closedir(dirp);

	// opendir /dev/pts
	if ((dirp = opendir("/dev/pts")) == NULL) {
		fprintf(stderr, "opendir error for /dev/pts\n");
		endwin();
		exit(1);
	}

	//printf("1111\n");
	while ((dentry = readdir(dirp)) != NULL) {
		//printf("2222\n");
		pid_t pid;
		if (dentry->d_ino == 0)
			continue;

		if (!strcmp(dentry->d_name, ".") || !strcmp(dentry->d_name, ".."))
			continue;


		sprintf(filename, "/dev/pts/%s", dentry->d_name);
		if (stat(filename, &statbuf) == -1) {
			fprintf(stderr, "stat error for %s\n", filename);
			break;
		}

		if (S_ISDIR(statbuf.st_mode) || S_ISLNK(statbuf.st_mode))
			continue;

		if (S_ISCHR(statbuf.st_mode) || S_ISBLK(statbuf.st_mode)) {
			Device_info *new_info = (Device_info *)malloc(sizeof(Device_info));
			strcpy(new_info->name, filename + 5);
			new_info->major_nr = major(statbuf.st_rdev);
			new_info->minor_nr = minor(statbuf.st_rdev);

			append_to_device_list(new_info);
		}
	}

	closedir(dirp);

	return;
}

void free_device_list() {
	if (Device_list.list != NULL) {
		int i;
		for (i = 0; i < Device_list.len; ++i) {
			if (Device_list.list[i] != NULL) {
				free(Device_list.list[i]);
			}
		}
		free(Device_list.list);

		Device_list.list = NULL;
		Device_list.len = 0;
		Device_list.size = 0;
	}

	return;
}

void init_device_list() {
	free_device_list();
	Device_list.list = (Device_info **)malloc(INIT_LIST_SIZE * sizeof(Device_info));
	if (Device_list.list == NULL) {
		fprintf(stderr, "malloc error in init_device_list\n");
		endwin();
		exit(1);
	}
	Device_list.len = 0;
	Device_list.size = INIT_LIST_SIZE;

	return;
}

void append_to_device_list(Device_info* new_info) {
	if (Device_list.len == Device_list.size) {
		Device_list.size *= 2;
		Device_list.list = (Device_info **)realloc(Device_list.list, Device_list.size * sizeof(Device_info *));
		if (Device_list.list == NULL) {
			fprintf(stderr, "realloc error in append_to_device_list\n");
			endwin();
			exit(1);
		}
	}
	Device_list.list[(Device_list.len)++] = new_info;

	return;
}

void print_device_list() {
	int i;

	for (i = 0; i < Device_list.len; ++i) {
		printf("%s, ma: %d, mi: %d\n", Device_list.list[i]->name, Device_list.list[i]->major_nr, Device_list.list[i]->minor_nr);
	}

	return;
}

void get_cur_usr_name() {
	struct passwd *pwd;

	if ((pwd = getpwuid(getuid())) == NULL) {
		fprintf(stderr, "getpwuid error\n");
		endwin();
		exit(1);
	}
	strcpy(cur_usr_name, pwd->pw_name);

	return;
}
