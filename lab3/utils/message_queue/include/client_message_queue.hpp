#pragma once

#include "custom_exceptions.hpp"
#include "game_message.hpp"
#include "logger.hpp"

class ClientMessageQueue {
   public:
	explicit ClientMessageQueue(Logger& logger);
	~ClientMessageQueue();

	void send_request(long session_id, int take);

	GameResponse receive_response(long session_id);

   private:
	int msqid_;
	Logger& logger_;
};