#include <pthread.h>

typedef struct SSU_Sem {
	pthread_mutex_t lock; // count 변수 접근 시에 사용할 mutex
	pthread_cond_t cond; // count 변수 접근 시에 사용할 조건변수
	int count;
} SSU_Sem;

void SSU_Sem_init(SSU_Sem *, int);
void SSU_Sem_up(SSU_Sem *);
void SSU_Sem_down(SSU_Sem *);
