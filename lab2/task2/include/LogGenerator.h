// LogGenerator.h
#pragma once

#include <atomic>
#include <memory> // Для std::shared_ptr
#include <thread>
#include <vector>
#include <random>
#include <netinet/in.h> // Для in_addr_t

#include "LogEvent.h"
#include "Logger.h"     // Используется для записи логов самим генератором
#include "SafeQueue.h"  // Очередь для передачи LogEvent

class LogGenerator {
   public:
    // Конструктор
    LogGenerator(int id,                                          // Уникальный ID для логов
                 std::shared_ptr<SafeQueue<LogEvent>> queue,       // Общая очередь
                 std::shared_ptr<Logger> logger,                  // Логгер для сообщений генератора
                 std::vector<in_addr_t> source_ips,               // Пул IP-адресов для генерации
                 long events_to_generate = -1);                   // Кол-во событий (-1 = бесконечно)

    // Деструктор (должен корректно останавливать поток)
    ~LogGenerator();

    // Запуск генерации в отдельном потоке
    void start();

    // Остановка генерации (сигнал потоку + ожидание завершения)
    void stop();

    // Запретим копирование и перемещение, чтобы избежать проблем с потоком
    LogGenerator(const LogGenerator&) = delete;
    LogGenerator& operator=(const LogGenerator&) = delete;
    LogGenerator(LogGenerator&&) = delete;
    LogGenerator& operator=(LogGenerator&&) = delete;

   private:
    // Основная функция потока генератора
    void run();
    // Функция генерации одного случайного события
    LogEvent generate_random_event();

    int id_;
    std::shared_ptr<SafeQueue<LogEvent>> event_queue_;
    std::shared_ptr<Logger> logger_;
    std::vector<in_addr_t> source_ips_;
    long events_to_generate_;
    long events_generated_ = 0;

    std::thread worker_thread_;
    std::atomic<bool> stop_flag_{false}; // Флаг для сигнала остановки

    // Генераторы случайных чисел (инициализируются в конструкторе)
    std::mt19937 random_engine_;
    std::uniform_int_distribution<int> source_ip_index_dist_;
    std::uniform_int_distribution<uint32_t> random_ip_dist_;
    std::uniform_int_distribution<uint16_t> port_dist_;
    std::uniform_int_distribution<int> event_type_dist_;
    std::uniform_int_distribution<size_t> data_size_dist_;

    // Приватные хелперы для случайных значений
    in_addr_t get_random_source_ip();
    in_addr_t get_random_dest_ip();
    in_port_t get_random_port();
    size_t get_random_data_size();
    LogEvent::EventType get_random_event_type();
};