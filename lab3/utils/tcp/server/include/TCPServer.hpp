#pragma once

#include <netinet/in.h>

#include <atomic>
#include <cstdint>
#include <functional>
#include <thread>

#include "custom_exceptions.hpp"
#include "logger.hpp"

class TCPServer {
   public:
	using Handler = std::function<void(int client_fd)>;

	TCPServer(uint16_t port, Logger& logger, size_t backlog = 5);
	~TCPServer();

	void start(const Handler& handler);

	void stop();

   private:
	void acceptLoop(const Handler& handler);

	uint16_t port_;
	int listen_fd_;
	size_t backlog_;
	std::atomic<bool> running_;
	Logger& logger_;
	std::thread accept_thread_;
};
