#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>

#define BUFFER_SIZE 128

void print_system_infos();
void print_current_time();
void print_running_time();
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

	printf("user, ");
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
void print_process_infos() {

}

void print_proecss_info(pid_t pid) {

}
