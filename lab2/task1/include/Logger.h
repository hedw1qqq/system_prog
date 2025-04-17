#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ConsoleLogHandler.h"
#include "FileLogHandler.h"
#include "LogHandler.h"

class Logger {
   private:
	std::vector<std::unique_ptr<LogHandler>> handlers_;
	LogLevel logLevel_;

	Logger(std::vector<std::unique_ptr<LogHandler>> &&handlers, LogLevel level);

   public:
	class Builder {
	   private:
		std::vector<std::unique_ptr<LogHandler>> handlers;
		LogLevel logLevel;

	   public:
		Builder();

		Builder &setLogLevel(LogLevel level);

		Builder &addHandler(std::unique_ptr<LogHandler> handler);

		Builder &addConsoleHandler();

		Builder &addFileHandler(const std::string &filename);

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
