#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include "SSU_Sem.h"

void SSU_Sem_init(SSU_Sem *s, int value) {
	pthread_mutex_init(&(s->lock), NULL); // mutex 초기화
	pthread_cond_init(&(s->cond), NULL); // 조건변수 초기화
	pthread_mutex_lock(&(s->lock)); // lock
 	s->count = value; // 세마포어의 count 값 초기화
	pthread_mutex_unlock(&(s->lock)); // unlock
}

void SSU_Sem_down(SSU_Sem *s) {
	pthread_mutex_lock(&(s->lock)); // lock
	while (s->count < 0) { // count가 음수라면
		pthread_cond_wait(&(s->cond), &(s->lock)); // wait
	}
 	--(s->count); // count 1 감소시킴
	if (s->count < 0) { // 감소시킨 후의 count가 음수라면
		pthread_cond_wait(&(s->cond), &(s->lock)); // wait
	}
	pthread_mutex_unlock(&(s->lock)); // unlock
}

void SSU_Sem_up(SSU_Sem *s) {
	pthread_mutex_lock(&(s->lock)); // lock
 	++s->count; // count 1 증가시킴
	pthread_mutex_unlock(&(s->lock)); // unlock
	pthread_cond_signal(&(s->cond)); // send signal
}
