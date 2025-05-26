#pragma once

#include <chrono>
#include <ctime>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

enum class LogLevel { DEBUG, INFO, WARNING, ERROR, CRITICAL };

class LogHandler {
   public:
	virtual ~LogHandler() = default;
	virtual void log(LogLevel level, const std::string &msg) = 0;
};

class FileLogHandler final : public LogHandler {
   private:
	std::ofstream log_file_;
	std::mutex mutex_;

	std::string format_msg(LogLevel level, const std::string &msg);
	std::string get_level_string(LogLevel level);

   public:
	explicit FileLogHandler(const std::string &filename);
	~FileLogHandler();
	void log(LogLevel level, const std::string &msg) override;
};

class ConsoleLogHandler final : public LogHandler {
   private:
	std::mutex mutex_;

	std::string format_msg(LogLevel level, const std::string &msg);
	std::string get_level_string(LogLevel level);

   public:
	void log(LogLevel level, const std::string &msg) override;
};

class Logger {
   private:
	std::vector<std::unique_ptr<LogHandler>> handlers_;
	LogLevel log_level_;

	Logger(std::vector<std::unique_ptr<LogHandler>> &&handlers, LogLevel level);

   public:
	class Builder {
	   private:
		std::vector<std::unique_ptr<LogHandler>> handlers_;
		LogLevel log_level_;

	   public:
		Builder();

		Builder &set_log_level(LogLevel level);
		Builder &add_handler(std::unique_ptr<LogHandler> handler);
		Builder &add_console_handler();
		Builder &add_file_handler(const std::string &filename);

		Logger build();
	};

	~Logger();

	void log(LogLevel level, const std::string &message);

	void debug(const std::string &message);
	void info(const std::string &message);
	void warning(const std::string &message);
	void error(const std::string &message);
	void critical(const std::string &message);
};
