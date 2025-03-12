#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <unistd.h>

#define N 5

sem_t forks[N];
sem_t mutex;
void take_fork(int idx) {
	sem_wait(&mutex);
	sem_wait(&forks[idx]);
	sem_wait(&forks[(idx + 1) % N]);
	printf("Philosopher %d took forks %d and %d\n", idx, idx, ((idx + 1) % N));
	sem_post(&mutex);
}

void put_fork(int idx) {
	printf("Philosopher %d puts down forks %d and %d and starts thinking.\n", idx, idx, (idx + 1) % N);
	sem_post(&forks[idx]);
	sem_post(&forks[(idx + 1) % N]);
}

void *philosopher(void *arg) {
	int i = *((int *)arg);
	while (1) {
		printf("Philosopher %d is thinking\n", i);
		sleep(1);
		take_fork(i);
		sleep(1);
		put_fork(i);
	}
	return NULL;
}

int main() {
	int indices[N];
	sem_init(&mutex, 0, 1);
	for (int i = 0; i < N; i++) {
		sem_init(&forks[i], 0, 1);
	}
	pthread_t threads[N];
	for (int i = 0; i < N; i++) {
		indices[i] = i;
		pthread_create(&threads[i], NULL, philosopher, &indices[i]);
	}
	for (int i = 0; i < N; i++) {
		pthread_join(threads[i], NULL);
	}
	sem_destroy(&mutex);
	for (int i = 0; i < N; i++) {
		sem_destroy(&forks[i]);
	}
}
