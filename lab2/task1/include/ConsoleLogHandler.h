//
// Created by ivglu on 28.03.2025.
//
#pragma once
#include "LogHandler.h"
class ConsoleLogHandler final : public LogHandler {
   public:
	void log(LogLevel level, const std::string &msg) override;

   private:
	std::mutex mutex_;

	std::string formatMsg(LogLevel level, const std::string &msg);

	std::string getLevelString(LogLevel level);
};
