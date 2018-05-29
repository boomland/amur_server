#pragma once

#include <string>
#include <vector>

#ifdef __linux__
	#include <unistd.h>
	#include <sys/types.h>
	#include <sys/wait.h>
	#include <signal.h>
#endif
#ifdef _WIN32
	#include <Windows.h>
	#include <shellapi.h>
#endif

class LaunchWrapper
{
public:
	LaunchWrapper();

	LaunchWrapper(const std::string& filename, const std::vector<std::string>& args, bool visible);
	LaunchWrapper(LaunchWrapper&& r);
	void destroy();

	~LaunchWrapper();

private:
	#ifdef __linux__
		pid_t pid;
	#endif
	#ifdef _WIN32
		SHELLEXECUTEINFO shExInfo;
	#endif
};

