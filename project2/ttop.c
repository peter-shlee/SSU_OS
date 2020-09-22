#include <stdio.h>
#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <utmpx.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <pwd.h>

#define BUFFER_SIZE (128)
#define MAX_USER_NAME (32)
#define INIT_LIST_SIZE (1024)
#define MILLIS (1000)
const unsigned long NANOS = 1000000000;
int page_size_in_KiB;
unsigned int print_start_index;
int running; // running 상태인 프로세스의 개수를 저장할 변수
int sleeping; // sleeping 상태인 프로세스의 개수를 저장할 변수
int stopped; // stopped 상태인 프로세스의 개수를 저장할 변수
int zombie; // zombie 상태인 프로세스의 개수를 저장할 변수

void init_screen(); // 콘솔 초기화 함수
void refresh_page(int row, int col); // ttop 내용 새로고침

void print_system_infos(int col); // ttop 명령어 상단의 system 정보 출력
char *print_current_time(); // 현재시간 문자열 만들어 리턴하는 함수
char *print_running_time(); // 실행 시간 문자열 만들어 리턴하는 함수
char *print_user_count(); // 유저 수 문자열로 만들어 리턴하는 함수
char *print_load_average(); // load average 문자열로 만들어 리턴하는 함수
void print_cpu_infos(int col); // cpu정보 문자열로 만들어 리턴하는 함수
void print_mem_infos(int col); // mem정보 문자열로 만들어 리턴하는 함수
void print_task_infos(int col); // tasks 정보들 문자열로 만들어 리턴하는 함수
void check_task_status(); // task들의 상태 체그해 각 상태별 task개수 저장하는 함수

void free_simple_task_list();
void free_task_list();
void init_task_list(); // Task_list 초기화하는 함수
void init_simple_task_list(); // Simple_task_list 초기화하는 함수

void increase_print_start_index(); // 출력 시작할 task의 인덱스를 증가시키는 함수
void decrease_print_start_index(); // 출력 시작할 task의 인덱스를 감소시키는 함수
unsigned long get_current_time(); // 밀리초 단위의 현재시간 리턴하는 함수
void update_time(); // cur_time, prev_time 갱신하는 함수
void update_cpu_time();// cpu time 갱신하는 함수
void update_simple_task_status(); // simple_task_list 갱신하는 함수
void update_task_status(int max_count); // task_list 갱신하는 함수
void print_process_infos(int row, int col); // 프로세스들의 정보를 출력하는 함수
void print_process_info(int index, int col); // 특정 프로세스의 정보 출력하는 함수
char *convert_time_format(unsigned long time); // clock tick 단위의 시간을 문자열 형태로 변환하여 리턴

unsigned long prev_cpu_idle; // 이전에 측정한 cpu idle time
unsigned long prev_cpu_nonidle; // 이전에 측정한 cpu nonidle time
unsigned long prev_cpu_time; // 이전에 측정한 cpu time
unsigned long cur_cpu_idle; // 새로 측정한 cpu idle time
unsigned long cur_cpu_nonidle; // 새로 측정한 cpu nonidle time
unsigned long cur_cpu_time; // 새로 측정한 cpu time
unsigned long prev_time; // 이전에 측정한 시간
unsigned long cur_time; // 새로 측정한 시간

typedef struct _task_info { // 각 프로세스의 정보 담을 구조체
	pid_t pid;
	char user_name[MAX_USER_NAME + 1];
	long pr; // priority
	long ni; // nice
	unsigned long virt;
	unsigned long res;
	unsigned long shr;
	char s[2]; // status
	float cpu;
	float mem;
	unsigned long time; // process running time
	char command[BUFFER_SIZE];
} Task_info;

struct _task_list { // Task_info 배열 관리 구조체
	Task_info **list;
	int len; // 원소 개수
	int size; // 배열 길이 
	int is_sorted_by_pid; // pid 기준으로 정렬되어있다면 1
	int is_sorted_by_cpu_and_pid; // cpu, pid 기준으로 정렬되어있다면 1
} Task_list;

typedef struct _simple_task_info { // 간략한 프로세스의 정보를 담는 구조체
	pid_t pid; 
	float cpu;
	unsigned long prev_cpu_time;
	unsigned long cur_cpu_time;
	int is_updated; // 갱신되었다면 1, 0이라면 사라진 프로세스로 간주하고 삭제해야함
	char s[2];
} Simple_task_info;

struct _simple_task_list {
	Simple_task_info **list;
	int len;
	int size;
	int is_sorted_by_pid;
	int is_sorted_by_cpu_and_pid;
} Simple_task_list;

