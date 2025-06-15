#define _CRT_SECURE_NO_WARNINGS

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <ctime>
#include <chrono>
#include <json.hpp>

using json = nlohmann::json;
using namespace std;
using namespace std::chrono;

// Parses a string like "2024-01-01" into a std::tm struct
std::tm parse_date(const std::string& date_str) {
	std::tm tm = {};
	sscanf(date_str.c_str(), "%d-%d-%d", &tm.tm_year, &tm.tm_mon, &tm.tm_mday);
	tm.tm_year -= 1900;
	tm.tm_mon -= 1;
	return tm;
}

// Returns the number of weekdays between two dates
int count_weekdays(const std::tm& start_tm, const std::tm& end_tm) {
	std::time_t start = std::mktime(const_cast<std::tm*>(&start_tm));
	std::time_t end = std::mktime(const_cast<std::tm*>(&end_tm));
	int days = 0;

	for (std::time_t t = start; t <= end; t += 86400) {
		std::tm* current_tm = std::localtime(&t);
		int wday = current_tm->tm_wday;
		if (wday != 0 && wday != 6) // skip Sunday(0) and Saturday(6)
			++days;
	}
	return days;
}

double calculate_used_hours(const json& days_off) {
	double used_hours = 0.0;

	for (const auto& entry : days_off) {
		if (entry.contains("date")) {
			// Single-day entry
			if (entry.contains("hours"))
				used_hours += entry["hours"].get<double>();
			else
				used_hours += 8.0;
		}
		else if (entry.contains("start_date") && entry.contains("end_date")) {
			std::tm start_tm = parse_date(entry["start_date"]);
			std::tm end_tm = parse_date(entry["end_date"]);
			double hours_per_day = entry.contains("hours_per_day") ? entry["hours_per_day"].get<double>() : 8.0;

			std::time_t start = std::mktime(&start_tm);
			std::time_t end = std::mktime(&end_tm);

			for (std::time_t t = start; t <= end; t += 86400) {
				std::tm* current_tm = std::localtime(&t);
				int wday = current_tm->tm_wday;
				if (wday != 0 && wday != 6) // skip weekends
					used_hours += hours_per_day;
			}
		}
	}

	return used_hours;
}


int main() {
	// Read settings.json
	std::ifstream settings_file("settings.json");
	if (!settings_file) {
		std::cerr << "Failed to open settings.json\n";
		return 1;
	}
	json settings;
	settings_file >> settings;

	std::tm start_tm = parse_date(settings["start_date"]);
	double accrual_rate = settings["accrual_rate_per_day"];

	// Get today's date
	std::time_t now = std::time(nullptr);
	std::tm* today_tm = std::localtime(&now);

	// Calculate accrued hours
	int workdays = count_weekdays(start_tm, *today_tm);
	double accrued_hours = workdays * accrual_rate;

	// Read days_off.json
	std::ifstream days_off_file("days_off.json");
	if (!days_off_file) {
		std::cerr << "Failed to open days_off.json\n";
		return 1;
	}
	json days_off;
	days_off_file >> days_off;

	double used_hours = calculate_used_hours(days_off);
	double available_hours = accrued_hours - used_hours;

	std::cout << "Available PTO hours: " << available_hours << std::endl;
	return 0;
}