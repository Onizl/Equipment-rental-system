#include <iostream>
#include <vector>
#include <map>
#include <string>
#include <memory>
#include <iomanip>
#include <random>
#include <chrono>
#include <thread>
#include <algorithm>

using namespace std;

const int NUM_PROJECTS = 10;
const int NUM_EQUIPMENT = 12;
const int BUFFER_SIZE = 10;
const int TOTAL_REQUESTS = 2500;
const double LYAMBDA = 2;
const int a = 6;
const int b = 8;

random_device rd;
mt19937 gen(rd());

double random_exponential(double lambda) {
	exponential_distribution<double> dist(lambda);
	return dist(gen);
}

double random_uniform(double min, double max) {
	uniform_real_distribution<double> dist(min, max);
	return dist(gen);
}

int random_int(int min, int max) {
	uniform_int_distribution<> dist(min, max);
	return dist(gen);
}

string random_equipment_type() {
	vector<string> types = { "excavator", "crane", "concrete_mixer" };
	return types[random_int(0, types.size() - 1)];
}

class Request {
private:
	int project_id;
	string equipment_type;
	double requested_period;
	int priority;
	string status;

	double arrival_time;
	double completion_time;
	double wait_time;

public:
	Request(int pid, string eq_type, double arv_time, double req_period, int prio)
		: project_id(pid), equipment_type(eq_type), requested_period(req_period),
		priority(prio), status("pending"), arrival_time(arv_time), completion_time(-1),
		wait_time(0) {

	}

	void update_status(const string &new_status) {
		status = new_status;
	}

	int get_project_id() const {
		return project_id;
	}

	const string &get_equipment_type() const {
		return equipment_type;
	}

	double get_requested_period() const {
		return requested_period;
	}

	int get_priority() const {
		return priority;
	}

	const string &get_status() const {
		return status;
	}

	double get_wait_time() const {
		return wait_time;
	}

	friend class Equipment;
};


class Equipment {
private:
	int equipment_id;
	string type;
	string status;
	int priority;
	shared_ptr<Request> current_request;

	double busy_time;
	double completion_time;

public:
	Equipment(int eid, string eq_type)
		: equipment_id(eid), type(eq_type), priority(eid),
		status("free"), current_request(nullptr), busy_time(0), completion_time(0) {
	}

	void assign_request(shared_ptr<Request> request, double current_time) {
		status = "busy";
		current_request = request;
		current_request->wait_time = current_time - current_request->arrival_time;
		completion_time = current_time + request->get_requested_period();
		busy_time += request->get_requested_period();
	}

	void complete_request(double current_time) {
		if (current_request != nullptr && current_time >= completion_time) {
			current_request->completion_time = current_time;
			current_request->update_status("processed");
			current_request = nullptr;
			status = "free";
		}
	}

	int get_equipment_id() const {
		return equipment_id;
	}

	const string &get_type() const {
		return type;
	}

	const string &get_status() const {
		return status;
	}

	int get_priority() const {
		return priority;
	}

	double get_busy_time() const {
		return busy_time;
	}

	double get_completion_time() const {
		return completion_time;
	}
};


class Project {
private:
	int project_id;
	int priority;

public:
	Project(int pid)
		: project_id(pid), priority(pid) {
	}

	shared_ptr<Request> generate_request(double current_time) {
		string equipment_type = random_equipment_type();
		double requested_period = random_uniform(a, b);
		auto request = make_shared<Request>(project_id, equipment_type, current_time, requested_period, priority);
		return request;
	}
};


class Buffer {
private:
	vector<shared_ptr<Request>> requests;
	int capacity;
	int pointer;

	bool add_request(shared_ptr<Request> request) {
		for (int i = 0; i < capacity; ++i) {
			int index = (pointer + i) % capacity;
			if (requests[index] == nullptr) {
				requests[index] = request;
				pointer = (index + 1) % capacity;
				return true;
			}
		}
		return false;
	}

	shared_ptr<Request> remove_request() {
		if (requests[pointer] != nullptr) {
			auto req = requests[pointer];
			requests[pointer] = nullptr;
			return req;
		}
		return nullptr;
	}

public:
	Buffer(int cap) : capacity(cap), pointer(0) {
		requests.resize(capacity, nullptr);
	}

	const vector<shared_ptr<Request>> &get_requests() const {
		return requests;
	}

	bool is_full() const {
		for (const auto &req : requests) {
			if (req == nullptr)
				return false;
		}
		return true;
	}

	friend class PlacementDispatcher;
	friend class SelectionDispatcher;
};

