
#include "compiler.hpp"

#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <system_error>
#include <thread>
#include <vector>

namespace fs = std::filesystem;

void run_compiler(const std::string& shm_name, const std::string& sem_req_name, const std::string& sem_resp_name,
                  Logger& logger, std::atomic<bool>& running_flag) {
	SharedMemory shm(shm_name, sizeof(CompilationSharedData), false, logger);
	Semaphore sem_req(sem_req_name, 0, logger);
	Semaphore sem_resp(sem_resp_name, 0, logger);

	auto data = reinterpret_cast<CompilationSharedData*>(shm.data());

	logger.info("Compilation subserver started and connected to IPC.");
	logger.info("Waiting for compilation requests...");

	std::string cpp_script_path = "./compile_cpp.sh";
	std::string tex_script_path = "./compile_tex.sh";

	if (!fs::exists(cpp_script_path)) {
		logger.warning("compile_cpp.sh not found at " + cpp_script_path +
		               ". Ensure it's in the working directory or PATH.");
	}
	if (!fs::exists(tex_script_path)) {
		logger.warning("compile_tex.sh not found at " + tex_script_path +
		               ". Ensure it's in the working directory or PATH.");
	}

	while (running_flag.load()) {
		logger.debug("Compiler: Waiting for request semaphore (sem_req)...");
		try {
			sem_req.wait();
		} catch (const SemaphoreException& e) {
			if (!running_flag.load()) {
				logger.info("Compiler: sem_req.wait() interrupted by shutdown signal.");
				break;
			}
			logger.error("Compiler: SemaphoreException on sem_req.wait(): " + std::string(e.what()));
			std::this_thread::sleep_for(std::chrono::seconds(1));
			continue;
		}

		if (!running_flag.load()) {
			logger.info("Compiler: Shutdown signal received after sem_req.wait().");
			break;
		}

		logger.debug("Compiler: Request semaphore acquired. Processing request.");

		if (data->status != CompileStatus::PENDING) {
			logger.warning("Compiler: Received request with non-PENDING status (" +
			               std::to_string(static_cast<int>(data->status)) + "). Skipping and signaling response.");
			sem_resp.post();
			continue;
		}

		std::string filename(data->file_name);
		size_t file_size = data->file_size;
		if (file_size > MAX_FILE_SIZE) {
			logger.error("Compiler: Input file '" + filename + "' is too large (" + std::to_string(file_size) +
			             " bytes). Max allowed: " + std::to_string(MAX_FILE_SIZE) + " bytes.");
			data->status = CompileStatus::FAILURE;
			sem_resp.post();
			continue;
		}
		std::vector<uint8_t> file_buffer(data->file_data, data->file_data + file_size);

		logger.info("Compiler: Processing file: '" + filename + "', size: " + std::to_string(file_size) + " bytes.");

		fs::path source_file_path_obj(filename);
		std::string temp_input_filename = "compiler_input_" + source_file_path_obj.filename().string();
		fs::path temp_input_path = fs::temp_directory_path() / temp_input_filename;

		std::ofstream temp_ofs(temp_input_path, std::ios::binary | std::ios::trunc);
		if (!temp_ofs) {
			logger.error("Compiler: Failed to create/open temporary input file: " + temp_input_path.string());
			data->status = CompileStatus::FAILURE;
			sem_resp.post();
			continue;
		}
		temp_ofs.write(reinterpret_cast<const char*>(file_buffer.data()), file_size);
		temp_ofs.close();

		std::string output_filename_stem = "compiled_" + source_file_path_obj.stem().string();
		fs::path temp_output_path;
		std::string command_to_execute;

		if (source_file_path_obj.extension() == ".cpp") {
			temp_output_path = fs::temp_directory_path() / output_filename_stem;
			command_to_execute =
			    cpp_script_path + " \"" + temp_input_path.string() + "\" \"" + temp_output_path.string() + "\"";
		} else if (source_file_path_obj.extension() == ".tex") {
			temp_output_path = fs::temp_directory_path() / (output_filename_stem + ".pdf");
			command_to_execute =
			    tex_script_path + " \"" + temp_input_path.string() + "\" \"" + temp_output_path.string() + "\"";
		} else {
			logger.error("Compiler: Unsupported file extension: '" + source_file_path_obj.extension().string() +
			             "' for file '" + filename + "'.");
			data->status = CompileStatus::FAILURE;
			sem_resp.post();
			if (fs::exists(temp_input_path)) fs::remove(temp_input_path);
			continue;
		}

		logger.debug("Compiler: Executing command: " + command_to_execute);
		int system_ret_code = system(command_to_execute.c_str());

		if (system_ret_code == 0 && fs::exists(temp_output_path)) {
			logger.info("Compiler: Command executed successfully for '" + filename +
			            "'. Output file: " + temp_output_path.string());

			std::uintmax_t result_fs_size_uintmax = fs::file_size(temp_output_path);
			if (result_fs_size_uintmax > MAX_RESULT_SIZE) {
				logger.error("Compiler: Compiled result file '" + temp_output_path.string() + "' is too large (" +
				             std::to_string(result_fs_size_uintmax) +
				             " bytes). Max allowed: " + std::to_string(MAX_RESULT_SIZE) + " bytes.");
				data->status = CompileStatus::FAILURE;
			} else {
				size_t result_file_size = static_cast<size_t>(result_fs_size_uintmax);
				std::ifstream result_ifs(temp_output_path, std::ios::binary);
				if (!result_ifs) {
					logger.error("Compiler: Failed to open compiled result file '" + temp_output_path.string() +
					             "' for reading, though it exists.");
					data->status = CompileStatus::FAILURE;
				} else {
					std::vector<uint8_t> result_buffer(result_file_size);
					if (!result_ifs.read(reinterpret_cast<char*>(result_buffer.data()), result_file_size)) {
						logger.error("Compiler: Failed to read content from compiled result file '" +
						             temp_output_path.string() + "'.");
						data->status = CompileStatus::FAILURE;
					} else {
						data->result_size = static_cast<uint32_t>(result_buffer.size());
						std::memcpy(data->result_data, result_buffer.data(), result_buffer.size());
						data->status = CompileStatus::SUCCESS;
						logger.info("Compiler: Compilation successful for '" + filename +
						            "'. Result size: " + std::to_string(data->result_size) + " bytes.");
					}
					result_ifs.close();
				}
			}
		} else {
			logger.error("Compiler: Compilation command failed for '" + filename + "'. System return code: " +
			             std::to_string(system_ret_code) + ". Expected output: '" + temp_output_path.string() +
			             (fs::exists(temp_output_path) ? "' (exists, but ret_code!=0 or other issue)"
			                                           : "' (does NOT exist or ret_code!=0)"));
			data->status = CompileStatus::FAILURE;
		}

		logger.debug("Compiler: Posting response semaphore (sem_resp).");
		sem_resp.post();

		logger.debug("Compiler: Cleaning up temporary files...");
		if (fs::exists(temp_input_path)) {
			std::error_code ec;
			fs::remove(temp_input_path, ec);
			if (ec)
				logger.warning("Compiler: Failed to remove temp input file " + temp_input_path.string() + ": " +
				               ec.message());
		}
		if (fs::exists(temp_output_path)) {
			std::error_code ec;
			fs::remove(temp_output_path, ec);
			if (ec)
				logger.warning("Compiler: Failed to remove temp output file " + temp_output_path.string() + ": " +
				               ec.message());
		}

		logger.debug("Compiler: Temporary files cleaned up. Waiting for next request.");
	}

	logger.info("Compilation subserver processing loop finished. Shutting down.");
}