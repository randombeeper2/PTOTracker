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

bool entry_exists(const nlohmann::json& days_off, const std::string& key, const std::string& date) {
	for (const auto& entry : days_off) {
		if (entry.contains(key) && entry[key] == date) {
			return true;
		}
	}
	return false;
}

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

int working_days_elapsed(const std::tm& start_date) {
	std::time_t now = std::time(nullptr);
	std::tm today = *std::localtime(&now);

	std::time_t start = std::mktime(const_cast<std::tm*>(&start_date));
	std::time_t end = std::mktime(&today);

	int weekdays_elapsed = 0;
	for (std::time_t t = start; t <= end; t += 86400) {
		std::tm* tm = std::localtime(&t);
		if (is_weekday(*tm)) {
			weekdays_elapsed++;
		}
	}
	return weekdays_elapsed;
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
		<< "  pto showvaca               Show all saved days off\n"
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

		// don't add a date that already exists
		if (entry_exists(days_off, "date", date)) {
			std::cerr << "Error: A time-off entry for " << date << " already exists.\n";
			return 1;
		}

		// don't add a weekend day
		std::tm date_tm = parse_date(date);
		if (!is_weekday(date_tm)) {
			std::cerr << "Error: Trying to add a date that is a weekend.\n";
			return 1;
		}

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

		// doesn't handle an overlap. for example if 06/20 is already in the days off and we add
		// 6/19-6/22, its not going to care...
		if (entry_exists(days_off, "date", start) || entry_exists(days_off, "date", end) || 
			entry_exists(days_off, "start_date", start) || entry_exists(days_off, "start_date", end) || 
			entry_exists(days_off, "end_date", start) || entry_exists(days_off, "end_date", end)) {
			std::cerr << "Error: A time-off entry already exists for the start or end date.\n";
			return 1;
		}

		// don't add a weekend day
		std::tm start_tm = parse_date(start);
		if (!is_weekday(start_tm)) {
			std::cerr << "Error: Trying to add a start_date that is a weekend.\n";
			return 1;
		}
		std::tm end_tm = parse_date(end);
		if (!is_weekday(end_tm)) {
			std::cerr << "Error: Trying to add a end_date that is a weekend.\n";
			return 1;
		}

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
	if (argc >= 2 && std::string(argv[1]) == "showvaca") {
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
	int working_days = working_days_elapsed(start_tm);

	std::cout << std::fixed << std::setprecision(1);

	list_days_off(days_off);
	std::cout << "PTO Summary:\n"
		<< "  Accrual Rate:" << rate << " hours/day\n"
		<< "  Working Days:" << working_days << " days\n"
		<< "  Accrued:     " << accrued << " hours\n"
		<< "  Used:        " << used << " hours\n"
		<< "  Balance:     " << available << " hours\n";

	

	return 0;
}
