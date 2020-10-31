#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>
#include <signal.h>
#include <wait.h>
#include <pthread.h>
#include <unistd.h>
#include "SSU_Sem.h"

#define NUM_THREADS 3
#define NUM_ITER 10

SSU_Sem *sems;

void *justprint(void *data)
{
	int thread_id = *((int *)data);
	int next_thread = (thread_id + 1) % NUM_THREADS;

	for(int i = 0; i < NUM_ITER; i++)
	{
		SSU_Sem_down(sems + thread_id);
		printf("This is thread %d\n", thread_id);
		SSU_Sem_up(sems + next_thread);
	}
	return 0;
}

int main(int argc, char *argv[])
{
	pthread_t mythreads[NUM_THREADS];
	int mythread_id[NUM_THREADS];

	sems = (SSU_Sem *) malloc (sizeof(SSU_Sem) * NUM_THREADS);
	for (int i = 0; i < NUM_THREADS; ++i) {
		SSU_Sem_init(sems + i, 0);
	}

	for(int i =0; i < NUM_THREADS; i++)
	{
		mythread_id[i] = i;
		pthread_create(&mythreads[i], NULL, justprint, (void *)&mythread_id[i]);
	}
	SSU_Sem_up(sems);

	for(int i =0; i < NUM_THREADS; i++)
	{
		pthread_join(mythreads[i], NULL);
	}

	return 0;
}