ostream &operator<<(ostream &os, const Buffer &buffer) {
	os << "Buffer: [";
	for (const auto &req : buffer.get_requests()) {
		if (req != nullptr) {
			os << req->get_project_id() << " ";
		}
		else {
			os << "- ";
		}
	}
	os << "]";
	return os;
}


class PlacementDispatcher {
private:
	Buffer &buffer;

public:
	PlacementDispatcher(Buffer &buf) : buffer(buf) {}

	void place_request(shared_ptr<Request> request, vector<shared_ptr<Request>> &completed_requests, double current_time) {
		if (!buffer.add_request(request)) {

			shared_ptr<Request> rejected_request = buffer.remove_request();
			if (rejected_request != nullptr) {
				rejected_request->update_status("rejected");
				completed_requests.push_back(rejected_request);
			}
			buffer.add_request(request);
		}
	}
};


class SelectionDispatcher {
private:
	vector<Equipment> &equipment_list;
	Buffer &buffer;

public:
	SelectionDispatcher(vector<Equipment> &eq_list, Buffer &buf)
		: equipment_list(eq_list), buffer(buf) {
	}

	void assign_equipment(double current_time, vector<shared_ptr<Request>> &completed_requests) {
		map<int, vector<shared_ptr<Request> *>> packages;
		for (auto &req : buffer.requests) {
			if (req != nullptr) {
				packages[req->get_project_id()].push_back(&req);
			}
		}

		if (!packages.empty()) {
			auto highest_priority_package = packages.begin();

			int highest_priority_project_id = highest_priority_package->first;
			vector<shared_ptr<Request> *> &highest_priority_requests = highest_priority_package->second;

			for (auto &equipment : equipment_list) {
				if (equipment.get_status() == "free") {
					for (auto req_ptr : highest_priority_requests) {

						shared_ptr<Request> &cell = *req_ptr;

						if (cell != nullptr && cell->get_project_id() == highest_priority_project_id) {
							equipment.assign_request(cell, current_time);
							cell->update_status("processed");
							completed_requests.push_back(cell);
							cell = nullptr;
							break;
						}
					}
				}
			}
		}
	}
};

double p_sum;
double utilization_sum;
double avg_total_sum; 
double total_buf_sum;
int completed_req;
int rejected_req;

void calculate_statistics(const vector<shared_ptr<Request>> &completed_requests, const vector<Equipment> &equipment_list, double total_time) {
	map<int, map<string, int>> source_stats;

	for (const auto &request : completed_requests) {
		if (request == nullptr)
			continue;

		int project_id = request->get_project_id();
		if (source_stats.find(project_id) == source_stats.end()) {
			source_stats[project_id] =
			{
				{"total", 0},
				{"rejected", 0},
				{"buffer_time", 0},
				{"processing_time", 0},
				{"buffer_time_sq", 0},
				{"processing_time_sq", 0}
			};
		}

		source_stats[project_id]["total"]++;
		if (request->get_status() == "rejected") {
			source_stats[project_id]["rejected"]++;
		}
		else {
			double buffer_time = request->get_wait_time();
			double processing_time = request->get_requested_period();

			source_stats[project_id]["buffer_time"] += buffer_time;
			source_stats[project_id]["processing_time"] += processing_time;
			source_stats[project_id]["buffer_time_sq"] += buffer_time * buffer_time;
			source_stats[project_id]["processing_time_sq"] += processing_time * processing_time;
		}
	}

	cout << "\n=== Source Statistics ===\n";
	cout << setw(15) << "Project ID" << setw(15) << "Total" << setw(15) << "Rejected" << setw(15) << "P(reject)"
		<< setw(20) << "Avg T(stay)" << setw(20) << "Avg T(service)" << setw(20) << "Avg T(buffer)"
		<< setw(20) << "D(T(service))" << setw(20) << "D(T(buffer))" << "\n";

	for (auto it = source_stats.begin(); it != source_stats.end(); ++it) {
		int project_id = it->first;
		const std::map<string, int> &stats = it->second;

		int total = stats.at("total");
		int rejected = stats.at("rejected");
		double p_reject = (total > 0) ? ((double)rejected / total) : 0;

		double avg_buffer_time = (total - rejected > 0) ? ((double)stats.at("buffer_time") / (total - rejected)) : 0;
		double avg_processing_time = (total - rejected > 0) ? ((double)stats.at("processing_time") / (total - rejected)) : 0;
		double avg_total_time = avg_buffer_time + avg_processing_time;

		double buffer_time_dispersion =
			(total - rejected > 0) ?
			((double)stats.at("buffer_time_sq") / (total - rejected)) - (avg_buffer_time * avg_buffer_time) : 0;

		double processing_time_dispersion =
			(total - rejected > 0) ?
			((double)stats.at("processing_time_sq") / (total - rejected)) - (avg_processing_time * avg_processing_time) : 0;

		p_sum += p_reject;
		avg_total_sum += avg_total_time;
		total_buf_sum += (double)stats.at("buffer_time");
		completed_req += total - rejected;
		rejected_req += rejected;

		cout << fixed << setprecision(2);

		cout << setw(15) << project_id
			<< setw(15) << total
			<< setw(15) << rejected
			<< setw(15) << p_reject
			<< setw(20) << avg_total_time
			<< setw(20) << avg_processing_time
			<< setw(20) << avg_buffer_time
			<< setw(20) << processing_time_dispersion
			<< setw(20) << buffer_time_dispersion << "\n";
	}

	cout << "\n=== Equipment Statistics ===\n";
	cout << setw(15) << "Equipment ID" << setw(15) << "Utilization" << setw(15) << "Busy Time\n";

	for (const auto &equipment : equipment_list) {
		double utilization = total_time > 0 ? (equipment.get_busy_time() / total_time) : 0;
		cout << setw(15) << equipment.get_equipment_id()
			<< setw(15) << utilization
			<< setw(10) << equipment.get_busy_time() << "\n";
		utilization_sum += utilization;
	}
	double p_mean = p_sum / NUM_PROJECTS;
	double utizilation_mean = utilization_sum / NUM_EQUIPMENT;
	double avg_total_mean = avg_total_sum / NUM_PROJECTS;
	cout << '\n' << "Mean p rejected: " << p_mean << " " << "Mean utizilation: " << utizilation_mean << " " << "Mean total time: " << avg_total_mean;
	cout << '\n' << "Completed req: " << completed_req << " " << "Rejecected req: " << rejected_req << " " << "Total buf time: " << total_buf_sum;
}

