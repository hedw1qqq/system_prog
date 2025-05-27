#include <signal.h>
#include <unistd.h>

#include <atomic>

#include "compiler.hpp"
#include "logger.hpp"

static Logger app_logger =
    Logger::Builder().set_log_level(LogLevel::INFO).add_console_handler().add_file_handler("compiler.log").build();

static std::atomic<bool> compiler_running_flag(true);

void compiler_signal_handler(int signum) {
	(void)signum;

	compiler_running_flag = false;
}

int main() {
	struct sigaction sa;
	sa.sa_handler = compiler_signal_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		app_logger.error("Compiler: Failed to set SIGINT handler");
		return 1;
	}
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		app_logger.error("Compiler: Failed to set SIGTERM handler");
		return 1;
	}

	const std::string shm_name = "/compile_shm";
	const std::string sem_req_name = "/sem_req";
	const std::string sem_resp_name = "/sem_resp";

	app_logger.info("Compiler subserver starting.");
	try {
		run_compiler(shm_name, sem_req_name, sem_resp_name, app_logger, compiler_running_flag);
	} catch (const IPCException& e) {
		app_logger.error("Compiler subserver failed to initialize IPC: " + std::string(e.what()));
		app_logger.error("Main server is running and has created the IPC objects.");

	} catch (const std::exception& e) {
		app_logger.error("Compiler subserver exited with exception: " + std::string(e.what()));
	}

	if (compiler_running_flag) {
		app_logger.info("Compiler subserver finished unexpectedly.");
	} else {
		app_logger.info("Compiler subserver received shutdown signal and is terminating.");
	}
	app_logger.info("Compiler subserver shut down.");
	return 0;
}