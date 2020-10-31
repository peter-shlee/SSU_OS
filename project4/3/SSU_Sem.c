#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include "SSU_Sem.h"

void SSU_Sem_init(SSU_Sem *s, int value) {
	pthread_mutex_init(&(s->lock), NULL);
	pthread_cond_init(&(s->cond), NULL);
	pthread_mutex_lock(&(s->lock));
 	s->count = value; 
	pthread_mutex_unlock(&(s->lock));
}

void SSU_Sem_down(SSU_Sem *s) {
	pthread_mutex_lock(&(s->lock));
	while (s->count < 0) {
		pthread_cond_wait(&(s->cond), &(s->lock));
	}
 	--(s->count); 
	if (s->count < 0) {
		pthread_cond_wait(&(s->cond), &(s->lock));
	}
	pthread_mutex_unlock(&(s->lock));
}

void SSU_Sem_up(SSU_Sem *s) {
	pthread_mutex_lock(&(s->lock));
 	++s->count; 
	pthread_mutex_unlock(&(s->lock));
	pthread_cond_signal(&(s->cond));
}
