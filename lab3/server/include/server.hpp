
#pragma once

#include <sys/types.h>

#include <TCPServer.hpp>
#include <compilation_shared_data.hpp>
#include <cstdint>
#include <semaphore.hpp>
#include <shared_memory.hpp>
#include <vector>

#include "logger.hpp"

static constexpr auto SHM_NAME = "/compile_shm";
static constexpr auto SEM_REQ_NAME = "/sem_req";
static constexpr auto SEM_RESP_NAME = "/sem_resp";
static constexpr uint16_t SERVER_PORT = 5555;

void handle_client(int client_fd, Logger& logger);