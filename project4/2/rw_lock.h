#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

struct rw_lock
{
	pthread_rwlock_t rwlock; // 첫 번째 방법에 사용할 rwlock - pthread_rwlock을 그대로 사용한다
	pthread_spinlock_t lock; // 두 번째 방법에 사용할 spinlock
	int num_of_readers; // 읽기 작업 하고있는 쓰레드의 개수
	int num_of_writers; // 쓰기 작업 하고있는 쓰레드의 개수
	int num_of_write_requests; // 쓰기 lock 요청한 쓰레드의 개수 - 두 번째 방법에 사용
};

void init_rwlock(struct rw_lock * rw);
void r_lock(struct rw_lock * rw);
void r_unlock(struct rw_lock * rw);
void w_lock(struct rw_lock * rw);
void w_unlock(struct rw_lock * rw);
long *max_element(long* start, long* end);
long *min_element(long* start, long* end);
