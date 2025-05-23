#include <algorithm>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <numeric>
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
	enum class Direction { UP, DOWN, IDLE /* Stationary */ };

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
	      floors_covered_count_(0),
	      remaining_action_ticks_(0) {}

	void start(std::deque<Person> *requests_queue, std::mutex *queue_mtx, std::condition_variable *queue_cv,
	           std::atomic<bool> *stop_flag, std::mutex *stats_mtx, double *total_wait_time_sec,
	           size_t *total_boarded_groups) {
		requests_queue_ = requests_queue;
		queue_mtx_ = queue_mtx;
		queue_cv_ = queue_cv;
		stop_simulation_ = stop_flag;
		global_stats_mtx_ = stats_mtx;
		total_wait_time_seconds_accumulator_ = total_wait_time_sec;
		total_boarded_groups_accumulator_ = total_boarded_groups;

		worker_thread_ = std::thread(&Elevator::run_logic, this);
	}

	void join() {
		if (worker_thread_.joinable()) {
			worker_thread_.join();
		}
	}

	void print_stats() const {
		std::string s_status, s_dir;
		{
			std::lock_guard<std::mutex> lk(elevator_internal_mtx_);
			s_status = status_to_string_internal(status_);
			s_dir = direction_to_string_internal(current_direction_);
		}

		std::cout << "Elevator " << id_ << " (Cap:" << capacity_ << " Load:" << current_load_ << "): transported "
		          << passengers_transported_count_ << " people, covered " << floors_covered_count_ << " floors. At F"
		          << current_floor_ << " STATUS:" << s_status << " DIR:" << s_dir << "\n";
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
	std::string status_to_string_internal(Status s) const {
		switch (s) {
			case Status::WAITING:
				return "WAITING";
			case Status::MOVING:
				return "MOVING";
			case Status::OPENING_DOORS:
				return "OPENING_DR";
			case Status::BOARDING_DEBOARDING:
				return "BOARD/DEBOARD";
			case Status::CLOSING_DOORS:
				return "CLOSING_DR";
			default:
				return "UNKNOWN";
		}
	}

	std::string direction_to_string_internal(Direction d) const {
		switch (d) {
			case Direction::UP:
				return "UP";
			case Direction::DOWN:
				return "DOWN";
			case Direction::IDLE:
				return "IDLE";
			default:
				return "N/A";
		}
	}

	void log_action(const std::string &action_message) {
		std::string current_state_info;
		{
			std::lock_guard<std::mutex> lk(elevator_internal_mtx_);
			current_state_info = "(F" + std::to_string(current_floor_) + " L" + std::to_string(current_load_) +
			                     " D:" + direction_to_string_internal(current_direction_) + ")";
		}
		logger_->info("Elevator " + std::to_string(id_) + " " + current_state_info + ": " + action_message);
	}

	void run_logic() {
		log_action("started operation.");

		while (true) {
			if (stop_simulation_->load()) {
				std::lock_guard<std::mutex> guard(elevator_internal_mtx_);
				if (passengers_inside_.empty() && pending_pickups_.empty() && status_ == Status::WAITING) {
					log_action("is idle and simulation is stopping. Terminating.");
					break;
				}
			}

			std::unique_lock<std::mutex> elevator_lk(elevator_internal_mtx_);

			bool action_was_initiated_this_cycle = false;
			bool needs_to_stop_now = should_stop_at_floor(current_floor_);

			if (status_ == Status::WAITING || status_ == Status::MOVING ||
			    (status_ == Status::CLOSING_DOORS && remaining_action_ticks_ == 0)) {
				if (needs_to_stop_now) {
					if (status_ == Status::MOVING) {
						log_action("arrived at F" + std::to_string(current_floor_) + " for service.");
					}
					status_ = Status::OPENING_DOORS;
					remaining_action_ticks_ = DOOR_OPEN_TICKS;
					action_was_initiated_this_cycle = true;
				} else if (!passengers_inside_.empty() || !pending_pickups_.empty()) {
					size_t next_target_floor = next_target_floor();
					if (next_target_floor != current_floor_) {
						move_towards(next_target_floor);
						action_was_initiated_this_cycle = true;
					} else {
						status_ = Status::WAITING;
						if (!passengers_inside_.empty() || !pending_pickups_.empty()) {
							log_action("target is current floor but no stop needed. Re-evaluating. Becoming IDLE.");
							current_direction_ = Direction::IDLE;
						}
						action_was_initiated_this_cycle = true;
					}
				} else {
					Person new_request_from_queue;
					bool call_was_taken = false;
					{
						std::lock_guard<std::mutex> q_lk(*queue_mtx_);
						if (!requests_queue_->empty()) {
							auto it = std::find_if(
							    requests_queue_->begin(), requests_queue_->end(),
							    [&](const Person &p) { return capacity_ - current_load_ >= p.num_people; });

							if (it != requests_queue_->end()) {
								new_request_from_queue = *it;
								requests_queue_->erase(it);
								call_was_taken = true;
							}
						}
					}

					if (call_was_taken) {
						log_action("accepted call P" + std::to_string(new_request_from_queue.id) + " (" +
						           std::to_string(new_request_from_queue.num_people) + " people) from F" +
						           std::to_string(new_request_from_queue.location) + " to F" +
						           std::to_string(new_request_from_queue.destination));
						pending_pickups_.push_back(new_request_from_queue);
						if (current_direction_ == Direction::IDLE) {
							if (new_request_from_queue.location != current_floor_) {
								current_direction_ = (new_request_from_queue.location > current_floor_)
								                         ? Direction::UP
								                         : Direction::DOWN;
							} else {
								current_direction_ =
								    (new_request_from_queue.destination > new_request_from_queue.location)
								        ? Direction::UP
								        : Direction::DOWN;
							}
						}
						action_was_initiated_this_cycle = true;
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
				action_was_initiated_this_cycle = true;

				elevator_lk.lock();
				if (remaining_action_ticks_ == 0) {
					switch (status_) {
						case Status::OPENING_DOORS:
							log_action("doors opened at F" + std::to_string(current_floor_));
							status_ = Status::BOARDING_DEBOARDING;
							remaining_action_ticks_ = process_boarding_deboarding_get_ticks_();
							break;
						case Status::BOARDING_DEBOARDING:
							log_action("boarding/deboarding finished at F" + std::to_string(current_floor_));
							status_ = Status::CLOSING_DOORS;
							remaining_action_ticks_ = DOOR_CLOSE_TICKS;
							break;
						case Status::CLOSING_DOORS:
							log_action("doors closed at F" + std::to_string(current_floor_));
							status_ = Status::WAITING;
							break;
						case Status::MOVING:

							break;
						default:
							log_action("Warning: Unexpected status " + status_to_string_internal(status_) +
							           " at end of timed action.");
							break;
					}
				}
				elevator_lk.unlock();
			} else if (!action_was_initiated_this_cycle) {
				std::unique_lock<std::mutex> q_lk_wait(*queue_mtx_);

				if (stop_simulation_->load() && requests_queue_->empty() && passengers_inside_.empty() &&
				    pending_pickups_.empty()) {
					break;
				}

				queue_cv_->wait_for(q_lk_wait, TICK_DURATION * 3,
				                    [&] { return !requests_queue_->empty() || stop_simulation_->load(); });
			}
		}
		log_action("operation finished. Final load: " + std::to_string(current_load_));
	}

	void move_towards(size_t target_floor) {
		if (target_floor == current_floor_) {
			return;
		}

		status_ = Status::MOVING;

		if (current_direction_ == Direction::IDLE ||
		    (current_direction_ == Direction::UP && target_floor < current_floor_) ||
		    (current_direction_ == Direction::DOWN && target_floor > current_floor_)) {
			current_direction_ = (target_floor > current_floor_) ? Direction::UP : Direction::DOWN;
		}

		if (current_direction_ == Direction::UP)
			current_floor_++;
		else if (current_direction_ == Direction::DOWN)
			current_floor_--;
		else {
			log_action("Error: Attempting to move while IDLE or inconsistent direction. Forcing direction.");
			current_direction_ = (target_floor > current_floor_) ? Direction::UP : Direction::DOWN;
			if (current_direction_ == Direction::UP)
				current_floor_++;
			else
				current_floor_--;
		}

		log_action("now at F" + std::to_string(current_floor_) + " (moving " +
		           direction_to_string_internal(current_direction_) + ")");
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
			std::lock_guard<std::mutex> q_lk(*queue_mtx_);
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

	size_t next_target_floor() {
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

	size_t process_boarding_deboarding_get_ticks_() {
		bool any_activity = false;

		auto deboard_iter_end =
		    std::remove_if(passengers_inside_.begin(), passengers_inside_.end(), [&](const Person &p) {
			    if (p.destination == current_floor_) {
				    log_action("deboarding P" + std::to_string(p.id) + " (" + std::to_string(p.num_people) +
				               " people)");
				    current_load_ -= p.num_people;
				    passengers_transported_count_ += p.num_people;
				    any_activity = true;
				    return true;
			    }
			    return false;
		    });
		passengers_inside_.erase(deboard_iter_end, passengers_inside_.end());

		auto pending_iter_end = std::remove_if(pending_pickups_.begin(), pending_pickups_.end(), [&](const Person &p) {
			if (p.location == current_floor_ && (capacity_ - current_load_ >= p.num_people)) {
				log_action("boarding P" + std::to_string(p.id) + " (pending) (" + std::to_string(p.num_people) +
				           " people)");
				current_load_ += p.num_people;
				passengers_inside_.push_back(p);

				std::chrono::duration<double> waited_seconds = std::chrono::system_clock::now() - p.arrival_time;

				{
					std::lock_guard<std::mutex> stats_lk(*global_stats_mtx_);
					*total_wait_time_seconds_accumulator_ += waited_seconds.count();
					(*total_boarded_groups_accumulator_)++;
				}
				any_activity = true;
				return true;
			}
			return false;
		});
		pending_pickups_.erase(pending_iter_end, pending_pickups_.end());

		{
			std::lock_guard<std::mutex> q_lk(*queue_mtx_);
			auto general_q_iter_end =
			    std::remove_if(requests_queue_->begin(), requests_queue_->end(), [&](const Person &p) {
				    if (p.location == current_floor_ && (capacity_ - current_load_ >= p.num_people)) {
					    bool wants_to_go_up = p.destination > p.location;

					    if ((current_direction_ == Direction::UP && wants_to_go_up) ||
					        (current_direction_ == Direction::DOWN && !wants_to_go_up) ||
					        (current_direction_ == Direction::IDLE && passengers_inside_.empty())) {
						    if (current_direction_ == Direction::IDLE && passengers_inside_.empty()) {
							    current_direction_ = wants_to_go_up ? Direction::UP : Direction::DOWN;
							    log_action("set direction to " + direction_to_string_internal(current_direction_) +
							               " by P" + std::to_string(p.id));
						    }
						    log_action("boarding P" + std::to_string(p.id) + " (general queue) (" +
						               std::to_string(p.num_people) + " people)");
						    current_load_ += p.num_people;
						    passengers_inside_.push_back(p);

						    std::chrono::duration<double> waited_seconds =
						        std::chrono::system_clock::now() - p.arrival_time;

						    {
							    std::lock_guard<std::mutex> stats_lk(*global_stats_mtx_);
							    *total_wait_time_seconds_accumulator_ += waited_seconds.count();
							    (*total_boarded_groups_accumulator_)++;
						    }
						    any_activity = true;
						    return true;
					    }
				    }
				    return false;
			    });
			requests_queue_->erase(general_q_iter_end, requests_queue_->end());
		}

		if (!any_activity) {
			log_action("no boarding/deboarding activity at F" + std::to_string(current_floor_));
			return 0;
		}
		return BOARDING_TICKS;
	}

	Logger *logger_;
	size_t id_;
	size_t capacity_;

	size_t current_floor_;
	size_t current_load_;
	Status status_;
	Direction current_direction_;
	int remaining_action_ticks_;

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
		logger_->info("House created: " + std::to_string(num_elevators) + " elevators, " + std::to_string(num_floors) +
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
				              " people): F" + std::to_string(new_call.location) + "->F" +
				              std::to_string(new_call.destination) +
				              ". Queue size: " + std::to_string(requests_queue_.size()));
			}
			queue_condition_var_.notify_one();

			std::uniform_int_distribution<> delay_dist(400, 1500);
			std::this_thread::sleep_for(std::chrono::milliseconds(delay_dist(rng)));
		}
		logger_->info("Call generation stopped.");

		for (auto &elevator_ptr : elevators_pool_) {
			elevator_ptr->join();
		}
		logger_->info("All elevators finished. Simulation ended.");
	}

	void print_statistics() {
		std::cout << "\n=== Final Statistics ===\n";
		logger_->info("--- Final Statistics ---");

		size_t total_people_transported = 0;
		size_t total_floors_travelled = 0;

		for (const auto &elevator_ptr : elevators_pool_) {
			elevator_ptr->print_stats();
			total_people_transported += elevator_ptr->get_passengers_transported();
			total_floors_travelled += elevator_ptr->get_floors_covered();
		}

		std::cout << "--------------------\n";
		std::cout << "Total People Transported (all elevators): " << total_people_transported << std::endl;
		logger_->info("Total People Transported (all elevators): " + std::to_string(total_people_transported));
		std::cout << "Total Floors Travelled (all elevators): " << total_floors_travelled << std::endl;
		logger_->info("Total Floors Travelled (all elevators): " + std::to_string(total_floors_travelled));

		if (total_boarded_groups_count_ > 0) {
			double avg_wait_seconds = total_wait_time_seconds_sum_ / total_boarded_groups_count_;
			double avg_wait_ticks = avg_wait_seconds / (TICK_DURATION.count() / 1000.0);
			std::cout << "Average Group Wait Time: " << std::fixed << std::setprecision(2) << avg_wait_seconds
			          << " seconds (~" << std::setprecision(1) << avg_wait_ticks << " ticks/group)." << std::endl;
			logger_->info("Average Group Wait Time: " + std::to_string(avg_wait_seconds) + " seconds.");
		} else {
			std::cout << "Average Group Wait Time: N/A (no groups boarded)\n";
			logger_->info("Average Group Wait Time: N/A");
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
		std::cerr << "Usage: " << argv[0] << " <num_elevators> <num_floors>\n"
		          << "Example: " << argv[0] << " 3 10\n";
		return 1;
	}
	size_t num_elevators, num_floors;
	try {
		num_elevators = std::stoul(argv[1]);
		num_floors = std::stoul(argv[2]);
	} catch (const std::exception &e) {
		std::cerr << "Error parsing arguments: " << e.what() << std::endl;
		return 1;
	}
	if (num_elevators == 0 || num_floors < 2) {
		std::cerr << "Invalid arguments: num_elevators must be > 0, num_floors must be >= 2.\n";
		return 1;
	}

	std::shared_ptr<Logger> logger =
	    Logger::Builder().add_file_handler("elevator_simulation.log").set_log_level(LogLevel::INFO).build();
	if (!logger) {
		std::cerr << "Failed to initialize logger. Exiting." << std::endl;
		return 1;
	}

	logger->info("Application started.");

	House house_simulation(num_elevators, num_floors, logger.get());
	std::thread simulation_thread([&]() {
		try {
			house_simulation.run_simulation();
		} catch (const std::exception &e) {
			logger->critical("Critical error in simulation thread: " + std::string(e.what()));
		} catch (...) {
			logger->critical("Unknown critical error in simulation thread.");
		}
	});

	std::string user_command;
	while (true) {
		std::cout << "\nEnter command ('stats', 'exit'): ";
		if (!(std::cin >> user_command)) {
			logger->info("Input stream error or EOF detected.");
			if (!std::cin.eof()) {
				std::cin.clear();
				std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
			}
			house_simulation.signal_stop();
			break;
		}

		if (user_command == "stats") {
			house_simulation.print_statistics();
		} else if (user_command == "exit") {
			logger->info("User 'exit' command received.");
			house_simulation.signal_stop();
			break;
		} else {
			std::cout << "Unknown command: '" << user_command << "'\n";
		}
	}

	if (simulation_thread.joinable()) {
		simulation_thread.join();
	}

	logger->info("Final statistics before application exit:");
	house_simulation.print_statistics();
	logger->info("Application finished.");
	return 0;
}