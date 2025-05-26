#include "client.hpp"
#include "logger.hpp"

static Logger app_logger = Logger::Builder().set_log_level(LogLevel::DEBUG).add_file_handler("client.log").build();

int main() {
	client::ClientApp app("127.0.0.1", 5555, app_logger);

	while (true) {
		std::cout << "\n=== MENU ===\n"
		          << "1) Compile file\n"
		          << "2) Play sticks game\n"
		          << "3) Exit\n"
		          << "Select option: ";
		int opt;
		if (!(std::cin >> opt)) {
			break;
		}

		switch (opt) {
			case 1: {
				std::cout << "Enter path to .cpp/.tex: ";
				std::string path;
				std::cin >> path;
				app.compile(path);
				break;
			}
			case 2:
				app.play();
				break;
			case 3:
				std::cout << "Goodbye!\n";
				return 0;
			default:
				std::cout << "Unknown option\n";
		}
	}
	return 0;
}
