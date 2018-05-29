#pragma once

#include <string>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/exception.hpp>

namespace FileSystem {

	using namespace boost::filesystem;

	inline bool createDir(const std::string& path) {
		create_directory(path);
		return true;
	}

	inline int copyFile(const std::string& path_from, const std::string& path_to) {
		boost::system::error_code ec;
		copy_file(path_from, path_to, ec);

		return 1;
	}

	inline bool isExists(const std::string& path) {
		return exists(path);
	}

	inline size_t fileSize(const std::string& path) {
		return file_size(path);
	}
};