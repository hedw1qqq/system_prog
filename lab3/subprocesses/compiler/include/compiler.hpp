#pragma once

#include <string>
#include <vector>

#include "../../shared_memory/include/compilation_shared_data.hpp"
#include "logger.hpp"
#include "semaphore.hpp"
#include "shared_memory.hpp"

void run_compiler(const std::string& shm_name, const std::string& sem_req_name, const std::string& sem_resp_name,
                  Logger& logger, std::atomic<bool>& running_flag);