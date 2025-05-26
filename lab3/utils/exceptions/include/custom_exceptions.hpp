#pragma once

#include <stdexcept>
#include <string>

class SystemException : public std::runtime_error {
   public:
	explicit SystemException(const std::string& msg) : std::runtime_error(msg) {}
};

class SocketException : public SystemException {
   public:
	explicit SocketException(const std::string& msg) : SystemException("SocketException: " + msg) {}
};

class InvalidAddressException : public SocketException {
   public:
	explicit InvalidAddressException(const std::string& msg) : SocketException("InvalidAddressException: " + msg) {}
};

class ConnectionException : public SocketException {
   public:
	explicit ConnectionException(const std::string& msg) : SocketException("ConnectionException: " + msg) {}
};

class TransmissionException : public SystemException {
   public:
	explicit TransmissionException(const std::string& msg) : SystemException("TransmissionException: " + msg) {}
};

class IPCException : public SystemException {
   public:
	explicit IPCException(const std::string& msg) : SystemException("IPCException: " + msg) {}
};

class SharedMemoryException : public IPCException {
   public:
	explicit SharedMemoryException(const std::string& msg) : IPCException("SharedMemoryException: " + msg) {}
};

class SemaphoreException : public IPCException {
   public:
	explicit SemaphoreException(const std::string& msg) : IPCException("SemaphoreException: " + msg) {}
};

class MessageQueueException : public IPCException {
   public:
	explicit MessageQueueException(const std::string& msg) : IPCException("MessageQueueException: " + msg) {}
};
