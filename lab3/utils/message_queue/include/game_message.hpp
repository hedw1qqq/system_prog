#pragma once

#include <sys/types.h>

constexpr key_t GAME_MSG_KEY = 0x1234;

constexpr long REQUEST_TAG = 1L;

struct GameRequest {
	long mtype;
	long session_id;
	int take;
};

struct GameResponse {
	long mtype;
	int taken;
	bool client_won;
	bool server_won;
	int remaining_sticks;
};