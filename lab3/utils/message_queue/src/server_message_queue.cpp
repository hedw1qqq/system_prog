#include "server_message_queue.hpp"

#include <sys/ipc.h>
#include <sys/msg.h>

#include <cerrno>
#include <cstring>

ServerMessageQueue::ServerMessageQueue(Logger& logger) : logger_(logger) {
	msqid_ = msgget(GAME_MSG_KEY, IPC_CREAT | IPC_EXCL | 0666);
	if (msqid_ < 0) {
		if (errno == EEXIST) {
			logger_.info("ServerMessageQueue: Message queue already exists, attempting to open.");
			msqid_ = msgget(GAME_MSG_KEY, 0666);
			if (msqid_ < 0) {
				logger_.error("ServerMessageQueue: msgget() failed to open existing queue: " +
				              std::string(strerror(errno)));
				throw MessageQueueException("ServerMessageQueue: msgget() failed to open existing queue");
			}
		} else {
			logger_.error("ServerMessageQueue: msgget(IPC_CREAT|IPC_EXCL) failed: " + std::string(strerror(errno)));
			throw MessageQueueException("ServerMessageQueue: msgget(IPC_CREAT|IPC_EXCL) failed");
		}
	}
	logger_.info("ServerMessageQueue: Message queue created/opened, msqid=" + std::to_string(msqid_));
}

ServerMessageQueue::~ServerMessageQueue() {}

GameRequest ServerMessageQueue::receive_request() {
	GameRequest req;

	if (msgrcv(msqid_, &req, sizeof(GameRequest) - sizeof(long), REQUEST_TAG, 0) < 0) {
		logger_.error("ServerMessageQueue: msgrcv() failed to receive request: " + std::string(strerror(errno)));
		throw MessageQueueException("ServerMessageQueue: msgrcv() failed to receive request");
	}
	logger_.debug("ServerMessageQueue: Received GameRequest: session_id=" + std::to_string(req.session_id) +
	              ", take=" + std::to_string(req.take));
	return req;
}

void ServerMessageQueue::send_response(const GameResponse& resp) {
	if (msgsnd(msqid_, &resp, sizeof(GameResponse) - sizeof(long), 0) < 0) {
		logger_.error("ServerMessageQueue: msgsnd() failed to send response to session_id=" +
		              std::to_string(resp.mtype) + ": " + std::string(strerror(errno)));
		throw MessageQueueException("ServerMessageQueue: msgsnd() failed to send response");
	}
	logger_.debug("ServerMessageQueue: Sent GameResponse to session_id=" + std::to_string(resp.mtype) +
	              ", server_took=" + std::to_string(resp.taken));
}

void ServerMessageQueue::remove_queue() {
	if (msqid_ >= 0) {
		if (msgctl(msqid_, IPC_RMID, nullptr) < 0) {
			logger_.warning("ServerMessageQueue: msgctl(IPC_RMID) failed for msqid=" + std::to_string(msqid_) + ": " +
			                std::string(strerror(errno)));
		} else {
			logger_.info("ServerMessageQueue: Message queue removed, msqid=" + std::to_string(msqid_));
		}
		msqid_ = -1;
	}
}