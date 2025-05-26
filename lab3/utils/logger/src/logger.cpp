#include "../include/Logger.hpp"

std::string ConsoleLogHandler::format_msg(const LogLevel level, const std::string &msg) {
	char timestamp[20];
	std::time_t now = std::time(nullptr);
	std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
	return std::string("[") + timestamp + "] [" + get_level_string(level) + "] " + msg;
}

std::string ConsoleLogHandler::get_level_string(const LogLevel level) {
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
	std::cout << format_msg(level, msg) << std::endl;
}

FileLogHandler::FileLogHandler(const std::string &filename) : log_file_(filename, std::ios::app) {
	if (!log_file_.is_open()) {
		throw std::runtime_error("FileLogHandler::FileLogHandler(): cannot open file: " + filename);
	}
}

std::string FileLogHandler::format_msg(const LogLevel level, const std::string &msg) {
	char timestamp[20];
	std::time_t now = std::time(nullptr);
	std::strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", std::localtime(&now));
	return std::string("[") + timestamp + "] [" + get_level_string(level) + "] " + msg;
}

std::string FileLogHandler::get_level_string(const LogLevel level) {
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

void FileLogHandler::log(const LogLevel level, const std::string &msg) {
	std::lock_guard lock(mutex_);
	if (log_file_.is_open()) {
		log_file_ << format_msg(level, msg) << std::endl;
		log_file_.flush();
	}
}

FileLogHandler::~FileLogHandler() {
	if (log_file_.is_open()) {
		log_file_.close();
	}
}

Logger::Builder::Builder() : log_level_(LogLevel::INFO) {}

Logger::Builder &Logger::Builder::set_log_level(LogLevel level) {
	log_level_ = level;
	return *this;
}

Logger::Builder &Logger::Builder::add_handler(std::unique_ptr<LogHandler> handler) {
	if (handler) {
		handlers_.push_back(std::move(handler));
	}
	return *this;
}

Logger::Builder &Logger::Builder::add_console_handler() { return add_handler(std::make_unique<ConsoleLogHandler>()); }

Logger::Builder &Logger::Builder::add_file_handler(const std::string &filename) {
	try {
		return add_handler(std::make_unique<FileLogHandler>(filename));
	} catch (const std::exception &) {
		return *this;
	}
}

Logger Logger::Builder::build() { return Logger(std::move(handlers_), log_level_); }

Logger::Logger(std::vector<std::unique_ptr<LogHandler>> &&handlers, LogLevel level)
    : handlers_(std::move(handlers)), log_level_(level) {}

Logger::~Logger() = default;

void Logger::log(LogLevel level, const std::string &message) {
	if (level >= log_level_) {
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
