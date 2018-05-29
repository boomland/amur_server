#include "APIWorkerThread.h"
#include "BaseLogger.h"
#include "AuthorizedClient.h"
#include "MySQLQueries.cpp"
#include "MainServer.h"
#include "ClusterPredictor.h"

#include <string.h>
#include <random> // for uniform_real_distribution, uniform_int_distribution , default_random_engine
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include <boost/algorithm/string.hpp>
#include <valarray>


APIWorkerThread::APIWorkerThread(size_t idx, MainServer *server, TaskQueue<Task<std::string, std::string>*> *wqueue)
	 : WorkerThread(idx, server, wqueue)
{
	FIRST_EPOCH_TRESH = server->config_file_.getSection("recommendation").getProperty<int>("first_tresh", 4);
	BATCH_SIZE = server->config_file_.getSection("recommendation").getProperty<int>("batch_size", 20);
	NUM_CLUST = server->config_file_.getSection("recommendation").getProperty<int>("n_clusters", 100);
	SECOND_EPOCH_RAND_PROB = server->config_file_.getSection("recommendation").getProperty<float>("rand_prob", 0.3);
	SECOND_EPOCH_POPULAR_PROB = server->config_file_.getSection("recommendation").getProperty<float>("popular_prob", 0.5);
	SECOND_EPOCH_NEAR_PROB = server->config_file_.getSection("recommendation").getProperty<float>("neigh_prob", 0.2);
}

APIWorkerThread::APIWorkerThread(APIWorkerThread&& r) : WorkerThread(std::forward<WorkerThread>(r)) {
	FIRST_EPOCH_TRESH = r.FIRST_EPOCH_TRESH;
	BATCH_SIZE = r.BATCH_SIZE;
	NUM_CLUST = r.NUM_CLUST;
	SECOND_EPOCH_RAND_PROB = r.SECOND_EPOCH_RAND_PROB;
	SECOND_EPOCH_POPULAR_PROB = r.SECOND_EPOCH_POPULAR_PROB;
	SECOND_EPOCH_NEAR_PROB = r.SECOND_EPOCH_NEAR_PROB;
}

std::string APIWorkerThread::performJob(Task<std::string, std::string> *job) {
	json request;
	try {
		request = json::parse(job->getContent());
	} catch (json::parse_error& err) {
		//MainServer->acess_log.log(et_error, );
		return error_response(RE_JSON_SYNTAX);
	}

	// Check if action defined in request
	std::string action;
	if (request.find("action") == request.end()) {
		return error_response(RE_NO_ACTION);
	} else {
		action = request["action"];
	}

	if (server_->black_list.find(request["tinder_id"].get<std::string>()) != server_->black_list.end()) {
		server_->access_log.log(et_notice,
			"Sucessfully blocked request from " + request["tinder_id"].get<std::string>()
			+ ": " + request["action"].get<std::string>());
		return error_response(RE_INTERNAL);
	}

	if (action == "SET_AUTH_TOKEN") {
		return performSetAuthToken(request);
	} else if (action == "GET_ENCOUNTERS") {
		return performGetEncounters(request);
	} else if (action == "MAKE_VOTE") {
		return performMakeVote(request);
	} else if (action == "GET_RECS") {
		return performRecommendation(request);
	} else {
		return error_response(RE_ACTION_UNKNOWN);
	}
}

/*
SET_AUTH_TOKEN
Description: Authorize function
Request format:
1) string tinder_id
2) string tinder_auth_token
Response format:
Returns profile of authorized person

*/

std::string APIWorkerThread::performSetAuthToken(const json& data) {
	std::string id, token;
	try {
		id = data.at("tinder_id");
		token = data.at("tinder_auth_token");
	} catch (json::out_of_range& err) {
		return error_response(RE_MISSED_ARGS);
	}
	try {
		AuthorizedClient* client = new AuthorizedClient(server_, id, token);
		int state = server_->addClient(client);
		server_->access_log.log(et_notice,
			"Sucessfully authorized: " + client->getID() + "(" + client->getData()["name"] + ")");
		json resp = client->getData();
		resp["photos"] = client->getPhotos();
		return ok_response(resp);
	} catch (RequestFormatError err) {
		return error_response(err.err_code_);
	} catch (std::exception err) {
		return error_response(RE_INTERNAL);
	}
}

