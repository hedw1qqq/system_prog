//
// Created by ivglu on 28.03.2025.
//

#include "../include/ConsoleLogHandler.h"

std::string ConsoleLogHandler::formatMsg(const LogLevel level, const std::string &msg) {
	char timestamp[20];
	std::time_t now = std::time(nullptr);
	std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

	return std::string("[") + timestamp + "] " + "[" + getLevelString(level) + "] " + msg;
}

std::string ConsoleLogHandler::getLevelString(const LogLevel level) {
	switch (level) {
		case LogLevel::INFO:
			return "INFO";
		case LogLevel::DEBUG:
			return "DEBUG";
		case LogLevel::ERROR:
			return "ERROR";
		case LogLevel::WARNING:
			return "WARN";
		case LogLevel::CRITICAL:
			return "CRITICAL";
		default:
			return "UNKNOWN";
	}
}

void ConsoleLogHandler::log(const LogLevel level, const std::string &msg) {
	std::lock_guard lock(mutex_);
	std::cout << formatMsg(level, msg) << std::endl;
}
