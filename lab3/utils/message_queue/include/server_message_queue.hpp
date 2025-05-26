#pragma once

#include "custom_exceptions.hpp"
#include "game_message.hpp"
#include "logger.hpp"

class ServerMessageQueue {
   public:
	explicit ServerMessageQueue(Logger& logger);
	~ServerMessageQueue();

	GameRequest receive_request();

	void send_response(const GameResponse& resp);

	void remove_queue();

   private:
	int msqid_;
	Logger& logger_;
};