void APIWorkerThread::getClusterStats(const std::string& tinder_id, std::map<int, std::pair<int, int>>& stat_map) {
	sql::PreparedStatement *pst = server_->mysql_con->prepareStatement(Q_CLUST_STATS.c_str());
	pst->setString(1, tinder_id.c_str());
	sql::ResultSet *res = pst->executeQuery();
	stat_map.clear();
	while (res->next()) {
		int cluster_idx = res->getInt(1);
		int positive = res->getInt(2);
		int overall = res->getInt(3);
		stat_map[cluster_idx] = {positive, overall};
	}
	delete pst;
	delete res;
}

void APIWorkerThread::makeDecision(
	int cluster_idx,
	std::map<int, std::pair<int, int>>& stat_map,
	float& prob,
	std::string& decision)
{
	const int cur_positive = stat_map[cluster_idx].first;
	const int cur_overall = stat_map[cluster_idx].second;

	if (cur_overall == 0) {
		decision = "EXPLORE";
		prob = 0.0f;
		return;
	}
	prob = (float)cur_positive / (float)cur_overall;
	if (cur_positive < 4) {
		decision = "EXPLORE";
		return;
	}
	if (prob < 0.3f) {
		decision = "PASS";
	} else {
		decision = "ACCEPT";
	}
}



std::map<int, std::pair<int, int>>::iterator APIWorkerThread::getBadknownCluster(std::map<int, std::pair<int, int>>& stat_map) {
	for (auto it = stat_map.begin(); it != stat_map.end(); ++it) {
		// [num_cluster]->{positive, overall};
		int cur_cluster_idx = it->first;
		int cur_overall = it->second.second;
		// если мы нашли кластер в котором оценили меньше FIRST_EPOCH_TRESH
		// человек, тогда предложим человека из этого кластера
		if (cur_overall <= FIRST_EPOCH_TRESH) {
			return it;
		}
	}
	return stat_map.end();
}

void APIWorkerThread::pushDataRandomPerson(json& out, AuthorizedClient* client, int cluster_idx, float& prob, int number_of_people = 1) {
	std::cout << "\t I am in pushDataRandomPerson!\n";
	sql::PreparedStatement *pst = server_->mysql_con->prepareStatement(Q_FILTER_v3.c_str());
	pst->setString(1, (client->getID()).c_str());
	pst->setInt(2, (std::atoi(client->getData()["gender"].c_str()) + 1) % 2);
	pst->setInt(3, std::atoi(client->getData()["age_min"].c_str()));
	pst->setInt(4, std::atoi(client->getData()["age_max"].c_str()));
	pst->setString(5, client->getData()["city_name"].c_str());
	pst->setInt(6, cluster_idx); // выбираем откуда хотим получить
	pst->setInt(7, number_of_people); // хотим получить пока что одного юзера
	sql::ResultSet *res = pst->executeQuery();
	server_->logger.log(et_notice, "Num rows in Q_FILTER_v3: " + std::to_string(res->rowsCount()));
	while (res->next()) {
		std::string tinder_id = res->getString("tinder_id").c_str();
		std::string name = res->getString("name").c_str();
		int gender = res->getInt("gender");
		std::string birth_date = res->getString("birth_date").c_str();
		int distance = 999;
		int cluster_idx = res->getInt("cluster_idx");
		std::string bio = "AS the best man in computer vision!";//"Bio information will appear in future versions! Sorry :(";

		std::string decision = "NO_DECISION";
		prob = 99.99;

		sql::PreparedStatement *pst_photos = server_->mysql_con->prepareStatement(Q_GET_PHOTOS_URLS.c_str());
		pst_photos->setString(1, tinder_id.c_str());
		sql::ResultSet *res_photos = pst_photos->executeQuery();
		json cur = {
			{ "id", tinder_id },
			{ "name", name },
			{ "gender", gender },
			{ "birthday", birth_date },
			{ "distance", distance },
			{ "bio", bio },
			{ "prob", prob},
			{ "decision", decision },
			{ "cluster", cluster_idx }
		};
		while (res_photos->next()) {
			cur["photos"].push_back({
				{ "url", res_photos->getString("url").c_str() },
				{ "type", "tinder" }
			});
		}
		//server_->logger.log(et_notice, "Cur dump: " + cur.dump());
		out.push_back(cur);
		delete pst_photos;
		delete res_photos;
	}
}

