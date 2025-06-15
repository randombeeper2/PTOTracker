#define _CRT_SECURE_NO_WARNINGS

#include <sstream>
#include <iostream>
#include <fstream>
 
#include <vector>
#include <string>
#include <json.hpp>
#include <ctime>
#include <iomanip>

using json = nlohmann::json;

const std::string SETTINGS_FILE = "../../settings.json";
const std::string DAYS_OFF_FILE = "../../days_off.json";

std::tm parse_date(const std::string& date_str) {
	std::tm tm = {};
	std::istringstream ss(date_str);
	ss >> std::get_time(&tm, "%Y-%m-%d");
	if (ss.fail()) {
		throw std::runtime_error("Invalid date format. Use YYYY-MM-DD.");
	}
	tm.tm_hour = 0;
	tm.tm_min = 0;
	tm.tm_sec = 0;
	return tm;
}

std::string format_date(const std::tm& tm) {
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d");
	return oss.str();
}

bool is_weekday(const std::tm& tm) {
	std::time_t t = std::mktime(const_cast<std::tm*>(&tm));
	std::tm* lt = std::localtime(&t);
	return lt->tm_wday != 0 && lt->tm_wday != 6; // not Sunday or Saturday
}

double calculate_accrued_hours(const std::tm& start_date, double accrual_rate) {
	std::time_t now = std::time(nullptr);
	std::tm today = *std::localtime(&now);

	std::time_t start = std::mktime(const_cast<std::tm*>(&start_date));
	std::time_t end = std::mktime(&today);

	double accrued = 0.0;
	for (std::time_t t = start; t <= end; t += 86400) {
		std::tm* tm = std::localtime(&t);
		if (is_weekday(*tm)) {
			accrued += accrual_rate;
		}
	}
	return accrued;
}

double calculate_used_hours(const json& days_off) {
	double used_hours = 0.0;

	for (const auto& entry : days_off) {
		if (entry.contains("date")) {
			used_hours += entry.value("hours", 8.0);
		}
		else if (entry.contains("start_date") && entry.contains("end_date")) {
			std::tm start_tm = parse_date(entry["start_date"]);
			std::tm end_tm = parse_date(entry["end_date"]);
			double hours_per_day = entry.value("hours_per_day", 8.0);

			std::time_t start = std::mktime(&start_tm);
			std::time_t end = std::mktime(&end_tm);

			for (std::time_t t = start; t <= end; t += 86400) {
				std::tm* current_tm = std::localtime(&t);
				if (is_weekday(*current_tm)) {
					used_hours += hours_per_day;
				}
			}
		}
	}

	return used_hours;
}

void save_days_off(const std::string& path, const json& days_off) {
	std::ofstream ofs(path);
	if (!ofs) {
		std::cerr << "Error: Unable to write to " << path << "\n";
		return;
	}
	ofs << days_off.dump(4); // pretty print
}

void list_days_off(const json& days_off) {
	std::cout << "Saved days off:\n";
	for (const auto& entry : days_off) {
		if (entry.contains("date")) {
			std::cout << "  " << entry["date"] << " (" << entry.value("hours", 8.0) << "h)\n";
		}
		else if (entry.contains("start_date")) {
			std::cout << "  " << entry["start_date"] << " to " << entry["end_date"]
				<< " (" << entry.value("hours_per_day", 8.0) << "h/day)\n";
		}
	}
}

void print_usage() {
	std::cout << "Usage:\n"
		<< "  pto                        Show available PTO\n"
		<< "  pto add <date> [hours]     Add a day off (default: 8 hours)\n"
		<< "  pto add_range <start> <end> [hours/day]  Add range of days off (default: 8)\n"
		<< "  pto list                   List all saved days off\n"
		<< "  pto remove <date>          Remove entries matching a date (exact match to 'date' or 'start_date')\n"
		<< "  pto usage                  Show this help message\n\n"
		<< "Date format: yyyy-mm-dd\n\n";
}

int main(int argc, char* argv[]) {


	// Show usage if explicitly asked
	if (argc >= 2 && std::string(argv[1]) == "usage") {
		print_usage();
		return 0;
	}


	// Load settings
	std::ifstream settings_ifs(SETTINGS_FILE);
	if (!settings_ifs) {
		std::cerr << "Error: Cannot open settings file.\n";
		return 1;
	}
	json settings;
	settings_ifs >> settings;

	// Load or create days_off
	json days_off = json::array();
	std::ifstream days_off_ifs(DAYS_OFF_FILE);
	if (days_off_ifs) {
		days_off_ifs >> days_off;
	}

	// CLI: add single date
	if (argc >= 3 && std::string(argv[1]) == "add") {
		std::string date = argv[2];
		double hours = (argc >= 4) ? std::stod(argv[3]) : 8.0;
		days_off.push_back({ {"date", date}, {"hours", hours} });
		save_days_off(DAYS_OFF_FILE, days_off);
		std::cout << "Added day off: " << date << " (" << hours << "h)\n";
		return 0;
	}

	// CLI: add date range
	if (argc >= 4 && std::string(argv[1]) == "add_range") {
		std::string start = argv[2];
		std::string end = argv[3];
		double hours_per_day = (argc >= 5) ? std::stod(argv[4]) : 8.0;
		days_off.push_back({
			{"start_date", start},
			{"end_date", end},
			{"hours_per_day", hours_per_day}
			});
		save_days_off(DAYS_OFF_FILE, days_off);
		std::cout << "Added days off: " << start << " to " << end << " (" << hours_per_day << "h/day)\n";
		return 0;
	}

	// CLI: list
	if (argc >= 2 && std::string(argv[1]) == "list") {
		list_days_off(days_off);
		return 0;
	}

	// CLI: remove by date (matches any "date" or "start_date")
	if (argc >= 3 && std::string(argv[1]) == "remove") {
		std::string target = argv[2];
		auto new_days_off = json::array();
		for (const auto& entry : days_off) {
			if ((entry.contains("date") && entry["date"] == target) ||
				(entry.contains("start_date") && entry["start_date"] == target)) {
				continue; // skip
			}
			new_days_off.push_back(entry);
		}
		save_days_off(DAYS_OFF_FILE, new_days_off);
		std::cout << "Removed entries for date: " << target << "\n";
		return 0;
	}

	if (argc > 1) {
		std::cerr << "Unknown command or wrong usage.\n";
		print_usage();
		return 1;
	}

	// Default behavior: calculate PTO
	std::tm start_tm = parse_date(settings["start_date"]);
	double rate = settings["accrual_rate_per_day"];
	double accrued = calculate_accrued_hours(start_tm, rate);
	double used = calculate_used_hours(days_off);
	double available = accrued - used;


	std::cout << "PTO Summary:\n"
		<< "  Accrued:  " << accrued << " hours\n"
		<< "  Used:     " << used << " hours\n"
		<< "  Balance:  " << available << " hours\n";

	return 0;
}
