
#include "../include/sticks_game.hpp"

#include <algorithm>
#include <map>
#include <random>
#include <string>
#include <thread>
void run_sticks_game(Logger& logger, std::atomic<bool>& running_flag, ServerMessageQueue& mq) {
	std::map<long, int> game_sessions_state;
	std::random_device rd;
	std::mt19937 gen(rd());

	const int START_STICKS = 21;
	const int MAX_PLAYER_TAKE = 3;

	logger.info("Sticks game logic loop started. Waiting for requests...");

	while (running_flag.load()) {
		GameRequest req;
		try {
			req = mq.receive_request();
		} catch (const MessageQueueException& e) {
			if (!running_flag.load()) {
				logger.info("Sticks game: receive_request() interrupted by shutdown signal.");
				break;
			}
			logger.error("Sticks game: MessageQueueException while receiving request: " + std::string(e.what()));
			std::this_thread::sleep_for(std::chrono::milliseconds(100));
			continue;
		}

		if (!running_flag.load()) {
			break;
		}

		long current_session_id = req.session_id;
		int client_take = req.take;
		logger.debug("Game request from session_id=" + std::to_string(current_session_id) + ", client takes " +
		             std::to_string(client_take) + " sticks.");

		if (game_sessions_state.find(current_session_id) == game_sessions_state.end()) {
			logger.info("New game started for session_id=" + std::to_string(current_session_id) +
			            ". Initial sticks: " + std::to_string(START_STICKS));
			game_sessions_state[current_session_id] = START_STICKS;
		}

		int& remaining_sticks = game_sessions_state[current_session_id];

		if (client_take < 1 || client_take > MAX_PLAYER_TAKE) {
			logger.warning("Session_id=" + std::to_string(current_session_id) +
			               " tried to take invalid number of sticks: " + std::to_string(client_take) +
			               ". Server takes 0, game continues.");
			GameResponse error_resp{};
			error_resp.mtype = current_session_id;
			error_resp.taken = 0;
			error_resp.client_won = false;
			error_resp.server_won = false;
			error_resp.remaining_sticks = remaining_sticks;
			mq.send_response(error_resp);
			continue;
		}

		if (client_take > remaining_sticks) {
			logger.warning("Session_id=" + std::to_string(current_session_id) + " tried to take " +
			               std::to_string(client_take) + " but only " + std::to_string(remaining_sticks) +
			               " remaining. Client takes all " + std::to_string(remaining_sticks) + ".");
			client_take = remaining_sticks;
		}

		remaining_sticks -= client_take;
		logger.debug("Session_id=" + std::to_string(current_session_id) + " took " + std::to_string(client_take) +
		             " sticks. Remaining: " + std::to_string(remaining_sticks));

		GameResponse resp{};
		resp.mtype = current_session_id;

		if (remaining_sticks <= 0) {
			resp.taken = 0;
			resp.client_won = true;
			resp.server_won = false;
			resp.remaining_sticks = 0;
			game_sessions_state.erase(current_session_id);
			logger.info("Session_id=" + std::to_string(current_session_id) + " wins! Game state cleared.");
		} else {
			std::uniform_int_distribution<> dist(1, std::min(MAX_PLAYER_TAKE, remaining_sticks));
			int server_take = dist(gen);
			remaining_sticks -= server_take;
			resp.taken = server_take;

			logger.debug("Server takes " + std::to_string(server_take) + " sticks for session_id=" +
			             std::to_string(current_session_id) + ". Remaining: " + std::to_string(remaining_sticks));

			if (remaining_sticks <= 0) {
				resp.client_won = false;
				resp.server_won = true;

				resp.remaining_sticks = remaining_sticks;
				game_sessions_state.erase(current_session_id);
				logger.info("Server wins against session_id=" + std::to_string(current_session_id) +
				            "! Game state cleared.");
			} else {
				resp.client_won = false;
				resp.server_won = false;

				resp.remaining_sticks = remaining_sticks;
			}
		}
		mq.send_response(resp);
		logger.debug("Sent game response to session_id=" + std::to_string(current_session_id));
	}
	logger.info("Sticks game logic loop finished.");
}