#include <iostream>

void APIWorkerThread::pushDataPopularPerson(json& out, AuthorizedClient* client, int cluster_idx, float& prob, int start_num = 0, int number_of_people = 1) {
	// вставить в cpp запросник
	std::cout << "\t I am in pushDataPopularPerson! ";
	std::cout << cluster_idx << " " <<  start_num << " " << number_of_people << '\n';
	sql::PreparedStatement *pst = server_->mysql_con->prepareStatement(Q_FILTER_v4.c_str());
	pst->setInt(1, cluster_idx);
	pst->setString(2, client->getID().c_str());
	pst->setInt(3, start_num);
	pst->setInt(4, number_of_people);
	sql::ResultSet *res = pst->executeQuery();
	server_->logger.log(et_notice, "Num rows in Q_FILTER_v4: " + std::to_string(res->rowsCount()));
	while (res->next()) {
		std::string tinder_id = res->getString("tinder_id").c_str();
		std::string name = res->getString("name").c_str();
		int gender = res->getInt("gender");
		std::string birth_date = res->getString("birth_date").c_str();
		int distance = 999;
		int cluster_idx = res->getInt("cluster_idx");
		std::string bio = "AS the best man in computer vision!";//"Bio information will appear in future versions! Sorry :(";

		std::string decision = "NO_DECISION";
		prob = 99.99;

		sql::PreparedStatement *pst_photos = server_->mysql_con->prepareStatement(Q_GET_PHOTOS_URLS.c_str());
		pst_photos->setString(1, tinder_id.c_str());
		sql::ResultSet *res_photos = pst_photos->executeQuery();
		json cur = {
			{ "id", tinder_id },
			{ "name", name },
			{ "gender", gender },
			{ "birthday", birth_date },
			{ "distance", distance },
			{ "bio", bio },
			{ "prob", prob},
			{ "decision", decision },
			{ "cluster", cluster_idx }
		};
		while (res_photos->next()) {
			cur["photos"].push_back({
				{ "url", res_photos->getString("url").c_str() },
				{ "type", "tinder" }
			});
		}
		//server_->logger.log(et_notice, "Cur dump: " + cur.dump());
		out.push_back(cur);
		delete pst_photos;
		delete res_photos;
	}
}

// void APIWorkerThread::pushDataNeighborPerson(json& out, Authorized* client, );

void APIWorkerThread::getRandomPerson(json& out, AuthorizedClient* client, int number_of_people = 1) {
	std::map<int, std::pair<int, int>> stat_map;
	getClusterStats(client->getID(), stat_map);
	auto it = getBadknownCluster(stat_map);
	float probability_random = 0.0f;
	if (it != stat_map.end()) {
		// малоизведанный кластер
		pushDataRandomPerson(out, client, it->first, probability_random, number_of_people);
	} else {
		// если нет берем рандомный
		std::default_random_engine generator;
 		std::uniform_int_distribution<int> distribution(0, NUM_CLUST);
	 	int cur_cluster_idx = distribution(generator);
	 	pushDataRandomPerson(out, client, cur_cluster_idx, probability_random, number_of_people);
	}
	return;
}

