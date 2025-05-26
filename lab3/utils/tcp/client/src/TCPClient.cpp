
#include "TCPClient.hpp"

#include <arpa/inet.h>
#include <unistd.h>

#include <cstring>

#include "custom_exceptions.hpp"

TCPClient::TCPClient(const std::string& host, uint16_t port, Logger& logger)
    : host_(host), port_(port), sock_fd_(-1), logger_(logger), connected_(false) {}

TCPClient::~TCPClient() { close(); }

void TCPClient::connect() {
	sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
	if (sock_fd_ < 0) {
		logger_.error("Failed to create socket");
		throw SocketException("socket() failed");  
	}

	sockaddr_in server_addr{};
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(port_);

	if (inet_pton(AF_INET, host_.c_str(), &server_addr.sin_addr) <= 0) {
		logger_.error("Invalid server address: " + host_);
		throw InvalidAddressException("inet_pton() failed for host: " + host_);
	}

	if (::connect(sock_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
		logger_.error("Error connecting to server");
		throw ConnectionException("connect() failed to " + host_ + ":" + std::to_string(port_));
	}

	connected_ = true;
	logger_.info("Connection to server established");
}

void TCPClient::send(const std::vector<uint8_t>& data) {
	if (!connected_) {
		logger_.warning("Attempt to send on closed connection");
		throw TransmissionException("Send on disconnected socket");
	}

	uint32_t size = data.size();
	uint32_t net_size = htonl(size);

	if (::send(sock_fd_, &net_size, sizeof(net_size), 0) != sizeof(net_size)) {
		logger_.warning("Failed to send data length header");
		throw TransmissionException("Failed to send length header");
	}
	if (::send(sock_fd_, data.data(), size, 0) != static_cast<ssize_t>(size)) {
		logger_.warning("Failed to send full data payload");
		throw TransmissionException("Partial data payload sent");
	}
	logger_.debug("Sent " + std::to_string(size) + " bytes");
}

std::vector<uint8_t> TCPClient::receive() {
	if (!connected_) {
		logger_.warning("Attempt to receive on closed connection");
		throw TransmissionException("Receive on disconnected socket");
	}

	uint32_t net_size;
	if (::recv(sock_fd_, &net_size, sizeof(net_size), MSG_WAITALL) != sizeof(net_size)) {
		logger_.error("Failed to receive data length header");
		throw TransmissionException("Failed to receive length header");
	}
	uint32_t size = ntohl(net_size);

	std::vector<uint8_t> buffer(size);
	if (::recv(sock_fd_, buffer.data(), size, MSG_WAITALL) != static_cast<ssize_t>(size)) {
		logger_.error("Failed to receive full data payload");
		throw TransmissionException("Incomplete data payload received");
	}
	logger_.debug("Received " + std::to_string(size) + " bytes");

	return buffer;
}

void TCPClient::close() {
	if (connected_) {
		::close(sock_fd_);
		logger_.info("Connection closed");
		connected_ = false;
	}
}
