#pragma once

#include <map>
#include "HTTPClient.h"
#include "TinderApi.cpp"
#include "RequestErrorCodes.h"
#include "RequestFormatError.h"

static const std::map<std::string, std::string> basic_headers = {
	{ "app_version", "6.9.4" },
	{ "platform" , "ios" },
	{ "content-type" , "application/json" },
	{ "User-agent" , "Tinder/7.5.3 (iPhone; iOS 10.3.2; Scale/2.00)" }
};

class MainServer;

class AuthorizedClient {
public:
	AuthorizedClient(MainServer* server,
		std::string& tinder_id,
		std::string& tinder_auth_token);

	std::map<std::string, std::string>& getData() {
		return data;
	}

	std::string getID() const {
		return tinder_id_;
	}
	
	std::string getToken() const {
		return tinder_auth_token_;
	}

	const std::vector<std::string>& getPhotos() const {
		return photos_;
	}

	const std::map<std::string, std::string>& getHeaders() const {
		return client_headers;
	}

	int updateDataBaseInfo();

private:
	MainServer *server_;
	std::string tinder_id_;
	std::string tinder_auth_token_;
	std::map<std::string, std::string> data;
	std::map<std::string, std::string> client_headers;
	std::vector<std::string> photos_;
	size_t idx_main_photo_;
};

