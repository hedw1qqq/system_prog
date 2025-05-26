#include "../include/shared_memory.hpp"

SharedMemory::SharedMemory(const std::string& name, size_t size, bool create, Logger& logger)
	: name_(name), size_(size), fd_(-1), ptr_(MAP_FAILED), logger_(logger)
{
	int flags = O_RDWR;
	if (create) flags |= O_CREAT | O_EXCL;
	fd_ = ::shm_open(name_.c_str(), flags, 0666);
	if (fd_ < 0) {
		logger_.error("shm_open failed: " + name_);
		throw SharedMemoryException("shm_open(" + name_ + ") failed");
	}

	if (create) {
		if (ftruncate(fd_, size_) < 0) {
			logger_.error("ftruncate failed for: " + name_);
			throw SharedMemoryException("ftruncate(" + name_ + ") failed");
		}
	}

	ptr_ = mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
	if (ptr_ == MAP_FAILED) {
		logger_.error("mmap failed for: " + name_);
		throw SharedMemoryException("mmap(" + name_ + ") failed");
	}

	logger_.info("SharedMemory " + name_ + (create ? " created" : " opened"));
}

SharedMemory::~SharedMemory() {
	close();
}

void SharedMemory::close() {
	if (ptr_ != MAP_FAILED) {
		munmap(ptr_, size_);
		ptr_ = MAP_FAILED;
	}
	if (fd_ >= 0) {
		::close(fd_);
		fd_ = -1;
	}
}

void SharedMemory::unlink() {
	if (::shm_unlink(name_.c_str()) < 0) {
		logger_.warning("shm_unlink failed for: " + name_);
	} else {
		logger_.info("SharedMemory " + name_ + " unlinked");
	}
}
