static unsigned hash_str(const char* s, unsigned mod) {
	#define A 54059
	#define B 76963
	#define C 86969
	#define FIRSTH 37

	unsigned h = FIRSTH;
	while (*s) {
		h = (h * A) ^ (s[0] * B);
		s++;
	}
	return h % mod;
}


#include <string>
#include <sstream>
#include <vector>
#include <iterator>

template<typename Out>
static void split(const std::string &s, char delim, Out result) {
	std::stringstream ss(s);
	std::string item;
	while (std::getline(ss, item, delim)) {
		*(result++) = item;
	}
}

static std::vector<std::string> split(const std::string &s, char delim) {
	std::vector<std::string> elems;
	split(s, delim, std::back_inserter(elems));
	return elems;
}