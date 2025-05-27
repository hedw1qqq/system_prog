#include "../include/sticks_game.hpp"
#include "logger.hpp"
#include "server_message_queue.hpp"
#include <signal.h>
#include <atomic>
#include <memory>

static Logger app_logger = Logger::Builder()
        .set_log_level(LogLevel::INFO)
        .add_console_handler()
        .add_file_handler("sticks_game.log")
        .build();

static std::atomic<bool> sticks_running_flag(true);

void sticks_signal_handler_set_flag_only(int signum) {
    (void) signum;

    sticks_running_flag.store(false);
}

int main() {
    struct sigaction sa;
    sa.sa_handler = sticks_signal_handler_set_flag_only;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    if (sigaction(SIGINT, &sa, NULL) == -1 || sigaction(SIGTERM, &sa, NULL) == -1) {
        app_logger.error(
            "Sticks game: Failed to set signal handlers.");
        return 1;
    }

    std::unique_ptr<ServerMessageQueue> mq_obj;
    try {
        mq_obj = std::make_unique<ServerMessageQueue>(app_logger);
    } catch (const std::exception &e) {
        app_logger.error("Failed to create ServerMessageQueue: " + std::string(e.what()));

        return 1;
    }

    app_logger.info("Sticks-game subserver started");
    try {
        run_sticks_game(app_logger, sticks_running_flag, *mq_obj);
    } catch (const MessageQueueException &e) {
        if (!sticks_running_flag.load()) {
            app_logger.info("Sticks game: run_sticks_game interrupted by signal, as expected.");
        } else {
            app_logger.error("Sticks game: MessageQueueException from run_sticks_game: " + std::string(e.what()));
        }
    } catch (const std::exception &e) {
        app_logger.error("Sticks game: run_sticks_game loop exited with exception: " + std::string(e.what()));
    } catch (...) {
        app_logger.error("Sticks game: run_sticks_game loop exited with unknown exception.");
    }

    app_logger.info("Sticks-game subserver loop finished. Cleaning up message queue...");
    if (mq_obj) {
        mq_obj->remove_queue();
    }


    app_logger.info("Sticks-game subserver shut down.");
    return 0;
}
