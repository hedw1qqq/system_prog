#include <iostream>

#include "../include/Logger.h"

int main() {
	try {
		Logger logger =
		    Logger::Builder().setLogLevel(LogLevel::DEBUG).addConsoleHandler().addFileHandler("my_app.log").build();

		logger.info("start");
		logger.warning("warning");
		logger.debug("debug");

	} catch (const std::exception& e) {
		std::cerr << "init error: " << e.what() << std::endl;
		return 1;
	}
	return 0;
}