
#include "../include/semaphore.hpp"

#include <fcntl.h>

Semaphore::Semaphore(const std::string& name, unsigned int initial, Logger& logger)
    : name_(name), sem_(nullptr), logger_(logger) {
	sem_ = sem_open(name_.c_str(), O_CREAT | O_EXCL, 0666, initial);
	if (sem_ == SEM_FAILED) {
		sem_ = sem_open(name_.c_str(), 0);
	}
	if (sem_ == SEM_FAILED) {
		logger_.error("sem_open failed: " + name_);
		throw SemaphoreException("sem_open(" + name_ + ") failed");
	}
	logger_.info("Semaphore " + name_ + " opened");
}

Semaphore::~Semaphore() { close(); }

void Semaphore::post() {
	if (sem_post(sem_) < 0) {
		logger_.error("sem_post failed: " + name_);
		throw SemaphoreException("sem_post(" + name_ + ") failed");
	}
}

void Semaphore::wait() {
	if (sem_wait(sem_) < 0) {
		logger_.error("sem_wait failed: " + name_);
		throw SemaphoreException("sem_wait(" + name_ + ") failed");
	}
}

void Semaphore::close() {
	if (sem_) {
		sem_close(sem_);
		sem_ = nullptr;
	}
}

void Semaphore::unlink() {
	if (sem_unlink(name_.c_str()) < 0) {
		logger_.warning("sem_unlink failed: " + name_);
	} else {
		logger_.info("Semaphore " + name_ + " unlinked");
	}
}
