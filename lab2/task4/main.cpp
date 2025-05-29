#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <thread>
#include <vector>

#include "include/Logger.hpp"

const std::chrono::milliseconds TICK_DURATION(100);
const size_t MOVE_TICKS_PER_FLOOR = 2;
const size_t DOOR_OPEN_TICKS = 1;
const size_t BOARDING_TICKS = 1;
const size_t DOOR_CLOSE_TICKS = 1;

struct Person {
  std::chrono::system_clock::time_point arrival_time;
  size_t location;
  size_t destination;
  size_t num_people;
  size_t id;
};

class Elevator {
   public:
  enum class Direction { UP, DOWN, IDLE };
  enum class Status { WAITING, MOVING, OPENING_DOORS, BOARDING_DEBOARDING, CLOSING_DOORS };
  explicit Elevator(Logger *logger, size_t id, size_t capacity)
      : logger_(logger),
        id_(id),
        capacity_(capacity),
        current_floor_(1),
        current_load_(0),
        status_(Status::WAITING),
        current_direction_(Direction::IDLE),
        passengers_transported_count_(0),
        floors_covered_count_(0) {}

  void start(std::deque<Person> *requests_queue, std::mutex *queue_mtx, std::condition_variable *queue_cv,
             std::atomic<bool> *stop_flag, std::mutex *stats_mtx, double *total_wait_time_sec,
             size_t *total_boarded_pax_groups) {
    requests_queue_ = requests_queue;
    queue_mtx_ = queue_mtx;
    queue_cv_ = queue_cv;
    stop_simulation_ = stop_flag;
    global_stats_mtx_ = stats_mtx;
    total_wait_time_seconds_accumulator_ = total_wait_time_sec;
    total_boarded_groups_accumulator_ = total_boarded_pax_groups;

    worker_thread_ = std::thread(&Elevator::run_logic, this);
  }

  void join() {
    if (worker_thread_.joinable()) {
      worker_thread_.join();
    }
  }

  void print_stats() const {
    std::lock_guard<std::mutex> lk(elevator_internal_mtx_);
    std::cout << "Elev " << id_ << " (Cap:" << capacity_ << " Load:" << current_load_
              << "): " << passengers_transported_count_ << "ppl, " << floors_covered_count_ << "flrs. At F"
              << current_floor_ << " St:" << status_to_string(status_) << "\n";
  }

  size_t get_passengers_transported() const {
    std::lock_guard<std::mutex> lk(elevator_internal_mtx_);
    return passengers_transported_count_;
  }

  size_t get_floors_covered() const {
    std::lock_guard<std::mutex> lk(elevator_internal_mtx_);
    return floors_covered_count_;
  }

