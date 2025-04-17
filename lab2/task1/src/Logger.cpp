

#include "../include/Logger.h"

Logger::Builder::Builder() : logLevel(LogLevel::INFO) {}

Logger::Builder &Logger::Builder::setLogLevel(LogLevel level) {
	logLevel = level;
	return *this;
}

Logger::Builder &Logger::Builder::addHandler(std::unique_ptr<LogHandler> handler) {
	if (handler) {
		handlers.push_back(std::move(handler));
	}
	return *this;
}

Logger::Builder &Logger::Builder::addConsoleHandler() { return addHandler(std::make_unique<ConsoleLogHandler>()); }

Logger::Builder &Logger::Builder::addFileHandler(const std::string &filename) {
	try {
		return addHandler(std::make_unique<FileLogHandler>(filename));
	} catch (const std::exception &e) {
		return *this;
	}
}

Logger Logger::Builder::build() { return Logger(std::move(handlers), logLevel); }

Logger::Logger(std::vector<std::unique_ptr<LogHandler>> &&handlers, LogLevel level)
    : handlers_(std::move(handlers)), logLevel_(level) {}

Logger::~Logger() = default;

void Logger::log(LogLevel level, const std::string &message) {
	if (level >= logLevel_) {
		for (auto &handler : handlers_) {
			if (handler) {
				handler->log(level, message);
			}
		}
	}
}

void Logger::debug(const std::string &message) { log(LogLevel::DEBUG, message); }
void Logger::info(const std::string &message) { log(LogLevel::INFO, message); }
void Logger::warning(const std::string &message) { log(LogLevel::WARNING, message); }
void Logger::error(const std::string &message) { log(LogLevel::ERROR, message); }
void Logger::critical(const std::string &message) { log(LogLevel::CRITICAL, message); }
