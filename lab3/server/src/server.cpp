
#include "server.hpp"

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <iostream>
#include <vector>

#include "../../subprocesses/compiler/include/compiler.hpp"
#include "../../subprocesses/sticks_game/include/sticks_game.hpp"
#include "TCPServer.hpp"
#include "client_message_queue.hpp"
#include "custom_exceptions.hpp"
#include "semaphore.hpp"
#include "shared_memory.hpp"

class TCPClientConnection {
   public:
	TCPClientConnection(int fd, Logger& log) : fd_(fd), log_(log) {}
	~TCPClientConnection() {}

	std::vector<uint8_t> receive() {
		uint32_t net_size;
		ssize_t recv_bytes = ::recv(fd_, &net_size, sizeof(net_size), MSG_WAITALL);
		if (recv_bytes == 0) throw TransmissionException("Client disconnected while waiting for header.");
		if (recv_bytes != sizeof(net_size)) {
			log_.error("recv header failed: " + std::string(strerror(errno)) + ", received " +
			           std::to_string(recv_bytes));
			throw TransmissionException("recv header failed");
		}
		auto size = ntohl(net_size);
		if (size > 1024 * 1024 * 20) {
			log_.error("Declared payload size too large: " + std::to_string(size));
			throw TransmissionException("Declared payload size too large");
		}
		std::vector<uint8_t> buf(size);
		if (size > 0) {
			recv_bytes = ::recv(fd_, buf.data(), size, MSG_WAITALL);
			if (recv_bytes == 0) throw TransmissionException("Client disconnected while waiting for payload.");
			if (recv_bytes != static_cast<ssize_t>(size)) {
				log_.error("recv payload failed: " + std::string(strerror(errno)) + ", received " +
				           std::to_string(recv_bytes) + " expected " + std::to_string(size));
				throw TransmissionException("recv payload failed");
			}
		}
		return buf;
	}
	void send(const std::vector<uint8_t>& data) {
		uint32_t net_size = htonl(data.size());
		if (::send(fd_, &net_size, sizeof(net_size), 0) != sizeof(net_size)) {
			log_.error("send header failed: " + std::string(strerror(errno)));
			throw TransmissionException("send header failed");
		}
		if (!data.empty()) {
			if (::send(fd_, data.data(), data.size(), 0) != static_cast<ssize_t>(data.size())) {
				log_.error("send payload failed: " + std::string(strerror(errno)));
				throw TransmissionException("send payload failed");
			}
		}
	}

   private:
	int fd_;
	Logger& log_;
};