   private:
  std::string status_to_string(Status s) const {
    if (s == Status::WAITING) return "WAIT";
    if (s == Status::MOVING) return "MOVE";
    if (s == Status::OPENING_DOORS) return "OPEN_DR";
    if (s == Status::BOARDING_DEBOARDING) return "BOARD";
    if (s == Status::CLOSING_DOORS) return "CLOSE_DR";
    return "UNK";
  }
  void run_logic() {
    logger_->info("Elev " + std::to_string(id_) + " started.");

    while (true) {
      if (stop_simulation_->load()) {
        std::lock_guard<std::mutex> guard(elevator_internal_mtx_);
        if (passengers_inside_.empty() && pending_pickups_.empty()) {
          break;
        }
      }

      std::unique_lock<std::mutex> elevator_lk(elevator_internal_mtx_);

      bool did_something = false;
      bool stop_here = should_stop_at_floor(current_floor_);


      if (status_ == Status::WAITING || status_ == Status::MOVING ||
          (status_ == Status::CLOSING_DOORS && remaining_action_ticks_ == 0)) {
        if (stop_here) {
          if (status_ == Status::MOVING) {
            logger_->info("Elev " + std::to_string(id_) + " arrived F" + std::to_string(current_floor_) +
                          " for service.");
          }
          status_ = Status::OPENING_DOORS;
          remaining_action_ticks_ = DOOR_OPEN_TICKS;
          did_something = true;
        } else if (!passengers_inside_.empty() || !pending_pickups_.empty()) {
          size_t next_target = determine_next_target_floor_();
          if (next_target != current_floor_) {
            move_towards(next_target);
            did_something = true;
          } else {
            status_ = Status::WAITING;
            did_something = true;
          }
        } else {
          Person new_call;
          bool found_call = false;
          {
            std::lock_guard<std::mutex> qlk(*queue_mtx_);
            if (!requests_queue_->empty()) {
              for (size_t i = 0; i < requests_queue_->size(); ++i) {
                Person &p_in_q = (*requests_queue_)[i];
                if (capacity_ - current_load_ >= p_in_q.num_people) {
                  new_call = p_in_q;
                  requests_queue_->erase(requests_queue_->begin() + i);
                  found_call = true;
                  break;
                }
              }
            }
          }

          if (found_call) {
            logger_->info("Elev " + std::to_string(id_) + " took new call P" + std::to_string(new_call.id) +
                          " (" + std::to_string(new_call.num_people) + "ppl) from F" +
                          std::to_string(new_call.location) + " to F" +
                          std::to_string(new_call.destination));
            pending_pickups_.push_back(new_call);
            if (current_direction_ == Direction::IDLE) {
              if (new_call.location != current_floor_) {
                current_direction_ =
                    (new_call.location > current_floor_) ? Direction::UP : Direction::DOWN;
              } else {
                current_direction_ =
                    (new_call.destination > new_call.location) ? Direction::UP : Direction::DOWN;
              }
            }
            did_something = true;
          } else {
            status_ = Status::WAITING;
            current_direction_ = Direction::IDLE;
          }
        }
      }

      elevator_lk.unlock();

      if (remaining_action_ticks_ > 0) {
        std::this_thread::sleep_for(TICK_DURATION);
        remaining_action_ticks_--;
        did_something = true;


        elevator_lk.lock();
        if (remaining_action_ticks_ == 0) {
          if (status_ == Status::OPENING_DOORS) {
            logger_->info("Elev " + std::to_string(id_) + " doors open F" + std::to_string(current_floor_));
            status_ = Status::BOARDING_DEBOARDING;
            remaining_action_ticks_ = process_boarding_deboarding_();
          } else if (status_ == Status::BOARDING_DEBOARDING) {
            logger_->info("Elev " + std::to_string(id_) + " board/deboard done F" +
                          std::to_string(current_floor_));
            status_ = Status::CLOSING_DOORS;
            remaining_action_ticks_ = DOOR_CLOSE_TICKS;
          } else if (status_ == Status::CLOSING_DOORS) {
            logger_->info("Elev " + std::to_string(id_) + " doors closed F" +
                          std::to_string(current_floor_));
            status_ = Status::WAITING;
          }
        }
        elevator_lk.unlock();
      } else if (!did_something) {
        std::unique_lock<std::mutex> qlk_wait(*queue_mtx_);
        if (stop_simulation_->load() && requests_queue_->empty() && passengers_inside_.empty() &&
            pending_pickups_.empty()) {
          break;
        }
        queue_cv_->wait_for(qlk_wait, TICK_DURATION * 3,
                            [&] { return !requests_queue_->empty() || stop_simulation_->load(); });
      }
    }
    logger_->info("Elev " + std::to_string(id_) + " stopped. Load: " + std::to_string(current_load_));
  }

  void move_towards(size_t target_floor) {
    if (target_floor == current_floor_) {
      return;
    }

    status_ = Status::MOVING;
    current_direction_ = (target_floor > current_floor_) ? Direction::UP : Direction::DOWN;

    if (current_direction_ == Direction::UP)
      current_floor_++;
    else
      current_floor_--;

    logger_->info("Elev " + std::to_string(id_) + " now at F" + std::to_string(current_floor_) + " (heading " +
                  (current_direction_ == Direction::UP ? "UP" : "DOWN") + ")");

    floors_covered_count_++;
    remaining_action_ticks_ = MOVE_TICKS_PER_FLOOR;
  }

  bool should_stop_at_floor(size_t floor_to_check) {
    for (const auto &group : passengers_inside_) {
      if (group.destination == floor_to_check) {
        return true;
      }
    }
    for (const auto &group : pending_pickups_) {
      if (group.location == floor_to_check) {
        return true;
      }
    }
    if (capacity_ - current_load_ > 0) {
      std::lock_guard<std::mutex> qlk(*queue_mtx_);
      for (const auto &group : *requests_queue_) {
        if (group.location == floor_to_check && (capacity_ - current_load_ >= group.num_people)) {
          bool group_wants_up = group.destination > group.location;
          if (current_direction_ == Direction::IDLE ||
              (current_direction_ == Direction::UP && group_wants_up) ||
              (current_direction_ == Direction::DOWN && !group_wants_up)) {
            return true;
          }
        }
      }
    }
    return false;
  }

