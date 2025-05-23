#include <iostream>

#include "../include/Logger.h"

int main() {
	try {
		auto logger =
		    Logger::Builder().setLogLevel(LogLevel::CRITICAL).addConsoleHandler().addFileHandler("my_app.log").build();

		logger.info("start");
		logger.warning("warning");
		logger.debug("debug");

	} catch (const std::exception& e) {
		std::cerr << "init error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}