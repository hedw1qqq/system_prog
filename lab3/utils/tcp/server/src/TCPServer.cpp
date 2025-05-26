#include "TCPServer.hpp"

#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <cstring>
#include <system_error>

TCPServer::TCPServer(uint16_t port, Logger& logger, size_t backlog)
    : port_(port), listen_fd_(-1), backlog_(backlog), running_(false), logger_(logger) {}

TCPServer::~TCPServer() { stop(); }

void TCPServer::start(const Handler& handler) {
	if (running_.load()) {
		logger_.warning("Server is already running on port " + std::to_string(port_));
		return;
	}

	listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
	if (listen_fd_ < 0) {
		logger_.error("Failed to create listen socket: " + std::string(strerror(errno)));
		throw SocketException("socket() failed");
	}

	sockaddr_in addr{};
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(port_);

	int opt = 1;
	if (setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
		logger_.warning("setsockopt(SO_REUSEADDR) failed: " + std::string(strerror(errno)));
	}

	if (bind(listen_fd_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
		::close(listen_fd_);
		listen_fd_ = -1;
		logger_.error("Failed to bind to port " + std::to_string(port_) + ": " + std::string(strerror(errno)));
		throw SocketException("bind() failed on port " + std::to_string(port_));
	}

	if (listen(listen_fd_, static_cast<int>(backlog_)) < 0) {
		::close(listen_fd_);
		listen_fd_ = -1;
		logger_.error("Failed to listen on port " + std::to_string(port_) + ": " + std::string(strerror(errno)));
		throw SocketException("listen() failed");
	}

	running_ = true;
	logger_.info("Server started, listening on port " + std::to_string(port_));

	accept_thread_ = std::thread(&TCPServer::acceptLoop, this, handler);
}

void TCPServer::acceptLoop(const Handler& handler) {
	logger_.debug("Accept loop started for port " + std::to_string(port_));
	while (running_.load()) {
		sockaddr_in client_addr;
		socklen_t addr_len = sizeof(client_addr);
		int client_fd = accept(listen_fd_, reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

		if (!running_.load()) {
			if (client_fd >= 0) {
				logger_.debug("Accepted a connection during shutdown, closing fd=" + std::to_string(client_fd));
				::close(client_fd);
			}
			logger_.debug("Accept loop: running_ is false, exiting.");
			break;
		}

		if (client_fd < 0) {
			if (running_.load()) {
				logger_.warning("accept() failed on port " + std::to_string(port_) + ": " +
				                std::string(strerror(errno)) + ". Retrying...");

			} else {
				logger_.info("accept() returned error during shutdown for port " + std::to_string(port_) +
				             ", as expected.");
			}

			continue;
		}

		logger_.info("New client connected, fd=" + std::to_string(client_fd) + " on port " + std::to_string(port_));

		std::thread([handler, client_fd, this_logger = &logger_]() {
			try {
				handler(client_fd);
			} catch (const std::exception& ex) {
				this_logger->error("Handler exception for fd=" + std::to_string(client_fd) + ": " + ex.what());
			}

			this_logger->info("Client fd=" + std::to_string(client_fd) + " processing thread finished.");
		}).detach();
	}
	logger_.debug("Accept loop finished for port " + std::to_string(port_));
}

void TCPServer::stop() {
	if (!running_.exchange(false)) {
		logger_.debug("Server on port " + std::to_string(port_) + " is already stopped or stopping.");
		return;
	}

	logger_.info("Server stopping on port " + std::to_string(port_));

	if (listen_fd_ != -1) {
		if (::shutdown(listen_fd_, SHUT_RD) < 0) {
			logger_.warning("shutdown(listen_fd_, SHUT_RD) failed for port " + std::to_string(port_) + ": " +
			                std::string(strerror(errno)));
		}

		if (::close(listen_fd_) < 0) {
			logger_.warning("close(listen_fd_) failed for port " + std::to_string(port_) + ": " +
			                std::string(strerror(errno)));
		}
		logger_.debug("Listen socket fd=" + std::to_string(listen_fd_) + " closed for port " + std::to_string(port_));
		listen_fd_ = -1;
	} else {
		logger_.debug("Listen socket was already closed or not opened for port " + std::to_string(port_));
	}

	if (accept_thread_.joinable()) {
		logger_.debug("Waiting for accept_thread to join for port " + std::to_string(port_));
		try {
			accept_thread_.join();
			logger_.debug("accept_thread joined successfully for port " + std::to_string(port_));
		} catch (const std::system_error& e) {
			logger_.error("System error while joining accept_thread for port " + std::to_string(port_) + ": " +
			              std::string(e.what()));
		}
	} else {
		logger_.debug("accept_thread was not joinable for port " + std::to_string(port_));
	}
	logger_.info("Server listener stopped successfully on port " + std::to_string(port_));
}