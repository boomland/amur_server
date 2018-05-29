#pragma once


#include <exception>
#include <string>
#include "RequestErrorCodes.h"

class RequestFormatError : public std::exception {

public:
	int err_code_;

	virtual const char *what() const throw() {
		return RE_VALUES.find(err_code_)->second.c_str();
	}

	RequestFormatError(int err_code) {
		err_code_ = err_code;
	}
};