#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <memory>
#include <netinet/in.h>

#include "logger.hpp"

class TCPClient {
public:
  TCPClient(const std::string& host, uint16_t port, Logger& logger);
  ~TCPClient();

  void connect();
  void send(const std::vector<uint8_t>& data);
  std::vector<uint8_t> receive();
  void close();

private:
  std::string host_;
  uint16_t port_;
  int sock_fd_;
  Logger& logger_;
  bool connected_;
};
