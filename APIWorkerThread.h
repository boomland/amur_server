#pragma once
#include <map>
#include <random>

#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include "WorkerThread.h"
#include "Task.h"

#include "RequestErrorCodes.h"
#include "AuthorizedClient.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;


class MainServer;

class APIWorkerThread : public WorkerThread<Task<std::string, std::string>>
{
public:
	APIWorkerThread(size_t idx, MainServer *server, TaskQueue<Task<std::string, std::string>*> *wqueue);
	APIWorkerThread(APIWorkerThread&& r);

	std::string error_response(int re_code) {
		json result = {
			{ "status" , "error" },
			{ "error_code" , re_code },
			{ "error_description" , RE_VALUES.find(re_code)->second }
		};
		return result.dump();
	}

	std::string ok_response(const json& data) {
		json result;
		result["status"] = "ok";
		result["data"] = data;
		return result.dump();
	}

	std::string performJob(Task<std::string, std::string> *task);

	std::string performSetAuthToken(const json & data);

	void getClusterStats(const std::string& tinder_id, std::map<int, std::pair<int, int>>& stat_map);

	void makeDecision(int cluster_idx, std::map<int, std::pair<int, int>>& stat_map, float & prob, std::string & decision);
	
	// yaroslav+
	std::map<int, std::pair<int, int>>::iterator getBadknownCluster(std::map<int, std::pair<int, int>>& stat_map);
	void pushSequencePerson(json& out, sql::ResultSet *res_sql, float& prob);
	void pushDataRandomPerson(json& out, AuthorizedClient* client, int cluster_idx, float& prob, int number_of_people);
	void pushDataPopularPerson(json& out, AuthorizedClient* client, int cluster_idx, float& prob, int start_num, int number_of_people);
	void getRandomPerson(json& out, AuthorizedClient* client, int number_of_people);
	void getPopularPerson(json& out, AuthorizedClient* client, int start_num, int number_of_people);
	int getNumberRecommendationFuction();

	std::string performRecommendation(const json & data);

	std::string performGetEncounters(const json & data);

	std::string performMakeVote(const json & data);

	bool preCheckTinderFormat(const json & data);

	bool setEncounterBaseInfo(std::map<std::string, std::string>& encounter, const json & cur_res);

	bool parsePhotos(json & out, std::vector<std::string>& urls, const json & cur_enc);

	bool parseInstagram(json & out, const json & cur_enc);

	void completeInsertEncounters(
		std::string& sql_enc,
		std::string& sql_geo,
		std::map<std::string, std::string>& enc,
		AuthorizedClient* client);

	void executeInsertion(
		std::string& sql_encs,
		std::string& sql_geo);

	void parseEncounterData(
		json & out,
		std::vector<std::string> & photo_urls,
		std::string & sql_encs, 
		std::string & sql_geo,
		AuthorizedClient * client,
		const json & data);

	std::string processEncounters(AuthorizedClient * client, const json & data, bool isInternal);

private:
	int BATCH_SIZE;
	int NUM_CLUST;
	int BAD_CLUSTER_TRESH;
	float RAND_PROB;
	float POPULAR_PROB;
	float NEAR_PROB;
	std::default_random_engine random_generator;
};

