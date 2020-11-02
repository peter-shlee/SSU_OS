#include "rw_lock.h"

void init_rwlock(struct rw_lock * rw)
{
	//	Write the code for initializing your read-write lock.
	pthread_rwlock_init(&(rw->rwlock), NULL); // rwlock 초기화
	rw->num_of_readers = 0; // 멤버 변수 초기화
	rw->num_of_writers = 0; // 멤버 변수 초기화

	return;
}

void r_lock(struct rw_lock * rw)
{
	//	Write the code for aquiring read-write lock by the reader.
	pthread_rwlock_rdlock(&(rw->rwlock)); // read lock
	return;
}

void r_unlock(struct rw_lock * rw)
{
	//	Write the code for releasing read-write lock by the reader.
	pthread_rwlock_unlock(&(rw->rwlock)); // read unlock
	return;
}

void w_lock(struct rw_lock * rw)
{
	//	Write the code for aquiring read-write lock by the writer.
	pthread_rwlock_wrlock(&(rw->rwlock)); // write lock
	return;
}

void w_unlock(struct rw_lock * rw)
{
	//	Write the code for releasing read-write lock by the writer.
	pthread_rwlock_unlock(&(rw->rwlock)); // unlock rwlock
	return;
}
