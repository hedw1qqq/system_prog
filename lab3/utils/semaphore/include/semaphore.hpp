#pragma once

#include <semaphore.h>

#include <string>

#include "custom_exceptions.hpp"
#include "logger.hpp"

class Semaphore {
   public:
	Semaphore(const std::string& name, unsigned int initial, Logger& logger);
	~Semaphore();

	void post();
	void wait();
	void close();
	void unlink();

   private:
	std::string name_;
	sem_t* sem_;
	Logger& logger_;
};
