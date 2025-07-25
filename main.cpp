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

// Formats hours as a string, converting to days and remaining hours if hours > 8
std::string format_hrs(double hours) {
    std::ostringstream oss;
    if (hours > 8.0) {
        int days = static_cast<int>(hours / 8.0);
        double rem_hours = hours - (days * 8.0);
        oss << days << " day" << (days == 1 ? "" : "s");
        if (rem_hours > 0.01) {
            oss << " " << std::fixed << std::setprecision(1) << rem_hours << " hr" << (rem_hours == 1.0 ? "" : "s");
        }
    } else {
        oss << std::fixed << std::setprecision(1) << hours << " hr" << (hours == 1.0 ? "" : "s");
    }
    return oss.str();
}

static bool entry_exists(const nlohmann::json& days_off, const std::string& key, const std::string& date) {
	for (const auto& entry : days_off) {
		if (entry.contains(key) && entry[key] == date) {
			return true;
		}
	}
	return false;
}

// this needs to check if the date provided is contained within any days off entries
// including date, start_date, end_date, and between start_date & end_date
static bool is_day_off(const nlohmann::json& days_off, const std::string& date) {
	// check specific dates
	// check date ranges

}

std::tm date_string_to_tm(const std::string& date_str) {
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

std::string tm_to_date_string(const std::tm& tm) {
	std::ostringstream oss;
	oss << std::put_time(&tm, "%Y-%m-%d");
	return oss.str();
}

bool is_weekday(const std::tm& tm) {
	std::time_t t = std::mktime(const_cast<std::tm*>(&tm));
	std::tm* lt = std::localtime(&t);
	return lt->tm_wday != 0 && lt->tm_wday != 6; // not Sunday or Saturday
}

int working_days_elapsed_since(const std::tm& start_date) {
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

double calculate_accrued_hours_to(const json& days_off, const std::tm& from_date, const std::tm& to_date, double accrual_rate) {
	std::time_t start = std::mktime(const_cast<std::tm*>(&from_date));
	std::time_t end = std::mktime(const_cast<std::tm*>(&to_date));

	double accrued = 0.0;
	for (std::time_t t = start; t <= end; t += 86400) {
		std::tm* tm = std::localtime(&t);
		if (is_weekday(*tm)) {
			accrued += accrual_rate;
		}
	}
	return accrued;
}

double calculate_accrued_hours(const json& days_off, const std::tm& start_date, double accrual_rate) {
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

double calculate_hours_of_days_off(const json& days_off) {
	double used_hours = 0.0;

	for (const auto& entry : days_off) {
		if (entry.contains("date")) {
			used_hours += entry.value("hours", 8.0);
		}
		else if (entry.contains("start_date") && entry.contains("end_date")) {
			std::tm start_tm = date_string_to_tm(entry["start_date"]);
			std::tm end_tm = date_string_to_tm(entry["end_date"]);
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
    if (days_off.empty()) {
        std::cout << "No days off recorded.\n";
        return;
    }
    std::cout << "-----------------------------------------------\n";
    std::cout << std::left << std::setw(22) << "Date/Range" << std::setw(18) << "Type" << std::setw(10) << "Hours" << "\n";
    std::cout << "-----------------------------------------------\n";
    for (const auto& entry : days_off) {
        if (entry.contains("date")) {
            std::cout << std::right << std::setw(22) << entry["date"]
                      << std::setw(18) << "Single Day"
                      << std::setw(14) << entry.value("hours", 8.0)
                      << "\n";
        }
        else if (entry.contains("start_date")) {
            std::string range = std::string(entry["start_date"]) + " to " + std::string(entry["end_date"]);
            std::cout << std::right << std::setw(22) << range
                      << std::setw(18) << "Range"
                      << std::setw(14) << entry.value("hours_per_day", 8.0)
                      << "\n";
        }
    }
    std::cout << "-----------------------------------------------\n";
}

bool is_future_date(std::tm& input_tm) {

	input_tm.tm_hour = 0; 
	input_tm.tm_min = 0; 
	input_tm.tm_sec = 0;

	std::time_t input_time = std::mktime(&input_tm);

	// Get today's date, set to midnight
	std::time_t now = std::time(nullptr);
	std::tm today_tm = *std::localtime(&now);
	today_tm.tm_hour = 0; today_tm.tm_min = 0; today_tm.tm_sec = 0;
	std::time_t today_time = std::mktime(&today_tm);

	return input_time > today_time;
}

void print_usage() {
	std::cout << "Usage (date format used: yyyy-mm-dd):\n"
		<< "  pto                        Show available PTO\n"
		<< "  pto add <date> [hours]     Add a day off (default: 8 hours)\n"
		<< "  pto add_range <start> <end> [hours/day]  Add range of days off (default: 8)\n"
		<< "  pto remove <date>          Remove entries matching a date (exact match to 'date' or 'start_date')\n"
		<< "  pto show_days_off          Show all saved days off\n"
		<< "  pto show_hrs_on <date>     Show amount of accrued hours on some future date\n"
		<< "  pto usage                  Show this help message\n\n";
}

// TODO: show hours accrued on a date - this can be used to look ahead and know how much vaca time I'll have by
// that date, assuming I don't take any additional time off between now and then.
// Show accrued hours on a specific future date
void show_hrs_on(const json& settings, const json& days_off, const std::string& future_date) {
    std::tm future_date_tm = date_string_to_tm(future_date);

    // should this matter?
    if (!is_weekday(future_date_tm)) {
        std::cerr << "Error: Trying to show a date in the future that is a weekend.\n";
        return;
    }

    // check that it is a date in the future
    if (is_future_date(future_date_tm)) {
        // accrue hours between when i started and then
        std::tm start_tm = date_string_to_tm(settings["start_date"]);
        double accrual_rate = settings["accrual_rate_per_day"];
        double accrued_hours = calculate_accrued_hours_to(days_off, start_tm, future_date_tm, accrual_rate);

        double hours_taken_off = calculate_hours_of_days_off(days_off);
        double hours_available = accrued_hours - hours_taken_off;

        std::cout << "Accrued hours to then: " << format_hrs(hours_available) << " hours.\n";
    }
    else {
        std::cerr << "Error: The date provided is not in the future.\n";
    }
}

// Add a single day off
void add_day_off(json& days_off, const std::string& date, double hours) {
    // don't add a date that already exists
    if (entry_exists(days_off, "date", date)) {
        std::cerr << "Error: A time-off entry for " << date << " already exists.\n";
        return;
    }

    // don't add a weekend day
    std::tm date_tm = date_string_to_tm(date);
    if (!is_weekday(date_tm)) {
        std::cerr << "Error: Trying to add a date that is a weekend.\n";
        return;
    }

    days_off.push_back({ {"date", date}, {"hours", hours} });
    save_days_off(DAYS_OFF_FILE, days_off);
    std::cout << "Added day off: " << date << " (" << hours << "h)\n";
}

// Add a range of days off
void add_range_days_off(json& days_off, const std::string& start, const std::string& end, double hours_per_day) {
    // doesn't handle an overlap. for example if 06/20 is already in the days off and we add
    // 6/19-6/22, its not going to care...
    if (entry_exists(days_off, "date", start) || entry_exists(days_off, "date", end) || 
        entry_exists(days_off, "start_date", start) || entry_exists(days_off, "start_date", end) || 
        entry_exists(days_off, "end_date", start) || entry_exists(days_off, "end_date", end)) {
        std::cerr << "Error: A time-off entry already exists for the start or end date.\n";
        return;
    }

    // don't add a weekend day
    std::tm start_tm = date_string_to_tm(start);
    if (!is_weekday(start_tm)) {
        std::cerr << "Error: Trying to add a start_date that is a weekend.\n";
        return;
    }
    std::tm end_tm = date_string_to_tm(end);
    if (!is_weekday(end_tm)) {
        std::cerr << "Error: Trying to add a end_date that is a weekend.\n";
        return;
    }

    days_off.push_back({
        {"start_date", start},
        {"end_date", end},
        {"hours_per_day", hours_per_day}
    });
    save_days_off(DAYS_OFF_FILE, days_off);
    std::cout << "Added days off: " << start << " to " << end << " (" << hours_per_day << "h/day)\n";
}

// Print the days that have been taken off and a summary
void print_pto_summary(const json& settings, const json& days_off) {
    std::tm start_tm = date_string_to_tm(settings["start_date"]);
    double accrual_rate = settings["accrual_rate_per_day"];
	std::string accrual_rate_str = std::to_string(accrual_rate) + " hours/day";

    double accrued_hours_since_hired = calculate_accrued_hours(days_off, start_tm, accrual_rate);
    int working_days_since_hired = working_days_elapsed_since(start_tm);
	std::string working_days_since_hired_str = std::to_string(working_days_since_hired) + " days";
    double hours_taken_off = calculate_hours_of_days_off(days_off);
    double hours_available = accrued_hours_since_hired - hours_taken_off;

    std::cout << "\n===============================================\n";
    std::cout << "         Paid Time Off Tracker          \n";
    std::cout <<   "===============================================\n\n";

    list_days_off(days_off);

    std::cout << "\n-----------------------------------------------\n";
    std::cout << std::setw(28) << std::left << "Accrual Rate:" 				<< std::setw(15) << std::right << accrual_rate_str << "\n";
    std::cout << std::setw(28) << std::left << "Working Days Since Hired:" << std::setw(15) << std::right << working_days_since_hired_str << "\n";
    std::cout << std::setw(28) << std::left << "Time Accrued:" << std::setw(15) << std::right << format_hrs(accrued_hours_since_hired) << "\n";
    std::cout << std::setw(28) << std::left << "Time Used:" << std::setw(15) << std::right << format_hrs(hours_taken_off) << "\n";
    std::cout << std::setw(28) << std::left << "Time Balance:" << std::setw(15) << std::right << format_hrs(hours_available) << "\n";
    std::cout <<   "-----------------------------------------------\n";

    if (hours_available > 40) {
        std::cout << "\n*** YOU SHOULD TAKE SOME VACATION! ***\n";
    }
    if (hours_available < 0) {
        int days_needed = static_cast<int>(std::ceil(std::abs(hours_available) / accrual_rate));
        std::cout << "\n*** You need to work " << days_needed << " more working day" << (days_needed == 1 ? "" : "s") << " to bring your PTO balance back to zero. ***\n";
    }
    std::cout << std::endl;
}

int main(int argc, char* argv[]) {
	std::cout << std::fixed << std::setprecision(1);

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

	// CLI: show hours on the provided date
	if (argc >= 3 && std::string(argv[1]) == "show_hrs_on") {
		show_hrs_on(settings, days_off, argv[2]);
		return 0;
	}

	// CLI: add single date
	if (argc >= 3 && std::string(argv[1]) == "add") {
		std::string date = argv[2];
		double hours = (argc >= 4) ? std::stod(argv[3]) : 8.0;
		add_day_off(days_off, date, hours);
		return 0;
	}

	// CLI: add date range
	if (argc >= 4 && std::string(argv[1]) == "add_range") {
		std::string start = argv[2];
		std::string end = argv[3];
		double hours_per_day = (argc >= 5) ? std::stod(argv[4]) : 8.0;
		add_range_days_off(days_off, start, end, hours_per_day);
		return 0;
	}

	// CLI: list
	if (argc >= 2 && std::string(argv[1]) == "show_days_off") {
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
	if (argc == 1) {
		print_pto_summary(settings, days_off);
		return 0;
	}
	

	return 0;
}