struct cpu_info{ // /proc/stat 첫째줄의 cpu 정보들 담는 구조체
	unsigned long us;
	unsigned long sy;
	unsigned long ni;
	unsigned long id;
	unsigned long wa;
	unsigned long hi;
	unsigned long si;
	unsigned long st;
	unsigned long total;
} cur_cpu_info, prev_cpu_info;

int main(void) {
	int row, col; // 터미널 크기 저장하는데 사용
	int exit_flag = 0;

	init_screen(); // ncurses 이용하여 화면 초기화

	page_size_in_KiB = getpagesize() / 1024; // KiB 단위의 페이지 크기 계산
	update_time(); // 현재시간 갱신
	update_cpu_time(); // 현재 cpu시간 갱신
	init_task_list(); // Task_list 초기화
	init_simple_task_list(); // Simple_task_list 초기화
	update_simple_task_status(); // Simple_task_list 갱신

	while(1) {
		getmaxyx(stdscr, row, col); // 창 크기 확인
		refresh_page(row, col); // 프로세스 정보들 갱신하여 화면에 출력
		while (get_current_time() - cur_time < 3 * MILLIS) { // 이전 화면 갱신으로부터 3초가 지나지 않았다면
			int refresh_page_flag = 0;
			int key_input;
			if ((key_input = getch()) != ERR) { // 키가 입력됐다면
				switch (key_input) {
					case 'q': // q 입력됐다면
						exit_flag = 1;
						break;
					case KEY_UP: // 위쪽 방향키 입력됐다면
						decrease_print_start_index();
						refresh_page_flag = 1;
						break;
					case KEY_DOWN: // 아래 방향키 입력됐다면
						increase_print_start_index();
						refresh_page_flag = 1;
						break;
					default:
						break;

				}
				if (refresh_page_flag || exit_flag) break;
			}
		}
		if (exit_flag) break;
	}

	free_task_list();
	free_simple_task_list();
	endwin();

	return 0;
}

void init_screen() {
	initscr();
	cbreak();
	noecho();
	keypad(stdscr, TRUE); // 방향키 입력을 위하여
	nodelay(stdscr, TRUE);
	curs_set(0); // 커서 안보이도록

	return;
}

void refresh_page(int row, int col) {
	update_time(); // 시간 갱신
	update_cpu_time(); // cpu 시간 갱신
	update_simple_task_status(); // simple_task_list 갱신
	update_task_status(row - 7); // task_list 갱신
	check_task_status(); // task들 상태 개수 갱신

	clear(); // 화면 내용 지움
	print_system_infos(col); // ttop 명령어 상단의 정보들 출력
	print_process_infos(row, col); // ttop 명령어 하단의 프로세스 정보 출력
	refresh(); // 새로운 화면 내용 표시

	return;
}

void print_system_infos(int col) {
	char info_string[1024];
	char *current_time_string, *running_time_string, *user_count_string, *load_average_string;
	// 출력할 각종 정보들 문자열 만듦
	current_time_string = print_current_time();
	running_time_string = print_running_time();
	user_count_string = print_user_count();
	load_average_string = print_load_average();
	sprintf(info_string, "top - %s up %s, %s user, load average: %s", current_time_string, running_time_string, user_count_string, load_average_string);
	//printf("%s", info_string);
	addnstr(info_string, col - 1);
	free(current_time_string );
	free(running_time_string );
	free(user_count_string );
	free(load_average_string );

	// tasks 정보 출력
	print_task_infos(col);

	// cpu 정보 출력
	print_cpu_infos(col);

	// mem 정보 출력
	print_mem_infos(col);
	printw("\n");

	return;
}

char *print_current_time() {
	time_t raw_time;
	struct tm *time_info;
	char *time_text;
	char *current_time_text;
	current_time_text = (char *)calloc(16 ,sizeof(char));

	time(&raw_time);
	time_info = localtime(&raw_time);
	time_text = asctime(time_info);
	strncpy(current_time_text, time_text + 11, sizeof(current_time_text) / sizeof(current_time_text[0]));
	//printf("%s", current_time_text);

	return current_time_text;
}

char *print_running_time() {
	// ~ min
	// ~:~~
	FILE *fp;
	const char* fname = "/proc/uptime";
	const char* mode = "r";
	float fuptime;
	int hour = 0;
	int minute = 0;
	char *running_time_text;
	running_time_text = (char *)malloc(16 * sizeof(char));

	// /proc/uptime 정보들 가져옴
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%f", &fuptime);
	fclose(fp);

	// 시간 계산
	hour = fuptime / (60 * 60);
	minute = (fuptime - (hour *60 * 60)) / 60;

	// 문자열 형식으로 변환
	if (hour == 0) {
		sprintf(running_time_text, "%d min", minute);
	} else {
		sprintf(running_time_text, "%d:%02d", hour, minute);
	}

	return running_time_text;
}