  bool has_pending_pickups_() { return !pending_pickups_.empty(); }

  size_t determine_next_target_floor_() {
    size_t closest_dropoff_floor = current_floor_;
    int min_dist_to_dropoff = 100000;
    if (!passengers_inside_.empty()) {
      for (const auto &p : passengers_inside_) {
        int dist = std::abs((int)p.destination - (int)current_floor_);
        if (dist < min_dist_to_dropoff) {
          min_dist_to_dropoff = dist;
          closest_dropoff_floor = p.destination;
        }
      }
      if (closest_dropoff_floor != current_floor_) {
        current_direction_ = (closest_dropoff_floor > current_floor_) ? Direction::UP : Direction::DOWN;
        return closest_dropoff_floor;
      }
    }


    size_t closest_pickup_floor = current_floor_;
    int min_dist_to_pickup = 100000;
    if (!pending_pickups_.empty()) {
      for (const auto &p : pending_pickups_) {
        int dist = std::abs((int)p.location - (int)current_floor_);
        if (dist < min_dist_to_pickup) {
          min_dist_to_pickup = dist;
          closest_pickup_floor = p.location;
        }
      }
      if (closest_pickup_floor != current_floor_) {
        current_direction_ = (closest_pickup_floor > current_floor_) ? Direction::UP : Direction::DOWN;
        return closest_pickup_floor;
      }
    }

    current_direction_ = Direction::IDLE;

    return current_floor_;
  }

  size_t process_boarding_deboarding_() {
    size_t op_ticks = 0;
    bool activity = false;

    for (size_t i = 0; i < passengers_inside_.size();) {
      if (passengers_inside_[i].destination == current_floor_) {
        logger_->info("Elev " + std::to_string(id_) + " deboards P" + std::to_string(passengers_inside_[i].id) +
                      " (" + std::to_string(passengers_inside_[i].num_people) + "ppl) at F" +
                      std::to_string(current_floor_));
        current_load_ -= passengers_inside_[i].num_people;
        passengers_transported_count_ += passengers_inside_[i].num_people;
        passengers_inside_.erase(passengers_inside_.begin() + i);
        activity = true;
      } else {
        i++;
      }
    }
    if (activity) {
      op_ticks += BOARDING_TICKS;
    }

    bool pending_boarded = false;
    for (size_t i = 0; i < pending_pickups_.size();) {
      if (pending_pickups_[i].location == current_floor_ &&
          (capacity_ - current_load_ >= pending_pickups_[i].num_people)) {
        Person group = pending_pickups_[i];
        logger_->info("Elev " + std::to_string(id_) + " boards P" + std::to_string(group.id) + " (pending) (" +
                      std::to_string(group.num_people) + "ppl) at F" + std::to_string(current_floor_));
        current_load_ += group.num_people;
        passengers_inside_.push_back(group);

        std::chrono::duration<double> waited = std::chrono::system_clock::now() - group.arrival_time;
        logger_->info("   P" + std::to_string(group.id) + " waited " +
                      std::to_string(std::chrono::duration_cast<std::chrono::seconds>(waited).count()) + "s");
        {
          std::lock_guard<std::mutex> stats_lk(*global_stats_mtx_);
          *total_wait_time_seconds_accumulator_ += waited.count();
          (*total_boarded_groups_accumulator_)++;
        }
        pending_pickups_.erase(pending_pickups_.begin() + i);
        pending_boarded = true;
      } else {
        i++;
      }
    }
    if (pending_boarded && !activity) {
      op_ticks += BOARDING_TICKS;
    }
    activity = activity || pending_boarded;


    bool general_boarded = false;
    {
      std::lock_guard<std::mutex> qlk(*queue_mtx_);
      for (size_t i = 0; i < requests_queue_->size();) {
        Person &group_in_q = (*requests_queue_)[i];
        if (group_in_q.location == current_floor_ && (capacity_ - current_load_ >= group_in_q.num_people)) {
          bool wants_up = group_in_q.destination > group_in_q.location;
          if ((current_direction_ == Direction::UP && wants_up) ||
              (current_direction_ == Direction::DOWN && !wants_up) ||
              (current_direction_ == Direction::IDLE && passengers_inside_.empty())) {
            if (current_direction_ == Direction::IDLE && passengers_inside_.empty()) {
              current_direction_ = wants_up ? Direction::UP : Direction::DOWN;
              logger_->info("Elev " + std::to_string(id_) + " sets dir to " +
                            (current_direction_ == Direction::UP ? "UP" : "DOWN") + " by P" +
                            std::to_string(group_in_q.id));
            }
            Person group = group_in_q;
            logger_->info("Elev " + std::to_string(id_) + " boards P" + std::to_string(group.id) +
                          " (general) (" + std::to_string(group.num_people) + "ppl) at F" +
                          std::to_string(current_floor_));
            current_load_ += group.num_people;
            passengers_inside_.push_back(group);

            std::chrono::duration<double> waited = std::chrono::system_clock::now() - group.arrival_time;
            logger_->info("   P" + std::to_string(group.id) + " waited " +
                          std::to_string(std::chrono::duration_cast<std::chrono::seconds>(waited).count()) +
                          "s");
            {
              std::lock_guard<std::mutex> stats_lk(*global_stats_mtx_);
              *total_wait_time_seconds_accumulator_ += waited.count();
              (*total_boarded_groups_accumulator_)++;
            }
            requests_queue_->erase(requests_queue_->begin() + i);
            general_boarded = true;
          } else {
            i++;
          }
        } else {
          i++;
        }
      }
    }
    if (general_boarded && !activity) op_ticks += BOARDING_TICKS;
    activity = activity || general_boarded;

    if (!activity) {
      logger_->info("Elev " + std::to_string(id_) + " no board/deboard at F" + std::to_string(current_floor_));
      return 0;
    }
    return std::max((size_t)1, op_ticks);
  }

