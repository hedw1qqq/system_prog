#pragma once

#include <cstddef>
#include <cstdint>

constexpr size_t MAX_FILE_NAME = 256;
constexpr size_t MAX_FILE_SIZE = 10 * 1024 * 1024;
constexpr size_t MAX_RESULT_SIZE = 10 * 1024 * 1024;

enum class CompileStatus : int32_t { NONE, PENDING, SUCCESS, FAILURE };

struct CompilationSharedData {
	CompileStatus status;
	char file_name[MAX_FILE_NAME];
	uint32_t file_size;
	uint8_t file_data[MAX_FILE_SIZE];
	uint32_t result_size;
	uint8_t result_data[MAX_RESULT_SIZE];
};
