#include "rw_lock.h"

void init_rwlock(struct rw_lock * rw)
{
	//	Write the code for initializing your read-write lock.
	//pthread_spin_init(&(rw->lock), 0);
	pthread_rwlock_init(&(rw->rwlock), NULL);
	rw->num_of_readers = 0;
	rw->num_of_writers = 0;

	return;
}

void r_lock(struct rw_lock * rw)
{
	//	Write the code for aquiring read-write lock by the reader.
//	while (1) {
//		if (rw->num_of_writers <= 0) {
//			pthread_spin_lock(&(rw->lock));
//			++(rw->num_of_readers);
//			pthread_spin_unlock(&(rw->lock));
//		}
//
//		sched_yield();
//	}
	pthread_rwlock_rdlock(&(rw->rwlock));
	return;
}

void r_unlock(struct rw_lock * rw)
{
	//	Write the code for releasing read-write lock by the reader.
//	pthread_spin_lock(&(rw->lock));
//	--(rw->num_of_readers);
//	pthread_spin_unlock(&(rw->lock));
	pthread_rwlock_unlock(&(rw->rwlock));
	return;
}

void w_lock(struct rw_lock * rw)
{
	//	Write the code for aquiring read-write lock by the writer.
//	while (1) {
//		if (rw->num_of_readers <= 0 && rw->num_of_writers <= 0) {
//			pthread_spin_lock(&(rw->lock));
//			++(rw->num_of_writers);
//			pthread_spin_unlock(&(rw->lock));
//			break;
//		}
//		sched_yield();
//	}
	pthread_rwlock_wrlock(&(rw->rwlock));
	return;
}

void w_unlock(struct rw_lock * rw)
{
	//	Write the code for releasing read-write lock by the writer.
//	pthread_spin_lock(&(rw->lock));
//	--(rw->num_of_writers);
//	pthread_spin_unlock(&(rw->lock));
	pthread_rwlock_unlock(&(rw->rwlock));
	return;
}
