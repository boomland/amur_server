#include "AuthorizedClient.h"
#include "MainServer.h"
#include "MySQLQueries.cpp"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include <iostream>

namespace MyStrUtils {
	inline void ffs(std::string& str) {
		for (size_t i = 0; i != str.size(); ++i) {
			if (str[i] == ',')
				str[i] = '.';
		}
	}
};

AuthorizedClient::AuthorizedClient(MainServer* server,
	std::string& tinder_id,
	std::string& tinder_auth_token) {

	server_ = server;
	tinder_id_ = tinder_id;
	tinder_auth_token_ = tinder_auth_token;

	client_headers = basic_headers;
	client_headers["X-Auth-Token"] = tinder_auth_token;

	HTTPClient client;
	json result;
	server_->logger.log(et_notice, "Before TinderAPI call");
	int state = TinderAPI::getSelf(client, server->access_log, client_headers, result);
	if (state == 1) {
		throw RequestFormatError(RE_TINDER_JSON_SYNTAX);
	} else if (state == 2) {
		throw RequestFormatError(RE_INCORRECT_TOKEN);
	}
	server_->logger.log(et_notice, result.dump());
	server_->logger.log(et_notice, "TinderAPI call finished!");
	try {
		auto name = result.find("name");
		auto gender = result.find("gender");
		auto gender_filter = result.find("gender_filter");
		auto birthday = result.find("birth_date");
		auto age_min = result.find("age_filter_min");
		auto age_max = result.find("age_filter_max");
		auto distance_filter = result.find("distance_filter");
		auto facebook_id = result.find("facebook_id");
		auto lat = result.at("pos").find("lat");
		auto lon = result.at("pos").find("lon");
		auto city_name = result.at("pos_info").at("city").find("name");
		auto photos = result.find("photos");

		if (name != result.end()
			|| gender != result.end()
			|| gender_filter != result.end()
			|| birthday != result.end()
			|| age_min != result.end()
			|| age_max != result.end()
			|| distance_filter != result.end()
			|| facebook_id != result.end()
			|| lat != result.at("pos").end()
			|| lon != result.at("pos").end()
			|| city_name != result.at("pos_info").at("city").end()
			|| photos != result.end())
		{
			std::string lat_str = std::to_string(lat.value().get<float>());
			std::string lon_str = std::to_string(lon.value().get<float>());
			MyStrUtils::ffs(lat_str);
			MyStrUtils::ffs(lon_str);
			data = {
				{ "tinder_id" , tinder_id },
				{ "tinder_auth_token" , tinder_auth_token },
				{ "name" , name.value().get<string>() },
				{ "gender" , std::to_string(gender.value().get<int>()) },
				{ "gender_filter" , std::to_string(gender_filter.value().get<int>()) },
				{ "birthday" , birthday.value().get<string>() },
				{ "age_min" , std::to_string(age_min.value().get<int>()) },
				{ "age_max" , std::to_string(age_max.value().get<int>()) },
				{ "distance_filter" , std::to_string(distance_filter.value().get<int>()) },
				{ "facebook_id" , facebook_id.value().get<string>() },
				{ "lat" , lat_str },
				{ "lon" , lon_str },
				{ "city_name" , city_name.value().get<string>() }
			};
			// Fill photos list
			size_t photos_count = photos->size();
			photos_.resize(photos_count);
			for (size_t i = 0; i != photos_count; ++i) {
				photos_[i] = photos.value()[i]["url"].get<string>();
				/*
				if (photos.value()[i]["main"].get<bool>() == true) {
					idx_main_photo_ = i;
				}
				*/
			}
			idx_main_photo_ = 0;
			data["main_photo_idx"] = std::to_string(idx_main_photo_);
		} else {
			server_->logger.log(et_error, "RE_TINDER_RESP_FORMAT: " + result.dump());
			throw RequestFormatError(RE_TINDER_RESP_FORMAT);
		}
		
	} catch (json::out_of_range& err) {
		server_->logger.log(et_error, "RE_TINDER_RESP_FORMAT(out_of_range): " + result.dump());
		throw RequestFormatError(RE_TINDER_RESP_FORMAT);
	}
	if (!updateDataBaseInfo()) {
		throw RequestFormatError(RE_MYSQL_ERROR);
	}
}



int AuthorizedClient::updateDataBaseInfo() {
	std::unique_lock<std::mutex> lock(server_->mysql_mutex_);
//	server_->logger.log(et_notice, "Uptdate data base info lock created!");
	try {
		sql::PreparedStatement* st = server_->mysql_con->prepareStatement(Q_FIND_USER.c_str());
		st->setString(1, tinder_id_.c_str());

		sql::ResultSet* res = st->executeQuery();
		if (res->rowsCount() > 0) {
			sql::PreparedStatement* st_upd = server_->mysql_con->prepareStatement(Q_UPDATE_USER.c_str());
			st_upd->setString(1, data["name"].c_str());
			st_upd->setInt(2, std::stoi(data["gender"]));
			st_upd->setInt(3, std::stoi(data["gender_filter"]));
			st_upd->setString(4, data["birthday"].c_str());
			st_upd->setInt(5, std::stoi(data["distance_filter"]));
			st_upd->setInt(6, std::stoi(data["age_min"]));
			st_upd->setInt(7, std::stoi(data["age_max"]));
			st_upd->setString(8, tinder_auth_token_.c_str());
			st_upd->setString(9, tinder_id_.c_str());
			st_upd->execute();
			server_->mysql_con->commit();
			delete st_upd;
		} else {
			sql::PreparedStatement* st_ins = server_->mysql_con->prepareStatement(Q_INSERT_USER.c_str());
			st_ins->setString(1, tinder_id_.c_str());
			st_ins->setString(2, data["facebook_id"].c_str());
			st_ins->setString(3, data["name"].c_str());
			st_ins->setInt(4, std::stoi(data["gender"]));
			st_ins->setInt(5, std::stoi(data["gender_filter"]));
			st_ins->setString(6, data["birthday"].c_str());
			st_ins->setInt(7, std::stoi(data["distance_filter"]));
			st_ins->setInt(8, std::stoi(data["age_min"]));
			st_ins->setInt(9, std::stoi(data["age_max"]));
			st_ins->setString(10, tinder_auth_token_.c_str());
			st_ins->execute();
			server_->mysql_con->commit();
			delete st_ins;
		}
		delete res;
		delete st;

		sql::PreparedStatement *st_geo = server_->mysql_con->prepareStatement(Q_INSERT_GEO.c_str());
		st_geo->setString(1, tinder_id_);
		st_geo->setDouble(2, std::stod(data["lat"]));
		st_geo->setDouble(3, std::stod(data["lon"]));
		st_geo->setString(4, data["city_name"].c_str());
		st_geo->execute();
		server_->mysql_con->commit();
		delete st_geo;

		return 1;
	} catch (sql::SQLException& e) {
		server_->access_log.log(et_error, "SQL error: " + std::string(e.what()));
		return 0;
	}
}