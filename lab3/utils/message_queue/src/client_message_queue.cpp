#include "client_message_queue.hpp"

#include <sys/ipc.h>
#include <sys/msg.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>

ClientMessageQueue::ClientMessageQueue(Logger& logger) : logger_(logger) {
	msqid_ = msgget(GAME_MSG_KEY, IPC_CREAT | 0666);
	if (msqid_ < 0) {
		logger_.error("ClientMessageQueue: msgget() failed: " + std::string(strerror(errno)));
		throw MessageQueueException("ClientMessageQueue: msgget() failed");
	}
	logger_.info("ClientMessageQueue: Message queue opened/accessed, msqid=" + std::to_string(msqid_));
}

ClientMessageQueue::~ClientMessageQueue() {}

void ClientMessageQueue::send_request(long session_id, int take) {
	GameRequest req;
	req.mtype = REQUEST_TAG;
	req.session_id = session_id;
	req.take = take;

	if (msgsnd(msqid_, &req, sizeof(GameRequest) - sizeof(long), 0) < 0) {
		logger_.error("ClientMessageQueue: msgsnd() failed to send request for session " + std::to_string(session_id) +
		              ": " + std::string(strerror(errno)));
		throw MessageQueueException("ClientMessageQueue: msgsnd() failed to send request");
	}
	logger_.debug("ClientMessageQueue: Sent GameRequest: session_id=" + std::to_string(req.session_id) +
	              ", take=" + std::to_string(take));
}

GameResponse ClientMessageQueue::receive_response(long session_id) {
	GameResponse resp;

	if (msgrcv(msqid_, &resp, sizeof(GameResponse) - sizeof(long), session_id, 0) < 0) {
		logger_.error("ClientMessageQueue: msgrcv() failed to receive response for session " +
		              std::to_string(session_id) + ": " + std::string(strerror(errno)));
		throw MessageQueueException("ClientMessageQueue: msgrcv() failed to receive response");
	}
	logger_.debug("ClientMessageQueue: Received GameResponse for session_id=" + std::to_string(resp.mtype) +
	              ": server_took=" + std::to_string(resp.taken) + ", client_won=" +
	              (resp.client_won ? "true" : "false") + ", server_won=" + (resp.server_won ? "true" : "false"));
	return resp;
}