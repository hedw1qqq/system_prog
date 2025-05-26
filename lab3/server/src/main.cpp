
#include <csignal>
#include <unistd.h>

#include <atomic>

#include "logger.hpp"
#include "server.hpp"

static Logger app_logger =
    Logger::Builder().set_log_level(LogLevel::INFO).add_console_handler().add_file_handler("server.log").build();

static std::atomic<bool> server_is_running(true);

void sigint_handler(int signum) {
	(void)signum;

	server_is_running.store(false);
}

int main() {
	app_logger.info("Main server starting...");
	struct sigaction sa;
	sa.sa_handler = sigint_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = 0;
	if (sigaction(SIGINT, &sa, NULL) == -1) {
		app_logger.error("Failed to set SIGINT handler");
		return 1;
	}
	if (sigaction(SIGTERM, &sa, NULL) == -1) {
		app_logger.error("Failed to set SIGTERM handler");
		return 1;
	}

	SharedMemory shm(SHM_NAME, sizeof(CompilationSharedData), true, app_logger);
	Semaphore sem_req(SEM_REQ_NAME, 0, app_logger);
	Semaphore sem_resp(SEM_RESP_NAME, 0, app_logger);
	app_logger.info("Compiler IPC (SHM, Semaphores) created.");

	TCPServer tcp_server_instance(SERVER_PORT, app_logger);
	tcp_server_instance.start([&](int fd) { handle_client(fd, app_logger); });

	app_logger.info("Main server is listening on port " + std::to_string(SERVER_PORT) +
	                ". Waiting for signal to shut down.");

	while(server_is_running.load()) {
		pause();
	}

	app_logger.info("Shutdown signal received. Stopping TCP server listener...");
	tcp_server_instance.stop();

	app_logger.info("Cleaning up compiler IPC resources...");
	sem_req.unlink();
	sem_resp.unlink();
	shm.unlink();
	app_logger.info("Compiler IPC resources unlinked.");

	app_logger.info("Main server shut down successfully.");
	return 0;
}