void APIWorkerThread::getPopularPerson(json& out, AuthorizedClient* client, int start_num = 0, int number_of_people = 1) {
	std::default_random_engine generator;
 	std::uniform_int_distribution<int> distribution(0, NUM_CLUST);

 	int cur_cluster_idx = distribution(generator);
 	float probability_popular = 0.0f;
 	pushDataPopularPerson(out, client, cur_cluster_idx, probability_popular, start_num, number_of_people);
 	return;
}

// void APIWorkerThread::getNeighborsPerson(json& out, AuthorizedClient* client, int start_num = 0, int number_of_people){};
int APIWorkerThread::getNumberRecommendationFuction() {
	std::random_device r;
	std::default_random_engine generator{r()};
 	std::uniform_real_distribution<float> distribution(0.0, 1.0);

 	float rand_value = distribution(generator);

 	if (rand_value <= SECOND_EPOCH_RAND_PROB) {
 		return 0;
 	} else if (rand_value <= SECOND_EPOCH_RAND_PROB + SECOND_EPOCH_POPULAR_PROB) {
 		return 1;
 	} else {
 		return 2;
 	}
}

std::string APIWorkerThread::performRecommendation(const json& data) {
	string id;
	try {
		id = data.at("tinder_id");
	} catch (json::out_of_range& err) {
		return error_response(RE_MISSED_ARGS);
	}
	AuthorizedClient* client = server_->getClient(id);
	if (client != nullptr) {
		if (data.find("votes_data") != data.end()) {
			std::string vote_resp = performMakeVote(data);
			if (vote_resp != ok_response({})) {
				return vote_resp;
			}
		}
		int t_gender = (std::atoi(client->getData()["gender"].c_str()) + 1) % 2;
		json out;
		int number_of_people = 1;
		int start_num = 0; // for "1-popular" method
		while (out.size() < BATCH_SIZE) {
			std::cout << "Size: " << out.size() << '\n';
			int number_func = getNumberRecommendationFuction();
			if (number_func == 0) { // 0 -random
				cout << "153_RANDOM\n";
				getRandomPerson(out, client, number_of_people);
			} else if (number_func == 1) { // 1 - popular
				cout << "153_POPULAR\n";
				getPopularPerson(out, client, start_num, number_of_people);
			} else { // 2 - neighbors
				cout << "153_NEIGH\n";
				// getNeighborsPerson(out, ....)
			}
		}
		std::cout << "I have 20 people!\n";
		return ok_response(out);
		try {

		} catch (sql::SQLException& err) {
			server_->logger.log(et_error, "SQL Exception: " + std::string(err.what()));
		} catch (std::exception& err) {
			server_->logger.log(et_error, "Unknown exception: " + std::string(err.what()));
		}
		return "";
	} else {
		return error_response(RE_NO_AUTH);
	}
}


// std::string APIWorkerThread::getRandomPerson(const json& data) {
// 	string id = data.at("tinder_id"); // получаем id текущего юзера

// }

// Моя, новая версия


std::string APIWorkerThread::performGetEncounters(const json& data) {
	string id, token;
	try {
		id = data.at("tinder_id");
		token = data.at("tinder_auth_token");
	} catch (json::out_of_range& err) {
		return error_response(RE_MISSED_ARGS);
	}
	AuthorizedClient* client = server_->getClient(id);
	if (client != nullptr) {
		if (data.find("votes_data") != data.end()) {
			std::string vote_resp = performMakeVote(data);
			if (vote_resp != ok_response({})) {
				return vote_resp;
			}
		}
		HTTPClient http_client;
		json recs;
		int state = TinderAPI::getRecs(http_client, server_->access_log, client->getHeaders(), recs);
		if (state != 0) {
			return error_response(RE_TINDER_ACTION_FAIL);
		}
		if (data.find("internal") != data.end()) {
			return processEncounters(client, recs, true);
		}
		return processEncounters(client, recs, false);
	} else {
		return error_response(RE_NO_AUTH);
	}
}