char *print_user_count() {
	struct utmpx *utmpxp;
	int logged_in_user_count = 0;
	char *user_count_string;
	user_count_string = (char *)malloc(8 * sizeof(char));

	// 로그인 된 유저 수 계산
	setutxent();
	while ((utmpxp = getutxent()) != NULL) {
		if (utmpxp->ut_type == USER_PROCESS) {
			++logged_in_user_count;
		}
	}
	endutxent();

	sprintf(user_count_string, "%d", logged_in_user_count);

	return user_count_string;
}

char *print_load_average() {
	FILE *fp;
	const char *fname = "/proc/loadavg";
	const char* mode = "r";
	float _1_min_avg, _5_min_avg, _15_min_avg;
	char *load_average_string;
	load_average_string = (char *)malloc(32 * sizeof(char));

	// /proc/loadavg에서 load average 정보 읽음
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}

	fscanf(fp, "%f%f%f", &_1_min_avg, &_5_min_avg, &_15_min_avg);
	fclose(fp);

	sprintf(load_average_string, "%.2f, %.2f, %.2f", _1_min_avg, _5_min_avg, _15_min_avg);

	return load_average_string;
}

void print_task_infos(int col) {
	char task_info_string[1024];

	sprintf(task_info_string, "\nTasks: %3d total, %3d running, %3d sleeping, %3d stopped, %3d zombie", Simple_task_list.len, running, sleeping, stopped, zombie);
	//printf("%s", task_info_string);
	addnstr(task_info_string, col);

	return;
}

void print_cpu_infos(int col) {
	FILE *fp;
	const char *fname = "/proc/stat";
	const char *mode = "r";
	char tmp[BUFFER_SIZE];
	unsigned long us, sy, ni, id, wa, hi, si, st, total;
	float fus, fsy, fni, fid, fwa, fhi, fsi, fst;
	char cpu_info_string[1024];
	float interval = sysconf(_SC_CLK_TCK) * (cur_time - prev_time);

	// /proc/stat에서 cpu 정보들 읽음
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%s%lu%lu%lu%lu%lu%lu%lu%lu", tmp, &us, &ni, &sy, &id, &wa, &hi, &si, &st);
	fclose(fp);
	total = us + sy + ni + id + wa + hi + si + st;

	prev_cpu_info = cur_cpu_info;
	cur_cpu_info.us = us;
	cur_cpu_info.sy = sy;
	cur_cpu_info.ni = ni;
	cur_cpu_info.id = id;
	cur_cpu_info.wa = wa;
	cur_cpu_info.hi = hi;
	cur_cpu_info.si = si;
	cur_cpu_info.st = st;
	cur_cpu_info.total = total;

	// 각각의 사용량 계산
	fus = ((us - prev_cpu_info.us) / (float)(total - prev_cpu_info.total)) * 100.0;
	fsy = ((sy - prev_cpu_info.sy) / (float)(total - prev_cpu_info.total)) * 100.0;
	fni = ((ni - prev_cpu_info.ni) / (float)(total - prev_cpu_info.total)) * 100.0;
	fid = ((id - prev_cpu_info.id) / (float)(total - prev_cpu_info.total)) * 100.0;
	fwa = ((wa - prev_cpu_info.wa) / (float)(total - prev_cpu_info.total)) * 100.0;
	fhi = ((hi - prev_cpu_info.hi) / (float)(total - prev_cpu_info.total)) * 100.0;
	fsi = ((si - prev_cpu_info.si) / (float)(total - prev_cpu_info.total)) * 100.0;
	fst = ((st - prev_cpu_info.st) / (float)(total - prev_cpu_info.total)) * 100.0;
	sprintf(cpu_info_string, "\n%%Cpu(s): %4.1f us, %4.1f sy,  %4.1f ni, %4.1f id, %4.1f wa, %4.1f hi, %4.1f si, %4.1f st", fus, fsy, fni, fid, fwa, fhi, fsi, fst);
	//printf("%s", cpu_info_string);
	addnstr(cpu_info_string, col);

	return;
}

