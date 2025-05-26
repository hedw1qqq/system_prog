
#pragma once

#include <atomic>

#include "game_message.hpp"
#include "logger.hpp"
#include "server_message_queue.hpp"

void run_sticks_game(Logger& logger, std::atomic<bool>& running_flag, ServerMessageQueue& mq);