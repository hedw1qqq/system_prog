#pragma once

#include <string>
#include <cstdint>
#include "TCPClient.hpp"
#include "logger.hpp"

namespace client {

class ClientApp {
public:
	ClientApp(const std::string& host, uint16_t port, Logger& logger);

	void compile(const std::string& path);

	void play();

private:
	std::string host_;
	uint16_t    port_;
	Logger&     logger_;
	TCPClient make_connection();
};

} 
