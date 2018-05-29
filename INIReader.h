#pragma once

#include <string>
#include <map>
#include <stdexcept>
#include <fstream>
#include <algorithm>
#include <cctype>
#include <sstream>
#include <iostream>

static inline void ltrim(std::string &s) {
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](int ch) {
		return !std::isspace(ch);
	}));
}

static inline void rtrim(std::string &s) {
	s.erase(std::find_if(s.rbegin(), s.rend(), [](int ch) {
		return !std::isspace(ch);
	}).base(), s.end());
}

static inline void trim(std::string &s) {
	ltrim(s);
	rtrim(s);
}

class INISection {
public:
	INISection() {}

	void setProperty(const std::string key, const std::string value) {
		data[key] = value;
	}

	template<typename T>
	T getProperty(const std::string key, T default_value) const {
		if (data.find(key) == data.end()) {
			return default_value;
		}
		std::string val = data.at(key);
		std::istringstream ss(val);
		T result = default_value;
		ss >> result;
		return result;
	}

//private:
	std::map<std::string, std::string> data;
};
class INIReader
{
public:
	INIReader() {}

	INIReader(const std::string filename) {
		std::ifstream ini_file(filename);
		if (!ini_file.is_open()) {
			throw std::runtime_error("Could not open INI file");
		}
		std::string current_section = "default";
		while (!ini_file.eof()) {
			std::string cur;
			std::getline(ini_file, cur);
			if (cur[0] == ';')
				continue;
			size_t left_br = cur.find('[');
			if (left_br != std::string::npos) {
				size_t right_br = cur.find(']');
				std::string section_name = cur.substr(left_br + 1, right_br - left_br - 1);
				current_section = section_name;
			} else {
				size_t eq_pos = cur.find('=');
				std::string key = cur.substr(0, eq_pos);
				trim(key);
				std::string val = cur.substr(eq_pos + 1, std::string::npos);
				trim(val);

				sections[current_section].setProperty(key, val);
			}
		}
	}

	void printDebug() {
		for (auto& sec : sections) {
			std::cout << "[" << sec.first << "]" << std::endl;
			for (auto& opt : sec.second.data) {
				std::cout << "--- " << opt.first << " = " << opt.second << std::endl;
			}
		}
	}

	/* TODO: Runtime when no sush section */
	INISection& getSection(std::string section_name) {
		if (sections.find(section_name) == sections.end()) {
			return default_section;
		}
		return sections[section_name];
	}

	~INIReader() {}

private:
	std::map<std::string, INISection> sections;
	INISection default_section;
};

