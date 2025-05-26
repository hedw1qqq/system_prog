#include "client.hpp"

#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>

namespace client {

ClientApp::ClientApp(const std::string& host, uint16_t port, Logger& logger)
    : host_(host), port_(port), logger_(logger) {}

TCPClient ClientApp::make_connection() {
	TCPClient conn(host_, port_, logger_);
	conn.connect();
	return conn;
}

void ClientApp::compile(const std::string& path_str) {
	namespace fs = std::filesystem;

	fs::path original_path(path_str);

	if (!fs::exists(original_path) || !fs::is_regular_file(original_path)) {
		logger_.error("File does not exist or is not a regular file: " + path_str);
		std::cerr << "Error: File does not exist or is not a regular file: " << path_str << std::endl;
		return;
	}

	auto conn = make_connection();

	conn.send(std::vector<uint8_t>{'C', 'O', 'M', 'P', 'I', 'L', 'E'});

	std::string filename_only = original_path.filename().string();
	conn.send(std::vector<uint8_t>(filename_only.begin(), filename_only.end()));

	std::ifstream ifs(original_path, std::ios::binary);
	if (!ifs) {
		logger_.error("Failed to open file: " + path_str);
		std::cerr << "Error: Failed to open file: " << path_str << std::endl;
		return;
	}
	std::vector<uint8_t> buf((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
	ifs.close();
	conn.send(buf);

	auto result_data = conn.receive();
	std::string maybe_err_str(result_data.begin(), result_data.end());

	if (maybe_err_str == "COMPILATION_FAILED") {
		std::cerr << "Server: compilation failed for " << filename_only << "\n";
		logger_.error("Server reported compilation failure for " + filename_only);
	} else {
		std::string output_filename_stem = "out_" + original_path.stem().string();
		std::string output_full_filename;

		if (original_path.extension() == ".cpp") {
			output_full_filename = output_filename_stem;
		} else if (original_path.extension() == ".tex") {
			output_full_filename = output_filename_stem + ".pdf";
		} else {
			logger_.warning("Unknown original file type for output naming: " + original_path.extension().string() +
			                ". Saving with default naming scheme.");
			output_full_filename = "out_" + filename_only;
		}

		std::ofstream ofs(output_full_filename, std::ios::binary);
		if (!ofs) {
			logger_.error("Failed to create output file: " + output_full_filename);
			std::cerr << "Error: Failed to create output file: " << output_full_filename << std::endl;
		} else {
			ofs.write(reinterpret_cast<const char*>(result_data.data()), result_data.size());
			ofs.close();
			std::cout << "Received compiled file: " << output_full_filename << "\n";
			logger_.info("Received compiled file: " + output_full_filename +
			             " (size: " + std::to_string(result_data.size()) + " bytes)");
		}
	}

	conn.close();
}

void ClientApp::play() {
	auto conn = make_connection();
	int current_sticks_on_table = 21;
	conn.send(std::vector<uint8_t>{'P', 'L', 'A', 'Y'});

	while (true) {
		int take = 0;
		std::cout << "Enter number of sticks to take (1-3): ";
		std::cin >> take;
		if (take < 1 || take > 3) {
			std::cout << "Invalid number of sticks. Please take 1, 2, or 3." << std::endl;
			continue;
		}
		conn.send(
		    std::vector<uint8_t>(reinterpret_cast<uint8_t*>(&take), reinterpret_cast<uint8_t*>(&take) + sizeof(int)));

		auto resp_data = conn.receive();
		int server_take;
		uint8_t client_won_byte;
		uint8_t server_won_byte;
		int sticks_after_server_turn;

		size_t offset = 0;
		std::memcpy(&server_take, resp_data.data() + offset, sizeof(int));
		offset += sizeof(int);
		client_won_byte = resp_data[offset++];
		server_won_byte = resp_data[offset++];
		std::memcpy(&sticks_after_server_turn, resp_data.data() + offset, sizeof(int));

		bool client_won = static_cast<bool>(client_won_byte);
		bool server_won = static_cast<bool>(server_won_byte);

		current_sticks_on_table = sticks_after_server_turn;

		if (server_take > 0) {
			std::cout << "Server took " << server_take << " sticks." << std::endl;
		}
		std::cout << "Sticks remaining: " << current_sticks_on_table << std::endl;

		if (client_won) {
			std::cout << "You win!" << std::endl;
			logger_.info("Game ended: client won. Sticks left: " + std::to_string(current_sticks_on_table));
			break;
		}
		if (server_won) {
			std::cout << "Server wins!" << std::endl;
			logger_.info("Game ended: server won. Sticks left: " + std::to_string(current_sticks_on_table));
			break;
		}
	}

	conn.close();
	logger_.info("Sticks game finished or quit.");
}

}  // namespace client