void check_task_status() {
	int i;
	// 프로세스들의 상태 개수 갱신
	running = 0;
	sleeping = 0;
	stopped = 0;
	zombie = 0;

	for (i = 0; i < Simple_task_list.len; ++i) {
		if (!strcmp(Simple_task_list.list[i]->s, "R")) {
			running += 1;
		} else if (!strcmp(Simple_task_list.list[i]->s, "S")) {
			sleeping += 1;
		} else if (!strcmp(Simple_task_list.list[i]->s, "T") || !strcmp(Simple_task_list.list[i]->s, "t")) {
			stopped += 1;
		} else if (!strcmp(Simple_task_list.list[i]->s, "Z")) {
			zombie += 1;
		}
	}

	return;
}

void print_mem_infos(int col) {
	FILE *fp;
	const char *fname = "/proc/meminfo";
	const char *mode = "r";
	int mem_total, mem_free, mem_used, mem_cached, mem_buffers, mem_SReclaimable, mem_available, mem_cache;
	int swap_total, swap_free, swap_used;
	char tmp[BUFFER_SIZE];
	char info_string[1024];
	int i;
	
	// used -> total - free - buffers - cache
	// buffers -> Buffers in /proc/meminfo
	// cache -> Cached and SReclaimable in /proc/meminfo

	// /proc/meminfo에서 메모리 관련 정보들 읽음
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
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

	// 각 항목 계산하여 출력
	mem_cache = mem_cached + mem_SReclaimable;
	mem_used = mem_total - mem_free - mem_buffers - mem_cache;
	swap_used = swap_total - swap_free;
	sprintf(info_string, "\nKiB Mem : %8d total, %8d free, %8d used, %8d buff/cache", mem_total, mem_free, mem_used, mem_buffers + mem_cache);
	addnstr(info_string, col);
	sprintf(info_string, "\nKiB Swap: %8d total, %8d free, %8d used, %8d avail Mem", swap_total, swap_free, swap_used, mem_available);
	addnstr(info_string, col);

}
///////////////////////////////////////////////////////
// task info
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

void init_task_list() { // Task_list 초기화
	free_task_list();
	Task_list.list = (Task_info **)malloc(INIT_LIST_SIZE * sizeof(Task_info));
	if (Task_list.list == NULL) {
		fprintf(stderr, "malloc error in init_task_list\n");
		endwin();
		exit(1);
	}
	Task_list.len = 0;
	Task_list.size = INIT_LIST_SIZE;
	Task_list.is_sorted_by_pid = 0;
	Task_list.is_sorted_by_cpu_and_pid = 0;

	return;
}

void append_to_task_list(Task_info* new_info) { // Task_list에 새로운 원소 추가
	if (Task_list.len == Task_list.size) { // 배열이 꽉 찼다면
		Task_list.size *= 2; // 크기 두배로 늘림
		Task_list.list = (Task_info **)realloc(Task_list.list, Task_list.size * sizeof(Task_info *));
		if (Task_list.list == NULL) { // 메모리 재할당 실패 시
			fprintf(stderr, "realloc error in append_to_task_list\n");
			endwin();
			exit(1);
		}
	}
	Task_list.list[(Task_list.len)++] = new_info;
	Task_list.is_sorted_by_pid = 0;
	Task_list.is_sorted_by_cpu_and_pid = 0;

	return;
}