  Logger *logger_;
  size_t id_;
  size_t capacity_;

  size_t current_floor_;
  size_t current_load_;
  Status status_;
  Direction current_direction_;
  int remaining_action_ticks_ = 0;

  std::vector<Person> passengers_inside_;
  std::vector<Person> pending_pickups_;

  size_t passengers_transported_count_;
  size_t floors_covered_count_;

  std::thread worker_thread_;
  std::deque<Person> *requests_queue_;
  std::mutex *queue_mtx_;
  std::condition_variable *queue_cv_;
  std::atomic<bool> *stop_simulation_;

  mutable std::mutex elevator_internal_mtx_;

  std::mutex *global_stats_mtx_;
  double *total_wait_time_seconds_accumulator_;
  size_t *total_boarded_groups_accumulator_;
};

class House {
   public:
  House(size_t num_elevators, size_t num_floors, Logger *logger)
      : num_floors_(num_floors),
        logger_(logger),
        stop_simulation_flag_(false),
        total_boarded_groups_count_(0),
        total_wait_time_seconds_sum_(0.0),
        next_person_group_id_(0) {
    logger_->info("House created: " + std::to_string(num_elevators) + " elevs, " + std::to_string(num_floors) +
                  " floors.");
    elevators_pool_.reserve(num_elevators);
    for (size_t i = 0; i < num_elevators; ++i) {
      size_t capacity = 2 + (i % 7);
      elevators_pool_.push_back(std::make_unique<Elevator>(logger_, i + 1, capacity));
    }
  }


