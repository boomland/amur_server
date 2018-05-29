#pragma once
#include <string.h>
#include <map>

const int RE_INTERNAL = 300;			// Internal server error
const int RE_MYSQL_ERROR = 301;			// Internal MySQL query error

const int RE_JSON_SYNTAX = 400;			// Incorrect JSON syntax
const int RE_NO_ACTION = 401;			// No action defined in query
const int RE_ACTION_UNKNOWN = 402;		// Action in query unknown
const int RE_MISSED_ARGS = 403;			// Some arguments in query are absent

const int RE_INCORRECT_TOKEN = 501;		// Incorrect token specified in query
const int RE_NO_AUTH = 502;				// Client doesn`t authorized on Amur server

const int RE_TINDER_INTERNAL = 600;		// Tinder responsed internal error
const int RE_TINDER_ACTION_FAIL = 601;	// Tinder action failed
const int RE_TINDER_JSON_SYNTAX = 602;  // Tinder response syntax error
const int RE_TINDER_RESP_FORMAT = 603;  // Tinder response don`t correspond expectations 

const static std::map<int, std::string> RE_VALUES = {
	{ RE_INTERNAL, "Internal server error" },
	{ RE_MYSQL_ERROR, "Internal MySQL query error" },
	{ RE_JSON_SYNTAX, "Incorrect JSON syntax" },
	{ RE_NO_ACTION, "No action defined in query" },
	{ RE_ACTION_UNKNOWN, "Action in query unknown" },
	{ RE_MISSED_ARGS, "Some arguments in query are absent" },
	{ RE_INCORRECT_TOKEN, "Incorrect token specified in query" },
	{ RE_NO_AUTH, "Client doesn`t authorized on Amur server" },
	{ RE_TINDER_INTERNAL, "Tinder responsed internal error" },
	{ RE_TINDER_ACTION_FAIL, "Tinder action failed" },
	{ RE_TINDER_JSON_SYNTAX, "Tinder response syntax error" },
	{ RE_TINDER_RESP_FORMAT, "Tinder response don`t correspond expectations" }
};