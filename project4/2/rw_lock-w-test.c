#include "rw_lock.h"

void init_rwlock(struct rw_lock * rw)
{
	//	Write the code for initializing your read-write lock.
	pthread_spin_init(&(rw->lock), 0); // spinlock 초기화
	rw->num_of_readers = 0; // 멤버 변수 초기화
	rw->num_of_writers = 0;	// 멤버 변수 초기화
	rw->num_of_write_requests = 0; // 멤버 변수 초기화
}

void r_lock(struct rw_lock * rw)
{
	//	Write the code for aquiring read-write lock by the reader.
	while (1) {
		if (rw->num_of_writers <= 0 && rw->num_of_write_requests <= 0) { // 쓰기 중인 쓰레드, 쓰기 요청한 쓰레드 없다면
			pthread_spin_lock(&(rw->lock)); // lock
			++(rw->num_of_readers); // 읽기 중인 쓰레드 개수 + 1
			pthread_spin_unlock(&(rw->lock)); // unlock
			break; // 반복 종료
		}

		sched_yield(); // 다른 쓰레드에게 넘김
	}
}

void r_unlock(struct rw_lock * rw)
{
	//	Write the code for releasing read-write lock by the reader.
	pthread_spin_lock(&(rw->lock)); // lock
	--(rw->num_of_readers); // 읽기 중인 쓰레드 개수 - 1
	pthread_spin_unlock(&(rw->lock)); // unlock
}

void w_lock(struct rw_lock * rw)
{
	//	Write the code for aquiring read-write lock by the writer.
	pthread_spin_lock(&(rw->lock)); // lock
	++(rw->num_of_write_requests); // 쓰기 요청한 쓰레드 개수 + 1
	pthread_spin_unlock(&(rw->lock)); // unlock

	while (1) {
		if (rw->num_of_readers <= 0 && rw->num_of_writers <= 0) { // 읽기, 쓰기 중인 쓰레드가 없다면
			pthread_spin_lock(&(rw->lock)); // lock
			--(rw->num_of_write_requests); // 쓰기 요청한 쓰레드 개수 - 1
			++(rw->num_of_writers); // 쓰기 작업중인 쓰레드 개수 + 1
			pthread_spin_unlock(&(rw->lock)); // lock
			break; // 반복 종료
		}
		sched_yield(); // 다른 쓰레드에게 넘김
	}
}

void w_unlock(struct rw_lock * rw)
{
	//	Write the code for releasing read-write lock by the writer.
	pthread_spin_lock(&(rw->lock)); // lock
	--(rw->num_of_writers); // 쓰기 작업 중인 쓰레드 개수 - 1
	pthread_spin_unlock(&(rw->lock)); // unlock
}
