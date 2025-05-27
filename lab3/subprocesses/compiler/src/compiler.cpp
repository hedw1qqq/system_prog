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

void run_compiler(const std::string &shm_name, const std::string &sem_req_name, const std::string &sem_resp_name,
                  Logger &logger, std::atomic<bool> &running_flag) {
    SharedMemory shm(shm_name, sizeof(CompilationSharedData), false, logger);
    Semaphore sem_req(sem_req_name, 0, logger);
    Semaphore sem_resp(sem_resp_name, 0, logger);

    auto data = reinterpret_cast<CompilationSharedData *>(shm.data());

    logger.info("Compilation subserver started and connected to IPC.");
    logger.info("Waiting for compilation requests...");

    std::string cpp_script_path = "./compile_cpp.sh";
    std::string tex_script_path = "./compile_tex.sh";

    if (!fs::exists(cpp_script_path)) {
        logger.warning("compile_cpp.sh not found at " + cpp_script_path);
    }
    if (!fs::exists(tex_script_path)) {
        logger.warning("compile_tex.sh not found at " + tex_script_path);
    }

    while (running_flag.load()) {
        logger.debug("Compiler: Waiting for request semaphore (sem_req)...");
        try {
            sem_req.wait();
        } catch (const SemaphoreException &e) {
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
                           std::to_string(static_cast<int>(data->status)) + ")");
            sem_resp.post();
            continue;
        }

        std::string filename_from_shm(data->file_name);
        size_t file_size = data->file_size;
        if (file_size > MAX_FILE_SIZE) {
            logger.error("Compiler: Input file '" + filename_from_shm + "' is too large (" + std::to_string(file_size) +
                         " bytes). Max allowed: " + std::to_string(MAX_FILE_SIZE) + " bytes.");
            data->status = CompileStatus::FAILURE;
            sem_resp.post();
            continue;
        }
        std::vector<uint8_t> file_buffer(data->file_data, data->file_data + file_size);

        logger.info(
            "Compiler: Processing file: '" + filename_from_shm + "', size: " + std::to_string(file_size) + " bytes.");

        fs::path source_file_original_path(filename_from_shm);

        std::string temp_input_filename = "compiler_input_" + source_file_original_path.filename().string();
        fs::path temp_input_full_path = fs::temp_directory_path() / temp_input_filename;

        std::ofstream temp_ofs(temp_input_full_path, std::ios::binary | std::ios::trunc);
        if (!temp_ofs) {
            logger.error("Compiler: Failed to create/open temporary input file: " + temp_input_full_path.string());
            data->status = CompileStatus::FAILURE;
            sem_resp.post();
            continue;
        }
        temp_ofs.write(reinterpret_cast<const char *>(file_buffer.data()), file_size);
        temp_ofs.close();


        std::string output_filename_stem = "compiled_" + source_file_original_path.stem().string();

        fs::path final_output_path_to_check;
        std::string command_to_execute;

        if (source_file_original_path.extension() == ".cpp") {
            final_output_path_to_check = fs::temp_directory_path() / output_filename_stem;

            command_to_execute =
                    cpp_script_path + " \"" + temp_input_full_path.string() + "\" \"" + final_output_path_to_check.
                    string() + "\"";
        } else if (source_file_original_path.extension() == ".tex") {
            fs::path tex_output_base_path = fs::temp_directory_path() / output_filename_stem;
            final_output_path_to_check = tex_output_base_path.string() + ".pdf";

            command_to_execute =
                    tex_script_path + " \"" + temp_input_full_path.string() + "\" \"" + tex_output_base_path.string() +
                    "\"";
        } else {
            logger.error("Compiler: Unsupported file extension: '" + source_file_original_path.extension().string() +
                         "' for file '" + filename_from_shm + "'.");
            data->status = CompileStatus::FAILURE;
            sem_resp.post();
            if (fs::exists(temp_input_full_path)) {
                fs::remove(temp_input_full_path);
            }
            continue;
        }

        logger.debug("Compiler: Executing command: " + command_to_execute);
        int system_ret_code = system(command_to_execute.c_str());


        if (system_ret_code == 0 && fs::exists(final_output_path_to_check)) {
            logger.info("Compiler: Command executed successfully for '" + filename_from_shm +
                        "'. Output file: " + final_output_path_to_check.string());

            std::uintmax_t result_fs_size_uintmax = fs::file_size(final_output_path_to_check);
            if (result_fs_size_uintmax > MAX_RESULT_SIZE) {
                logger.error(
                    "Compiler: Compiled result file '" + final_output_path_to_check.string() + "' is too large (" +
                    std::to_string(result_fs_size_uintmax) +
                    " bytes). Max allowed: " + std::to_string(MAX_RESULT_SIZE) + " bytes.");
                data->status = CompileStatus::FAILURE;
            } else {
                size_t result_file_size = static_cast<size_t>(result_fs_size_uintmax);
                std::ifstream result_ifs(final_output_path_to_check, std::ios::binary);
                if (!result_ifs) {
                    logger.error(
                        "Compiler: Failed to open compiled result file '" + final_output_path_to_check.string());
                    data->status = CompileStatus::FAILURE;
                } else {
                    std::vector<uint8_t> result_buffer(result_file_size);
                    if (!result_ifs.read(reinterpret_cast<char *>(result_buffer.data()), result_file_size)) {
                        logger.error("Compiler: Failed to read content from compiled result file '" +
                                     final_output_path_to_check.string() + "'.");
                        data->status = CompileStatus::FAILURE;
                    } else {
                        data->result_size = static_cast<uint32_t>(result_buffer.size());
                        std::memcpy(data->result_data, result_buffer.data(), result_buffer.size());
                        data->status = CompileStatus::SUCCESS;
                        logger.info("Compiler: Compilation successful for '" + filename_from_shm +
                                    "'. Result size: " + std::to_string(data->result_size) + " bytes.");
                    }
                    result_ifs.close();
                }
            }
        } else {
            logger.error("Compiler: Compilation command failed for '" + filename_from_shm + "'. System return code: " +
                         std::to_string(system_ret_code) + ". Expected output: '" + final_output_path_to_check.string());
            data->status = CompileStatus::FAILURE;
        }

        logger.debug("Compiler: Posting response semaphore (sem_resp).");
        sem_resp.post();

        logger.debug("Compiler: Cleaning up temporary files...");
        if (fs::exists(temp_input_full_path)) {
            std::error_code ec;
            fs::remove(temp_input_full_path, ec);
            if (ec)
                logger.warning("Compiler: Failed to remove temp input file " + temp_input_full_path.string() + ": " +
                               ec.message());
        }
        if (fs::exists(final_output_path_to_check)) {
            std::error_code ec;
            fs::remove(final_output_path_to_check, ec);
            if (ec)
                logger.warning(
                    "Compiler: Failed to remove final output file " + final_output_path_to_check.string() + ": " +
                    ec.message());
        }
        logger.debug("Compiler: Temporary files cleaned up. Waiting for next request.");
    }

    logger.info("Compilation subserver processing loop finished. Shutting down.");
}