Task_info *make_new_task_info(pid_t pid) { // pid에 해당하는 프로세스의 Task_info 생성
	FILE *fp;
	char fname[MAXNAMLEN + 1];
	char tmp[BUFFER_SIZE];
	const char *mode = "r";
	unsigned long stime, utime;
	float mem_total;
	int i;
	struct passwd *result;
	uid_t uid;

	//printf("1111\n");
	Task_info *new_info = (Task_info *)malloc(sizeof(Task_info));
	if (new_info == NULL) {
		fprintf(stderr, "malloc error in make_new_task_info\n");
		endwin();
		exit(1);
	}
	new_info->pid = pid;

	//printf("2222\n");
	/////
	// /proc/[pid]/stat에서 필요한 정보들 읽어옴
	sprintf(fname, "/proc/%d/stat", pid);
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}

	for(i = 0; i < 2; ++i)
		fscanf(fp, "%s", tmp);
	fscanf(fp, "%s", new_info->s);
	for(i = 0; i < 10; ++i)
		fscanf(fp, "%s", tmp);
	fscanf(fp, "%lu%lu", &stime, &utime); // stime, utime 
	new_info->time = stime + utime;
	for(i = 0; i < 2; ++i)
		fscanf(fp, "%s", tmp);
	fscanf(fp, "%ld%ld", &(new_info->pr), &(new_info->ni)); // pr, ni
	fclose(fp);
	
	//printf("3333\n");
	/////
	// /proc/[pid]/loginuid에서 필요한 정보들 읽어옴
	sprintf(fname, "/proc/%d/loginuid", pid);
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%d", &uid); // uid
	fclose(fp);

	// uid 이용하여 user name 얻어옴
	if ((result = getpwuid(uid)) == NULL) {
		strcpy(new_info->user_name, "root");
	} else {
		strcpy(new_info->user_name, result->pw_name);
	}

	//printf("4444\n");
	/////
	// /proc/[pid]/status에서 필요한 정보 읽어옴
	sprintf(fname, "/proc/%d/status", pid);
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%s%s", tmp, new_info->command); // command

	fclose(fp);

	//printf("5555\n");
	/////
	// /proc/[pid]/statm에서 필요한 정보 읽어옴
	sprintf(fname, "/proc/%d/statm", pid);
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%lu%lu%lu", &(new_info->virt), &(new_info->res), &(new_info->shr));
	fclose(fp);
	new_info->virt *= page_size_in_KiB; // virt
	new_info->res *= page_size_in_KiB; // res
	new_info->shr *= page_size_in_KiB; // shr

	//printf("6666\n");
	/////
	// /proc/meminfo에서 필요한 정보 읽어옴
	sprintf(fname, "/proc/meminfo");
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}
	fscanf(fp, "%s%f", tmp, &mem_total);
	fclose(fp);

	new_info->mem = (new_info->res) / mem_total * 100.0;

	return new_info;
}

int compare_by_pid(const void *a, const void *b) { // qsort에 사용
	return (*(Task_info **)a)->pid - (*(Task_info **)b)->pid;
}

int compare_by_cpu_and_pid(const void *_a, const void *_b) { // qsort에 사용
	int a_cpu, b_cpu;
	Task_info **a = (Task_info **)_a;
	Task_info **b = (Task_info **)_b;
	a_cpu = (int)((*a)->cpu * 100);
	b_cpu = (int)((*b)->cpu * 100);

	if (a_cpu == b_cpu) {
		return (*a)->pid - (*b)->pid;
	} else {
		return a_cpu - b_cpu;
	}
}

void sort_list_by_pid() { // pid 기준으로 정렬
	Task_list.is_sorted_by_pid = 1;
	Task_list.is_sorted_by_cpu_and_pid = 0;

	qsort(Task_list.list, Task_list.len, sizeof(Task_info *), compare_by_pid);
}

void sort_list_by_cpu_and_pid() { // cpu사용량, pid 기준으로 정렬
	Task_list.is_sorted_by_pid = 0;
	Task_list.is_sorted_by_cpu_and_pid = 1;

	qsort(Task_list.list, Task_list.len, sizeof(Task_info *), compare_by_cpu_and_pid);
}

Task_info **find_element(pid_t pid) { // Task_list에서 pid로 검색
	Task_info tmp_info;
	Task_info *key;
	if (!Task_list.is_sorted_by_pid) {
		sort_list_by_pid();
	}

	tmp_info.pid = pid;
	key = &tmp_info;
	return bsearch(&key, Task_list.list, Task_list.len, sizeof(Task_info *), compare_by_pid);
}

//void print_list() { // Task_list 내용 출력 - 디버깅용
//	int i;
//
//	printf("*** Task_info ***\n");
//	for (i = 0; i < Task_list.len; ++i) {
//		printf("pid: %d, cpu: %.1f\n", Task_list.list[i]->pid, Task_list.list[i]->cpu);
//		printf("username: %s, pri: %ld, ni: %ld, virt: %lu, res: %lu, shr: %lu, s: %s, mem: %.1f, time: %lu, command: %s\n", Task_list.list[i]->user_name, Task_list.list[i]->pr, Task_list.list[i]->ni, Task_list.list[i]->virt, Task_list.list[i]->res, Task_list.list[i]->shr, Task_list.list[i]->s, Task_list.list[i]->mem, Task_list.list[i]->time, Task_list.list[i]->command);
//	}
//	printf("\n");
//
//	return;
//}

// simple task info
void free_simple_task_list() {
	if (Simple_task_list.list != NULL) {
		int i;
		for (i = 0; i < Simple_task_list.len; ++i) {
			if (Simple_task_list.list[i] != NULL) {
				free(Simple_task_list.list[i]);
			}
		}
		free(Simple_task_list.list);

		Simple_task_list.list = NULL;
		Simple_task_list.len = 0;
		Simple_task_list.size = 0;
	}

	return;
}

