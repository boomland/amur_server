/* Tinder API functions */

#include <nlohmann/json.hpp>
#include "HTTPClient.h"
#include "BaseLogger.h"
#include <iostream>
#include <chrono>


using json = nlohmann::json;


namespace TinderAPI {
	static const std::string tinder_base_url = "https://api.gotinder.com";

	inline int basicReqest(
		const std::string& url,
		const std::string& api_name,
		HTTPClient& client,
		BaseLogger& access_logger,
		const HTTPClient::HeaderMap& headers,
		json& result)
	{	
		std::string resp = client.performGetRequest(tinder_base_url + url, headers);
		if (resp.find("Unauthorized") != std::string::npos) {
			return 2;
		}
		try {
			result = json::parse(resp);
		} catch (json::parse_error& error) {
			access_logger.log(et_error, "[" + api_name + " API] Tinder returned incorrect JSON: " + resp);
			return 1;
		}
		return 0;
	}

	inline int getSelf(HTTPClient& client, BaseLogger& access_logger, const HTTPClient::HeaderMap& headers, json& result) {
		return basicReqest("/profile", "Profile", client, access_logger, headers, result);
	}

	inline int getRecs(HTTPClient& client, BaseLogger& access_logger, const HTTPClient::HeaderMap& headers, json& result) {
		return basicReqest("/v2/recs/core?locale=en-US", "RecsV2", client, access_logger, headers, result);
	}

	inline int getPerson(const std::string& id, HTTPClient& client, BaseLogger& access_logger, const HTTPClient::HeaderMap& headers, json& result) {
		return basicReqest("/user/" + id, "getPerson", client, access_logger, headers, result);
	}

	inline int makeLike(const std::string& id, HTTPClient& client, BaseLogger& access_logger, const HTTPClient::HeaderMap& headers, json& result) {
		return basicReqest("/like/" + id, "makeLike", client, access_logger, headers, result);
	}

	inline int makePass(const std::string& id, HTTPClient& client, BaseLogger& access_logger, const HTTPClient::HeaderMap& headers, json& result) {
		return basicReqest("/pass/" + id, "makePass", client, access_logger, headers, result);
	}
}