std::string APIWorkerThread::performMakeVote(const json& data) {
	string id, token;
	try {
		id = data.at("tinder_id");
		token = data.at("tinder_auth_token");
	} catch (json::out_of_range& err) {
		return error_response(RE_MISSED_ARGS);
	}

	AuthorizedClient* client = server_->getClient(id);
	if (client != nullptr) {
		try {
			auto votes_data = data.find("votes_data");
			if (votes_data == data.end()) {
				return error_response(RE_MISSED_ARGS);
			}
			HTTPClient http_client;
			json resp;
			size_t num_votes = votes_data.value().size();
			if (num_votes == 0) {
				return error_response(RE_MISSED_ARGS);
			}
			std::string bd_query = "INSERT INTO marks(client_tinder_id, person_id, vote) VALUES ";
			for (size_t i = 0; i != num_votes; ++i) {
				std::string cur_id = votes_data.value()[i]["id"].get<std::string>();
				int cur_vote = votes_data.value()[i]["vote"].get<int>();

				if (i != 0) {
					bd_query += ", ";
				}
				bd_query += "('" + id + "', '" + cur_id + "', " + std::to_string(cur_vote) + ")";
			}
			//perform bd_query
			try {
				server_->mysql_mutex_.lock();
				sql::Statement* st = server_->mysql_con->createStatement();
				st->execute(bd_query.c_str());
				server_->mysql_con->commit();
				server_->mysql_mutex_.unlock();
			} catch (sql::SQLException& exc) {
				server_->access_log.log(et_error,
					"MySQL exception at performVote: " + std::string(exc.what()));
				return error_response(RE_MYSQL_ERROR);
			} catch (std::exception& exc) {
				server_->logger.log(et_error,
					"Std::exception at performVote: " + std::string(exc.what()));
				return error_response(RE_INTERNAL);
			}
		} catch (json::out_of_range& err) {
			return error_response(RE_MISSED_ARGS);
		}
	} else {
		return error_response(RE_NO_AUTH);
	}
	return ok_response({});
}


bool APIWorkerThread::preCheckTinderFormat(const json& data) {
	if (data.find("data") == data.end()
		|| data["data"].find("results") == data["data"].end())
	{
		return false;
	}
	return true;
}

#include "boost/date_time/gregorian/gregorian.hpp"
bool APIWorkerThread::setEncounterBaseInfo(
	std::map<std::string, std::string>& encounter,
	const json& cur_enc)
{
	using namespace boost::gregorian;
	try {
		std::string birth_date = cur_enc["user"]["birth_date"].get<std::string>().substr(0, 10);
		encounter = {
			{ "id", cur_enc["user"]["_id"].get<std::string>() },
			{ "gender", std::to_string(cur_enc["user"]["gender"].get<int>()) },
			{ "bio", cur_enc["user"]["bio"].get<std::string>() },
			{ "birthday", birth_date },
			{ "name", cur_enc["user"]["name"].get<std::string>() }
		};

		date date_now(day_clock::local_day());
		std::vector<std::string> parts;
		boost::split(parts, birth_date, boost::is_any_of("-"));
		int year = std::stoi(parts[0]);
		int month = std::stoi(parts[1]);
		int day = std::stoi(parts[2]);
		date date_birth(year, month, day);
		days days_alive = date_now - date_birth;
		int age = days_alive.days() / 365;
		encounter["age"] = std::to_string(age);

		auto dist_it = cur_enc.find("distance_mi");
		if (dist_it != cur_enc.end()) {
			encounter["distance"] = std::to_string(dist_it.value().get<int>());
		} else {
			encounter["distance"] = "NaN";
		}
	} catch (json::out_of_range& err) {
		return false;
	}
	return true;
}

bool APIWorkerThread::parsePhotos(
	json& out,
	std::vector<std::string>& urls,
	const json& cur_enc)
{
	try {
		auto photos = cur_enc["user"]["photos"];
		size_t num_photos = photos.size();
		for (size_t j = 0; j != num_photos; ++j) {
			std::string url = photos[j]["url"].get<std::string>();
			out[out.size() - 1]["photos"].push_back({
				{ "url" , url },
				{ "type" ,  "tinder" }
			});
			urls.push_back(url);
		}
	} catch (json::out_of_range& err) {
		return false;
	}
	return true;
}