void init_simple_task_list() { // Simple_task_list 초기화
	free_simple_task_list();
	Simple_task_list.list = (Simple_task_info **)malloc(INIT_LIST_SIZE * sizeof(Simple_task_info *));
	if (Simple_task_list.list == NULL) {
		fprintf(stderr, "malloc error in init_simple_task_list\n");
		endwin();
		exit(1);
	}
	Simple_task_list.len = 0;
	Simple_task_list.size = INIT_LIST_SIZE;
	Simple_task_list.is_sorted_by_pid = 0;
	Simple_task_list.is_sorted_by_cpu_and_pid = 0;

	return;
}

void append_to_simple_task_list(Simple_task_info *new_info) {
	if (Simple_task_list.len == Simple_task_list.size) { // 배열 꽉찼으면
		Simple_task_list.size *= 2; // 크기 두배로 늘림
		Simple_task_list.list = (Simple_task_info **)realloc(Simple_task_list.list, Simple_task_list.size * sizeof(Simple_task_info *));
		if (Simple_task_list.list == NULL) { // 메모리 할당 실패 시
			fprintf(stderr, "realloc error in append_to_simple_task_list\n");
			endwin();
			exit(1);
		}
	}
	Simple_task_list.list[(Simple_task_list.len)++] = new_info;
	Simple_task_list.is_sorted_by_pid = 0;
	Simple_task_list.is_sorted_by_cpu_and_pid = 0;

	return;
}

Simple_task_info *make_new_simple_task_info(pid_t pid) {
	Simple_task_info *new_info = (Simple_task_info *)malloc(sizeof(Simple_task_info));
	if (new_info == NULL) {
		fprintf(stderr, "malloc error in make_new_simple_task_info\n");
		endwin();
		exit(1);
	}
	new_info->pid = pid;
	new_info->is_updated = 1;

	return new_info;
}

int compare_simple_by_pid(const void *_a, const void *_b) { // qsort에 사용
	//printf("compare start\n");
	Simple_task_info **a = (Simple_task_info **)_a;
	Simple_task_info **b = (Simple_task_info **)_b;
	//printf("%d - %d\n", (*a)->pid, (*b)->pid);
	//printf("compare end\n");
	return (*a)->pid - (*b)->pid;
}

int compare_simple_by_cpu_and_pid(const void *_a, const void *_b) { // qsort에 사용
	int a_cpu, b_cpu;
	Simple_task_info **a = (Simple_task_info **)_a;
	Simple_task_info **b = (Simple_task_info **)_b;
	a_cpu = (int)((*a)->cpu * 100);
	b_cpu = (int)((*b)->cpu * 100);

	if (a_cpu == b_cpu) {
		return (*a)->pid - (*b)->pid;
	} else {
		return b_cpu - a_cpu;
	}
}

void sort_simple_list_by_pid() { // pid 기준으로 정렬
	Simple_task_list.is_sorted_by_pid = 1;
	Simple_task_list.is_sorted_by_cpu_and_pid = 0;

	qsort(Simple_task_list.list, Simple_task_list.len, sizeof(Simple_task_info *), compare_simple_by_pid);
}

void sort_simple_list_by_cpu_and_pid() { // cpu 사용률과 pid 기준으로 정렬
	Simple_task_list.is_sorted_by_pid = 0;
	Simple_task_list.is_sorted_by_cpu_and_pid = 1;

	qsort(Simple_task_list.list, Simple_task_list.len, sizeof(Simple_task_info *), compare_simple_by_cpu_and_pid);
}

Simple_task_info **find_simple_element(pid_t pid) { // Simple_Task_list에서 pid로 검색
	Simple_task_info tmp_info;
	Simple_task_info *key;
	if (!Simple_task_list.is_sorted_by_pid) {
		sort_simple_list_by_pid();
	}

	tmp_info.pid = pid;
	key = &tmp_info;
	return (Simple_task_info **)bsearch(&key, Simple_task_list.list, Simple_task_list.len, sizeof(Simple_task_info *), compare_simple_by_pid);
}

//void print_simple_list() {
//	int i;
//
//	printf("*** Simple_task_info ***\n");
//	for (i = 0; i < Simple_task_list.len; ++i) {
//		//if (Simple_task_list.list[i]->cpu > 0)
//		printf("pid: %d, cpu: %.1f, cputime: %lu\n", Simple_task_list.list[i]->pid, Simple_task_list.list[i]->cpu, Simple_task_list.list[i]->cur_cpu_time - Simple_task_list.list[i]->prev_cpu_time);
//	}
//	printf("\n");
//
//	return;
//}