  void run_simulation() {
    logger_->info("Simulation starting...");
    for (auto &elevator_ptr : elevators_pool_) {
      elevator_ptr->start(&requests_queue_, &queue_access_mtx_, &queue_condition_var_, &stop_simulation_flag_,
                          &global_stats_mtx_, &total_wait_time_seconds_sum_, &total_boarded_groups_count_);
    }

    std::mt19937 rng(static_cast<unsigned>(std::chrono::system_clock::now().time_since_epoch().count()));
    std::uniform_int_distribution<> floor_dist(1, num_floors_);
    std::uniform_int_distribution<> people_dist(1, 4);

    while (!stop_simulation_flag_.load()) {
      Person new_call;
      new_call.id = next_person_group_id_++;
      new_call.location = floor_dist(rng);
      do {
        new_call.destination = floor_dist(rng);
      } while (new_call.destination == new_call.location);
      new_call.num_people = people_dist(rng);
      new_call.arrival_time = std::chrono::system_clock::now();

      {
        std::lock_guard<std::mutex> lk(queue_access_mtx_);
        requests_queue_.push_back(new_call);
        logger_->info("New call P" + std::to_string(new_call.id) + " (" + std::to_string(new_call.num_people) +
                      "ppl): F" + std::to_string(new_call.location) + "->F" +
                      std::to_string(new_call.destination) +
                      ". Qsize: " + std::to_string(requests_queue_.size()));
      }
      queue_condition_var_.notify_one();

      std::uniform_int_distribution<> delay_dist(300, 1200);
      std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(rng)));
    }
    logger_->info("Call generation stopped.");

    for (auto &elevator_ptr : elevators_pool_) {
      elevator_ptr->join();
    }
    logger_->info("All elevators finished. Simulation ended.");
  }

  void print_statistics() {
    std::cout << "\n=== Final Stats ===\n";
    logger_->info("--- Final Stats ---");

    size_t total_ppl_overall = 0;
    size_t total_flrs_overall = 0;

    for (const auto &elevator_ptr : elevators_pool_) {
      elevator_ptr->print_stats();
      total_ppl_overall += elevator_ptr->get_passengers_transported();
      total_flrs_overall += elevator_ptr->get_floors_covered();
    }

    std::cout << "--------------------\n";
    std::cout << "Total Ppl Transported (all elevs): " << total_ppl_overall << std::endl;
    logger_->info("Total Ppl Transported (all elevs): " + std::to_string(total_ppl_overall));
    std::cout << "Total Floors (all elevs): " << total_flrs_overall << std::endl;
    logger_->info("Total Floors (all elevs): " + std::to_string(total_flrs_overall));

    if (total_boarded_groups_count_ > 0) {
      double avg_wait_s = total_wait_time_seconds_sum_ / total_boarded_groups_count_;
      double avg_wait_t = avg_wait_s / (TICK_DURATION.count() / 1000.0);
      std::cout << "Avg Group Wait: " << std::fixed << std::setprecision(2) << avg_wait_s << "s (~"
                << std::setprecision(1) << avg_wait_t << " ticks/group)." << std::endl;
      logger_->info("Avg Group Wait: " + std::to_string(avg_wait_s) + "s.");
    } else {
      std::cout << "Avg Group Wait: N/A (no groups boarded)\n";
      logger_->info("Avg Group Wait: N/A");
    }
    std::cout << "====================\n";
  }

  void signal_stop() {
    logger_->info("Stop signal received. Notifying components.");
    stop_simulation_flag_.store(true);
    queue_condition_var_.notify_all();
  }


   private:
  size_t num_floors_;
  Logger *logger_;
  std::vector<std::unique_ptr<Elevator>> elevators_pool_;
  std::deque<Person> requests_queue_;
  std::mutex queue_access_mtx_;
  std::condition_variable queue_condition_var_;
  std::atomic<bool> stop_simulation_flag_;
  size_t next_person_group_id_;
  std::mutex global_stats_mtx_;
  double total_wait_time_seconds_sum_;
  size_t total_boarded_groups_count_;
};
int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::cerr << "Usage: " << argv[0] << " <num_elevators> <num_floors>\n";
    return 1;
  }
  size_t num_elevators = 0;
  size_t num_floors = 0;
  try {
    num_elevators = std::stoul(argv[1]);
    num_floors = std::stoul(argv[2]);
  } catch (const std::exception &e) {
    std::cerr << "Arg parsing error: " << e.what() << std::endl;
    return 1;
  }

  if (num_elevators == 0 || num_floors < 2) {
    std::cerr << "Bad args: elevators > 0, floors >= 2.\n";
    return 1;
  }

  std::shared_ptr<Logger> logger =
      Logger::Builder().add_file_handler("elevator_sim.log").set_log_level(LogLevel::INFO).build();

  if (!logger) {
    std::cerr << "Logger init failed. Exiting." << std::endl;
    return 1;
  }

  House house(num_elevators, num_floors, logger.get());

  std::thread sim_thread([&house]() { house.run_simulation(); });

  std::string cmd;
  while (true) {
    std::cout << "\nCmd ('stats', 'exit'): ";
    std::cin >> cmd;

    if (std::cin.fail() || std::cin.eof()) {
      logger->info("Input error/EOF, stopping.");
      house.signal_stop();
      break;
    }

    if (cmd == "stats") {
      house.print_statistics();
    } else if (cmd == "exit") {
      logger->info("User 'exit' cmd.");
      house.signal_stop();
      break;
    } else {
      std::cout << "Unknown cmd: " << cmd << std::endl;
    }
  }

  if (sim_thread.joinable()) {
    sim_thread.join();
  }

  house.print_statistics();
  return 0;
}
