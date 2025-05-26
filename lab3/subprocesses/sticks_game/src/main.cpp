
#include <signal.h>

#include <atomic>

#include "../include/sticks_game.hpp"
#include "logger.hpp"
#include "server_message_queue.hpp"

static Logger app_logger =
    Logger::Builder().set_log_level(LogLevel::INFO).add_console_handler().add_file_handler("sticks_game.log").build();

static std::atomic<bool> sticks_running_flag(true);
static ServerMessageQueue* mq_ptr = nullptr;

void sticks_sigint_handler(int signum) {
	(void)signum;
	app_logger.info("Sticks game subserver: SIGINT received, shutting down...");
	sticks_running_flag = false;
}

int main() {
	signal(SIGINT, sticks_sigint_handler);
	signal(SIGTERM, sticks_sigint_handler);

	ServerMessageQueue mq(app_logger);
	mq_ptr = &mq;

	app_logger.info("Sticks-game subserver started");
	try {
		run_sticks_game(app_logger, sticks_running_flag, mq);
	} catch (const std::exception& e) {
		app_logger.error("Sticks game run loop exited with exception: " + std::string(e.what()));
	} catch (...) {
		app_logger.error("Sticks game run loop exited with unknown exception.");
	}

	app_logger.info("Sticks-game subserver cleaning up message queue...");
	if (mq_ptr) {
		mq_ptr->remove_queue();
	}
	app_logger.info("Sticks-game subserver shut down.");
	return 0;
}