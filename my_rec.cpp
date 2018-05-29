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
		std::map<int, std::pair<int, int>> stat_map;
		getClusterStats(client->getID(), stat_map);
		try {
			sql::PreparedStatement *pst = server_->mysql_con->prepareStatement(Q_FILTER_v2.c_str());
			pst->setString(1, id.c_str());
			pst->setInt(2, t_gender);
			pst->setInt(3, std::atoi(client->getData()["age_min"].c_str()));
			pst->setInt(4, std::atoi(client->getData()["age_max"].c_str()));
			pst->setString(5, client->getData()["city_name"].c_str());
			pst->setInt(6, 20);
			sql::ResultSet *res = pst->executeQuery();
			server_->logger.log(et_notice, "Num rows in Q_FILTER_v2: " + std::to_string(res->rowsCount()));
			while (res->next()) {
				std::string tinder_id = res->getString("tinder_id").c_str();
				std::string name = res->getString("name").c_str();
				int gender = res->getInt("gender");
				std::string birth_date = res->getString("birth_date").c_str();
				int distance = 999;
				int cluster_idx = res->getInt("cluster_idx");
				std::string bio = "Bio information will appear in future versions! Sorry :(";

				std::string decision = "NO_DECISION";
				float probability = 0.0f;
				makeDecision(cluster_idx, stat_map, probability, decision);

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
					{ "prob", probability },
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
			return ok_response(out);

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

