//
// Created by ivglu on 28.03.2025.
//
#pragma once
#include <mutex>

#include "LogHandler.h"

class FileLogHandler final : public LogHandler {
   private:
	std::ofstream logfile_;
	std::mutex mutex_;

	std::string formatMsg(LogLevel level, const std::string &msg);

	std::string getLevelString(LogLevel level);

   public:
	explicit FileLogHandler(const std::string &filename);

	~FileLogHandler();
	void log(LogLevel level, const std::string &msg);
};
