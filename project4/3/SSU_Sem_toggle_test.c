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

SSU_Sem *sems; // 쓰레드당 하나씩 사용할 세마포어들을 저장할 배열의 인덱스

void *justprint(void *data)
{
	int thread_id = *((int *)data);
	int next_thread = (thread_id + 1) % NUM_THREADS; // 다음에 실행할 쓰레드의 thread_id를 구함

	for(int i = 0; i < NUM_ITER; i++)
	{
		SSU_Sem_down(sems + thread_id); // 세마포어 1 감소시켜 다른 쓰레드가 깨울 때 까지 wait
		printf("This is thread %d\n", thread_id); // 현재 쓰레드 id 출력
		SSU_Sem_up(sems + next_thread); // 다음 쓰레드의 세마포어 1 증가시켜 다음 쓰레드를 수행하도록 함
	}
	return 0;
}

int main(int argc, char *argv[])
{
	pthread_t mythreads[NUM_THREADS];
	int mythread_id[NUM_THREADS];

	sems = (SSU_Sem *) malloc (sizeof(SSU_Sem) * NUM_THREADS); // 쓰레드 개수만큼의 세마포어를 동적 할당한다
	for (int i = 0; i < NUM_THREADS; ++i) { // 쓰레드 총 개수만큼 반복하며 세마포어를 초기화
		SSU_Sem_init(sems + i, 0); // 세마포어 count 0으로 초기화
	}

	for(int i =0; i < NUM_THREADS; i++)
	{
		mythread_id[i] = i;
		pthread_create(&mythreads[i], NULL, justprint, (void *)&mythread_id[i]);
	}
	SSU_Sem_up(sems); // 첫번째 세마포어의 count를 1 증가시켜 첫번째 쓰레드를 동작시킨다

	for(int i =0; i < NUM_THREADS; i++)
	{
		pthread_join(mythreads[i], NULL);
	}

	return 0;
}