void handle_client(int client_fd, Logger& log) {
	log.info("Handling new client on fd: " + std::to_string(client_fd));
	TCPClientConnection conn(client_fd, log);

	try {
		auto cmd_buf = conn.receive();
		std::string cmd(cmd_buf.begin(), cmd_buf.end());
		log.debug("Received command: " + cmd + " from fd: " + std::to_string(client_fd));
		long game_session_id = static_cast<long>(client_fd);
		if (cmd == "COMPILE") {
			auto name_buf = conn.receive();
			std::string filename(name_buf.begin(), name_buf.end());
			auto file_buf = conn.receive();

			log.info("Compile request for '" + filename + "' (" + std::to_string(file_buf.size()) + " bytes)");

			SharedMemory shm(SHM_NAME, sizeof(CompilationSharedData), false, log);
			Semaphore sem_req(SEM_REQ_NAME, 0, log);
			Semaphore sem_resp(SEM_RESP_NAME, 0, log);
			auto data = reinterpret_cast<CompilationSharedData*>(shm.data());

			if (file_buf.size() > MAX_FILE_SIZE) {
				throw std::runtime_error("File too large for compilation SHM buffer");
			}
			if (filename.length() >= MAX_FILE_NAME) {
				throw std::runtime_error("Filename too long");
			}

			data->status = CompileStatus::PENDING;
			std::strncpy(data->file_name, filename.c_str(), MAX_FILE_NAME - 1);
			data->file_name[MAX_FILE_NAME - 1] = '\0';
			data->file_size = static_cast<uint32_t>(file_buf.size());
			std::memcpy(data->file_data, file_buf.data(), file_buf.size());

			log.info("Compile request for " + filename);
			sem_req.post();
			log.debug("Waiting for compiler response for " + filename);
			sem_resp.wait();
			log.debug("Compiler response received for " + filename);

			if (data->status == CompileStatus::SUCCESS) {
				if (data->result_size > MAX_RESULT_SIZE) {
					log.error("Result size " + std::to_string(data->result_size) + " exceeds MAX_RESULT_SIZE for " +
					          filename);
					throw std::runtime_error("Compiled result too large from SHM");
				}
				std::vector<uint8_t> out(data->result_data, data->result_data + data->result_size);
				conn.send(out);
				log.info("Sent compiled result to client for " + filename);
			} else {
				std::string err_msg = "COMPILATION_FAILED";
				conn.send(std::vector<uint8_t>(err_msg.begin(), err_msg.end()));
				log.error("Compilation failed for " + filename + ", reported by subserver.");
			}
		} else if (cmd == "PLAY") {
			log.info("Play game request from fd: " + std::to_string(client_fd));
			ClientMessageQueue mq(log);

			while (true) {
				auto move_buf = conn.receive();
				if (move_buf.size() != sizeof(int)) {
					log.error("PLAY: Invalid move size from client. Expected " + std::to_string(sizeof(int)) + " got " +
					          std::to_string(move_buf.size()));
					throw TransmissionException("Invalid move size from client");
				}
				int client_take;
				std::memcpy(&client_take, move_buf.data(), sizeof(int));
				log.debug("PLAY: Client wants to take " + std::to_string(client_take) + " sticks.");

				mq.send_request(game_session_id, client_take);
				GameResponse game_resp = mq.receive_response(game_session_id);

				log.debug("PLAY: Subserver response: server_took=" + std::to_string(game_resp.taken) + ", client_won=" +
				          (game_resp.client_won ? "T" : "F") + ", server_won=" + (game_resp.server_won ? "T" : "F") +
				          ", Sticks left: " + std::to_string(game_resp.remaining_sticks));

				std::vector<uint8_t> out_buf(sizeof(int) + sizeof(uint8_t) + sizeof(uint8_t) + sizeof(int));
				size_t offset = 0;
				std::memcpy(out_buf.data() + offset, &game_resp.taken, sizeof(int));
				offset += sizeof(int);
				out_buf[offset++] = static_cast<uint8_t>(game_resp.client_won);
				out_buf[offset++] = static_cast<uint8_t>(game_resp.server_won);
				std::memcpy(out_buf.data() + offset, &game_resp.remaining_sticks, sizeof(int));

				conn.send(out_buf);

				if (game_resp.client_won || game_resp.server_won) {
					log.info("PLAY: Game ended for session_id=" + std::to_string(game_session_id) +
					         ". Client_won: " + std::to_string(game_resp.client_won) +
					         ", Server_won: " + std::to_string(game_resp.server_won) +
					         ", Sticks left: " + std::to_string(game_resp.remaining_sticks));
					break;
				} else {
					log.info("PLAY: Turn ended for session_id=" + std::to_string(game_session_id) +
					         ". Server took: " + std::to_string(game_resp.taken) +
					         ", Sticks left: " + std::to_string(game_resp.remaining_sticks));
				}
			}
		} else {
			log.warning("Unknown command: '" + cmd + "' from fd: " + std::to_string(client_fd));
			std::string err_msg = "UNKNOWN_COMMAND";
			conn.send(std::vector<uint8_t>(err_msg.begin(), err_msg.end()));
		}
	} catch (const TransmissionException& ex) {
		log.warning("TransmissionException for fd=" + std::to_string(client_fd) + ": " + ex.what());
	} catch (const IPCException& ex) {
		log.error("IPCException for fd=" + std::to_string(client_fd) + ": " + ex.what());
		try {
			std::string err_msg = "SERVER_IPC_ERROR";
			conn.send(std::vector<uint8_t>(err_msg.begin(), err_msg.end()));
		} catch (const std::exception& send_ex) {
			log.error("Failed to send IPC_ERROR to client: " + std::string(send_ex.what()));
		}
	} catch (const std::exception& ex) {
		log.error("Client handler generic exception for fd=" + std::to_string(client_fd) + ": " +
		          std::string(ex.what()));
		try {
			std::string err_msg = "SERVER_ERROR";
			conn.send(std::vector<uint8_t>(err_msg.begin(), err_msg.end()));
		} catch (const std::exception& send_ex) {
			log.error("Failed to send SERVER_ERROR to client: " + std::string(send_ex.what()));
		}
	}

	log.info("Finished handling client on fd: " + std::to_string(client_fd));
}