//////////////////////////////////////////////////////////////

unsigned int print_start_index; // 출력 시작할 Task_list의 인덱스
void increase_print_start_index() { // print_start_index 증가시킴
	if (print_start_index >= Simple_task_list.len - 1) { // 맨 마지막 원소이면
		print_start_index = Simple_task_list.len - 1; // 증가시키지 않음
	} else {
		++print_start_index;
	}

	return;
}

void decrease_print_start_index() { // print_start_index 감소시킴
	if (print_start_index <= 0) { // 맨 앞 원소이면
		print_start_index = 0; // 감소시키지 않음
	} else {
		--print_start_index;
	}

	return;
}

void update_time() {
	prev_time = cur_time;
	cur_time = get_current_time();
	//printf("prev_time = %lu, current_time = %lu\n", prev_time, cur_time);

	return;
}

unsigned long get_current_time() { // 현재시간 밀리초단위로 리턴
	struct timespec ts;

	if (clock_gettime(CLOCK_REALTIME, &ts) == -1) {
		fprintf(stderr, "clock_gettime error\n");
		endwin();
		exit(1);
	}

	return (NANOS * ts.tv_sec + ts.tv_nsec) / (NANOS / MILLIS);
}

void update_cpu_time() {
	FILE *fp;
	const char *fname = "/proc/stat";
	const char *mode = "r";
	char tmp[BUFFER_SIZE];
	unsigned long user, nice, system, idle, lowait, irq, softirq, steal, guest, guest_nice;

	// /proc/stat에서 원하는 정보 읽어온다
	if ((fp = fopen(fname, mode)) == NULL) {
		fprintf(stderr, "fopen error for %s\n", fname);
		endwin();
		exit(1);
	}

	fscanf(fp, "%s%lu%lu%lu%lu%lu%lu%lu%lu%lu%lu", tmp, &user, &nice, &system, &idle, &lowait, &irq, &softirq, &steal, &guest, &guest_nice);
	fclose(fp);

	// 이전 cpu time 갱신
	prev_cpu_nonidle = cur_cpu_nonidle;
	prev_cpu_idle = cur_cpu_idle;
	prev_cpu_time = cur_cpu_time;
	//cur_cpu_time = user + nice + system + idle + lowait + irq + softirq + steal + guest + guest_nice;

	// 새로운 cpu time 갱신
	cur_cpu_nonidle = user + nice + system + irq + softirq + steal;
	cur_cpu_idle = idle + lowait;
	cur_cpu_time = cur_cpu_nonidle + cur_cpu_idle;

	return;
}

void update_task_status(int max_count) {
	int i;
	int target_index;

	init_task_list(); // Task_list 초기화하여 기존에 있던 정보들 다 지운다
	for(i = 0, target_index = print_start_index; i < max_count && print_start_index + i < Simple_task_list.len; ++i) { // 화면에 출력할 만큼만 Task_info를 생성한다
		pid_t target_pid;

		target_index = print_start_index + i;
		target_pid = Simple_task_list.list[target_index]->pid; // Simple_task_list에서 Task_info 생성할 프로세스의 pid 가져온다

		Task_info *new_info = make_new_task_info(target_pid); // 새로운 Task_info 생성
		new_info->cpu = Simple_task_list.list[target_index]->cpu;
		append_to_task_list(new_info); // 배열에 추가
	}
	//print_list();
}

