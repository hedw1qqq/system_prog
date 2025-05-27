#pragma once

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>

#include "custom_exceptions.hpp"
#include "logger.hpp"

class SharedMemory {
public:
    SharedMemory(const std::string &name, size_t size, bool create, Logger &logger);

    ~SharedMemory();

    void *data() const { return ptr_; }

    void close();

    void unlink();

private:
    std::string name_;
    size_t size_;
    int fd_;
    void *ptr_;
    Logger &logger_;
};
