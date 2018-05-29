#include "LaunchWrapper.h"
#include <iostream>


LaunchWrapper::LaunchWrapper() {}

LaunchWrapper::LaunchWrapper(const std::string & filename, const std::vector<std::string>& args, bool visible) {
#ifdef __linux__
	pid = fork();
	if (!pid) {
        std::vector<char *> tmp(args.size());
        for (int i = 0; i != args.size(); ++i) {
            tmp[i] = const_cast<char*>(args[i].c_str());
        }
		char **params = &tmp[0];
		execvp(filename.c_str(), params);
		_exit(1);
	}
#endif
#ifdef _WIN32
	std::wstring arg_str = L"";
	std::wstring filename_str(filename.begin(), filename.end());
	for (size_t i = 0; i != args.size(); ++i) {
		arg_str += std::wstring(args[i].begin(), args[i].end());
		if (i != args.size() - 1) {
			arg_str += L" ";
		}
	}
	shExInfo = { 0 };
	shExInfo.cbSize = sizeof(shExInfo);
	shExInfo.fMask = SEE_MASK_NOCLOSEPROCESS;
	shExInfo.hwnd = 0;
	shExInfo.lpVerb = L"open";
	shExInfo.lpFile = filename_str.c_str();
	shExInfo.lpParameters = arg_str.c_str();
	std::wcout << L"Program: " << std::wstring(filename.begin(), filename.end()).c_str() << L". Arguments: " << arg_str << std::endl;
	shExInfo.lpDirectory = 0;
	if (visible) {
		shExInfo.nShow = SW_NORMAL;
	} else {
		shExInfo.nShow = SW_HIDE;
	}
	shExInfo.hInstApp = 0;
	ShellExecuteEx(&shExInfo);
#endif
}

LaunchWrapper::LaunchWrapper(LaunchWrapper && r)
{
#ifdef __linux__
	pid = r.pid;
#endif
#ifdef _WIN32
	shExInfo = r.shExInfo;
#endif
}

void LaunchWrapper::destroy() {
#ifdef __linux__
	kill(pid, SIGTERM);
	waitpid(pid, NULL, 0);
#endif
#ifdef _WIN32
	if (shExInfo.hProcess) {
		TerminateProcess(shExInfo.hProcess, 0);
		CloseHandle(shExInfo.hProcess);
	}
#endif
}


LaunchWrapper::~LaunchWrapper() {
	destroy();
}