void update_simple_task_status() {
	struct dirent *dentry;
	struct stat statbuf;
	char filename[MAXNAMLEN + 1];
	DIR *dirp;
	int i;

	for (i = 0; i < Simple_task_list.len; ++i) {
		Simple_task_list.list[i]->prev_cpu_time = Simple_task_list.list[i]->cur_cpu_time;
		Simple_task_list.list[i]->cur_cpu_time = 0;
		Simple_task_list.list[i]->is_updated = 0;
	}

	if ((dirp = opendir("/proc")) == NULL) {
		fprintf(stderr, "opendir error for /proc\n");
		endwin();
		exit(1);
	}

	//printf("1111\n");

	while ((dentry = readdir(dirp)) != NULL) { // /proc 디렉토리 내의 모든 파일을 확인한다
		//printf("2222\n");
		pid_t pid;
		if (dentry->d_ino == 0)
			continue;

		memcpy(filename, dentry->d_name, MAXNAMLEN);
		if ((pid = atoi(filename))) { // 파일명이 숫자일때
			FILE *fp;
			unsigned long stime, utime;
			char proc_stat_filename[MAXNAMLEN + 1];
			char tmp[BUFFER_SIZE];
			int is_new_info = 0;
			int i;
			//printf("2.5222\n");
			Simple_task_info *task_info ;
			Simple_task_info **task_info_p = find_simple_element(pid); // 해당 pid의 정보가 이미 존재하는지 확인한다
			//printf("3333\n");
			if (task_info_p == NULL) { // 해당 pid의 정보가 없다면
				is_new_info = 1;
				task_info = make_new_simple_task_info(pid); // 새로 생성
			} else {
				task_info = *task_info_p;
			}

			//printf("4444\n");
			// /proc/[pid]/stat에서 필요한 정보를 읽어온다
			sprintf(proc_stat_filename, "/proc/%d/stat", pid);
			if ((fp = fopen(proc_stat_filename, "r")) == NULL) {
				fprintf(stderr, "fopen error for %s\n", proc_stat_filename);
				endwin();
				exit(1);
			}

			for (i = 0; i < 2; ++i)
				fscanf(fp, "%s", tmp);
			fscanf(fp, "%s", task_info->s); // status
			for (i = 0; i < 10; ++i)
				fscanf(fp, "%s", tmp);
			fscanf(fp, "%lu%lu", &utime, &stime); // utime, stime
			fclose(fp);

			//printf("5555\n");
			task_info->cur_cpu_time = utime + stime;
			task_info->cpu = ((float)(task_info->cur_cpu_time - task_info->prev_cpu_time) / (sysconf(_SC_CLK_TCK) * (cur_time - prev_time))) * 100.0 * MILLIS; // cpu 사용량 계산
			if (task_info->cpu > 100.0) {
				task_info->cpu = 100.0;
			}
			task_info->is_updated = 1;

			//printf("6666\n");
			if (is_new_info) {
				//printf("add new simple info\n");
				append_to_simple_task_list(task_info);
			}
			//printf("7777\n");
		} else {
			continue;
		}
	}
	// 종료된 프로세스 Simple_task_list에서 제거하는 작업
	for (i = 0; i < Simple_task_list.len; ++i) {
		if (Simple_task_list.list[i]->is_updated == 0) { // 종료된 프로세스
			free(Simple_task_list.list[i]);
			if (i < --(Simple_task_list.len)) {
				Simple_task_list.list[i] = Simple_task_list.list[Simple_task_list.len];
			}
			Simple_task_list.list[Simple_task_list.len] = NULL;
			Simple_task_list.is_sorted_by_pid = 0;
			Simple_task_list.is_sorted_by_cpu_and_pid = 0;
		}
	}

	sort_simple_list_by_cpu_and_pid();
	//printf("8888\n");
	return;
}

void print_process_infos(int row, int col) { // 여러 프로세스의 정보들을 출력
	int i;
	char task_info_string[1024];

	sprintf(task_info_string, "\n%6s %-8s%3s %3s %7s %7s %7s %s %5s %4s %9s %s", "PID", "USER", "PR", "NI", "VIRT", "RES", "SHR", "S", "%CPU", "%MEM", "TIME+", "COMMAND");
	//printf("%s\n", task_info_string);
	addnstr(task_info_string, col);
	for (i = 0; i < Task_list.len; ++i) {
		print_process_info(i, col);
	}

	return;
}

void print_process_info(int index, int col) { // 프로세스 하나의 정보 출력
	Task_info *t;
	char task_info_string[1024];
	char pr_string[10];
	char *time_string;

	t = Task_list.list[index];
	if (t->pr == -100) {
		sprintf(pr_string, "rt");
	} else {
		sprintf(pr_string, "%ld", t->pr);
	}
	time_string = convert_time_format(t->time);
	sprintf(task_info_string, "\n%6d %-8s%3s %3ld %7lu %7lu %7lu %s %5.1f %4.1f %9s %s", t->pid, t->user_name, pr_string, t->ni, t->virt, t->res, t->shr, t->s, t->cpu, t->mem, time_string, t->command);

	//printf("%s\n", task_info_string);
	addnstr(task_info_string, col);
	free(time_string);

	return;
}

char *convert_time_format(unsigned long time){
	char *time_string;
	unsigned long tmp_seconds = time / sysconf(_SC_CLK_TCK);
	unsigned long minutes = tmp_seconds / 60;
	float seconds = (time - minutes * 60.0 * sysconf(_SC_CLK_TCK)) / sysconf(_SC_CLK_TCK);
	time_string = (char *)malloc(16 * sizeof(char));

	sprintf(time_string, "%lu:%05.2f", minutes, seconds);
	return time_string;
}