bool APIWorkerThread::parseInstagram(json& out, const json& cur_enc) {
	try {
		auto inst_it = cur_enc.find("instagram");
		if (inst_it != cur_enc.end()) {
			out[out.size() - 1]["instagram"] = inst_it.value()["username"].get<std::string>();
		}
	} catch (std::exception& err) {
		return false;
	}
	return true;
}

void APIWorkerThread::completeInsertEncounters(
	std::string& sql_enc,
	std::string& sql_geo,
	std::map<std::string, std::string>& enc,
	AuthorizedClient* client)
{
	sql_enc +=
		"('" + enc["id"] + "', '"
		+ enc["name"] + "', "
		+ enc["gender"] + ", '"
		+ enc["birthday"] + "',"
		 + enc["age"] + ",'" + enc["city"] + "'),";
	if (enc["distance"] != "NaN") {
		sql_geo +=
			"('" + enc["id"] + "', "
			+ client->getData()["lat"] + ", "
			+ client->getData()["lon"] + ", "
			+ enc["distance"] + "),";
	}
}

void APIWorkerThread::executeInsertion(
	std::string& sql_encs,
	std::string& sql_geo)
{
	sql_encs[sql_encs.size() - 1] = ' ';
	sql_geo[sql_geo.size() - 1] = ' ';

	sql::Statement* st = server_->mysql_con->createStatement();
	st->execute(sql_encs.c_str());
	st->execute(sql_geo.c_str());
	server_->mysql_con->commit();
	delete st;
}

void APIWorkerThread::parseEncounterData(
	json& out,
	std::vector<std::string>& photo_urls,
	std::string& sql_encs,
	std::string& sql_geo,
	AuthorizedClient* client,
	const json& data)
{
	auto results = data["data"]["results"];
	size_t num_enc = results.size();
	for (size_t i = 0; i != num_enc; ++i) {
		std::map<std::string, std::string> encounter;
		setEncounterBaseInfo(encounter, results[i]);
		encounter["city"] = client->getData()["city_name"];
		out.push_back(encounter);
		parsePhotos(out, photo_urls, results[i]);
		parseInstagram(out, results[i]);
		completeInsertEncounters(sql_encs, sql_geo, encounter, client);
	}
}


std::string APIWorkerThread::processEncounters(AuthorizedClient* client, const json& data, bool isInternal) {
	try {
		server_->logger.log(et_notice, "API call finished. Started parsing and MySQL work.");
		std::unique_lock<std::mutex> lock(server_->mysql_mutex_);
		std::string sql_encs = Q_INSERT_CACHE_ENCOUNTER;
		std::string sql_geo = Q_INSERT_CACHE_GEO;
		json out;
		std::vector<std::string> photo_urls;

		if (!preCheckTinderFormat(data))
			return error_response(RE_TINDER_RESP_FORMAT);

		std::vector<std::string> out_ids;
		parseEncounterData(out, photo_urls, sql_encs, sql_geo, client, data);

		if (!isInternal) {
			return ok_response(out);
		} else {
			json result;
			server_->storage_manager->processURLs(photo_urls, result);
			executeInsertion(sql_encs, sql_geo);
			server_->logger.log(et_notice, "MySQL and parsing finished!");
			return ok_response(result);
		}
	} catch (json::out_of_range& err) {
		return error_response(RE_TINDER_RESP_FORMAT);
	} catch (sql::SQLException& err) {
		server_->logger.log(et_error, "MySQL error cache encounters: " + std::string(err.what()));
		return error_response(RE_MYSQL_ERROR);
	} catch (std::exception& err) {
		server_->logger.log(et_error, "Internal: " + std::string(err.what()));
		return error_response(RE_INTERNAL);
	}
}
