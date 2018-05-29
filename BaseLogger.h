#pragma once

#include <map>
#include <fstream>
#include <iostream>
#include <iomanip>
#include <ctime>
#include <time.h>
#include <string>
#include <mutex>
#include "TermColor.hpp"

enum EventType {
	et_notice,
	et_warning,
	et_error,
	et_fatal,
	et_ok
};

static std::map<EventType, std::string> event_strings = {
	{ et_notice, "NOTICE" },
	{ et_warning, "WARNING" },
	{ et_error, "ERROR" },
	{ et_fatal, "FATAL" },
	{ et_ok, "OK"}
};

class BaseLogger {
public:
	BaseLogger(const std::string& filename, int print_stdout) {
		need_stdout_ = print_stdout;
		log_file = std::ofstream(filename, std::ofstream::out | std::ofstream::app);
		if (!log_file.is_open()) {
			log(et_fatal, "Can not open log file!");
		}
	}

	int log(EventType et, std::string message) {
		std::unique_lock<std::mutex> lock(mut_);

        std::time_t now = std::time(NULL);
        std::tm * ptm = std::localtime(&now);
        char buffer[32];
        // Format: Mo, 15.06.2009 20:20:00
        std::strftime(buffer, 32, "%d-%m-%Y %H:%M:%S:", ptm);

		#ifdef __linux__
			timeval curTime;
			gettimeofday(&curTime, NULL);
			int ms = curTime.tv_usec / 1000;
		#endif
		#ifdef _WIN32
			SYSTEMTIME time;
			GetSystemTime(&time);
			LONG ms = time.wMilliseconds;
		#endif

		if (need_stdout_) {
			std::cout << '[' << buffer
				<< ms << "][";
			switch (et) {
			case et_notice:
				std::cout << termcolor::white;
				break;
			case et_ok:
				std::cout << termcolor::green;
				break;
			case et_warning:
				std::cout << termcolor::yellow;
				break;
			case et_error:
				std::cout << termcolor::magenta;
				break;
			case et_fatal:
				std::cout << termcolor::red;
				break;
			}
			std::cout << event_strings[et] << termcolor::reset << "] "
				<< message << std::endl;
		}
		log_file << '[' << buffer
			<< ms << "][" << event_strings[et] << "] "
			<< message << std::endl;
		return 0;
	}

	~BaseLogger() {
		log_file << "------------------------" << std::endl;
		log_file.close();
	}


private:
	int need_stdout_;
	std::ofstream log_file;
	std::mutex mut_;
};