bool is_simulation_complete(const Buffer &buffer, const vector<Equipment> &equipment_list) {
	for (const auto &request : buffer.get_requests()) {
		if (request != nullptr)
			return false;
	}

	for (const auto &equipment : equipment_list) {
		if (equipment.get_status() == "busy")
			return false;
	}

	return true;
}

void print_status(const Buffer &buffer, const vector<Equipment> &equipment_list) {
	cout << buffer << "\n";
	for (const auto &eq : equipment_list) {
		cout << "Equipment " << eq.get_equipment_id() << " (" << eq.get_type() << "): " << eq.get_status();
		if (eq.get_status() == "busy") {
			cout << " (completes at " << eq.get_completion_time() << ")";
		}
		cout << "\n";
	}
}

void run_simulation(bool step_by_step = false) {
	vector<Project> projects;
	for (int i = 0; i < NUM_PROJECTS; ++i) {
		projects.emplace_back(i + 1);
	}

	vector<Equipment> equipment_list;
	for (int i = 0; i < NUM_EQUIPMENT; ++i) {
		equipment_list.emplace_back(i + 1, random_equipment_type());
	}

	Buffer buffer(BUFFER_SIZE);
	PlacementDispatcher placement_dispatcher(buffer);
	SelectionDispatcher selection_dispatcher(equipment_list, buffer);

	vector<shared_ptr<Request>> completed_requests;

	double current_time = 0;
	int processed_requests = 0;
	while (processed_requests < TOTAL_REQUESTS || !is_simulation_complete(buffer, equipment_list)) {
		Project &project = projects[random_int(0, NUM_PROJECTS - 1)];

		if (processed_requests < TOTAL_REQUESTS) {
			auto request = project.generate_request(current_time);
			placement_dispatcher.place_request(request, completed_requests, current_time);
			processed_requests++;
		}

		for (auto &equipment : equipment_list) {
			equipment.complete_request(current_time);
		}

		selection_dispatcher.assign_equipment(current_time, completed_requests);

		if (step_by_step) {
			cout << "\n=== Step ===\n";
			cout << "New request created at the following time: " << current_time << "\n";
			print_status(buffer, equipment_list);
			this_thread::sleep_for(chrono::milliseconds(500));
		}
		current_time += random_exponential(LYAMBDA);
	}

	double total_time = current_time;
	calculate_statistics(completed_requests, equipment_list, total_time);
}

int main() {
	run_simulation(true);

	return 0;
}
