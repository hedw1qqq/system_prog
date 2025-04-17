//
// Created by ivglu on 28.03.2025.
//
#include "../include/FileLogHandler.h"

FileLogHandler::FileLogHandler(const std::string& filename) : logfile_(filename, std::ios::app) {
	if (!logfile_.is_open()) {
		throw std::runtime_error("FileLogHandler::FileLogHandler(): cannot open file: " + filename);
	}
}
std::string FileLogHandler::formatMsg(const LogLevel level, const std::string& msg) {
	char timestamp[20];
	const std::time_t now = std::time(nullptr);
	std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));

	return std::string("[") + timestamp + "] " + "[" + getLevelString(level) + "] " + msg;
}

std::string FileLogHandler::getLevelString(const LogLevel level) {
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

void FileLogHandler::log(const LogLevel level, const std::string& msg) {
	std::lock_guard lock(mutex_);
	if (logfile_.is_open()) {
		logfile_ << formatMsg(level, msg) << std::endl;
		logfile_.flush();
	}
}
FileLogHandler::~FileLogHandler() {
	if (logfile_.is_open()) {
		logfile_.close();
	}
}
