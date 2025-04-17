
#pragma once
#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <vector>

enum class LogLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };

class LogHandler {
   public:
	virtual ~LogHandler() = default;
	virtual void log(LogLevel level, const std::string &msg) = 0;
};
