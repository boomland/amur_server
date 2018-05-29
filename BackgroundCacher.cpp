#include "BackgroundCacher.h"
#include "MainServer.h"
#include <chrono>
#include <set>
#include "cppconn/prepared_statement.h"
#include <nlohmann/json.hpp>
using json = nlohmann::json;

json BackgroundCacher::callGetPeople(const AuthorizedClient* a_client)
{
	main_server->logger.log(et_notice, "Selected online user: " + a_client->getID());
	json main_request = {
		{ "action", "GET_ENCOUNTERS" },
		{ "tinder_id", a_client->getID() },
		{ "tinder_auth_token", a_client->getToken() },
		{ "internal", "true" }
	};
	Task<std::string, std::string>* main_task = new Task<std::string, std::string>(1,
		main_request.dump());
	main_server->getWorkQueue()->push(main_task);
	main_task->waitFor();
	return json::parse(main_task->getResult());
}

std::vector<StorageResource*> BackgroundCacher::parseMainResponse(const json& main_response, std::set<std::string>& out_ids) {
	std::vector<StorageResource*> res_arr;
	std::vector<Task<HTTPTaskParams, std::string>*> tasks;
	size_t num_photos = main_response["data"].size();
	main_server->logger.log(et_notice, "Download started. Num_photos: " + std::to_string(num_photos));

	for (size_t i = 0; i != num_photos; ++i) {
		int id = main_response["data"][i]["res_id"].get<int>();
		std::string url = main_response["data"][i]["url"].get<std::string>();
		std::string tinder_id = main_response["data"][i]["tinder_id"].get<std::string>();
		out_ids.insert(tinder_id);
		StorageResource* new_res = main_server->storage_manager->getResource(id);
		if (new_res != nullptr) {
                    res_arr.push_back(new_res);
		    auto* cur_task = new Task<HTTPTaskParams, std::string>(1, { url, id });
		    tasks.push_back(cur_task);
		    main_server->getHTTPQueue()->push(cur_task);
		}
	}
	int sz = res_arr.size();
	for (int i = 0; i != sz; ++i) {
		tasks[i]->waitFor();
		delete tasks[i];
	}
	main_server->logger.log(et_notice, "Download finished. Sucessfully acquired " + std::to_string(sz) + " images!");
	return res_arr;
}

void BackgroundCacher::gpuFacePipe(const std::vector<StorageResource*>& res_arr) {
	std::vector<int> res_ids(res_arr.size());
	//main_server->logger.log(et_notice, "Downloaded " + std::to_string(res_arr.size()) + " photos");
	for (int i = 0; i != res_arr.size(); ++i)
		res_ids[i] = res_arr[i]->getID();
	Task<GPUTaskParams, std::string> *task =
		new Task<GPUTaskParams, std::string>(0, { cnn_face_detect, res_ids });
	main_server->getGPUQueue()->push(task);
	task->waitFor();
	std::vector<int> final_ids;
	for (int i = 0; i != res_ids.size(); ++i)
		if (res_arr[i]->is_single_detected == 1)
			final_ids.push_back(res_ids[i]);
	delete task;
	main_server->logger.log(et_notice, "Detecting face finished. Single-face images: "
		+ std::to_string(final_ids.size()));
	task = new Task<GPUTaskParams, std::string>(0, { gpu_descriptor, final_ids });
	main_server->getGPUQueue()->push(task);
	task->waitFor();
	delete task;
}

#include <fstream>

void BackgroundCacher::saveFaceDetection(const std::vector<StorageResource*>& res_arr) {
	std::string query = "INSERT INTO face_detection(res_id, left_c, right_c, top_c, bottom_c) VALUES ";
	for (int i = 0; i != res_arr.size(); ++i) {
		query += "(" + std::to_string(res_arr[i]->getID()) + ", ";
		if (res_arr[i]->is_single_detected) {
			query += std::to_string(res_arr[i]->face_det.left()) + ", "
				+ std::to_string(res_arr[i]->face_det.right()) + ", "
				+ std::to_string(res_arr[i]->face_det.top()) + ", "
				+ std::to_string(res_arr[i]->face_det.bottom()) + "),";
		} else {
			query += "0, 0, 0, 0),";
		}
	}
	query[query.size() - 1] = ' ';
	sql::Statement *st = main_server->mysql_con->createStatement();
	st->execute(query.c_str());
	delete st;
	main_server->logger.log(et_notice, "Face detection saved.");
}

void BackgroundCacher::saveFaceDescriptors(const std::vector<StorageResource*>& res_arr) {
	std::ifstream in("cache/descr_cache.tmp", std::ios::in | std::ios::binary);

	std::vector<StorageResource*> final_res;
	for (int i = 0; i != res_arr.size(); ++i) {
		if (res_arr[i]->is_single_detected == 1)
			final_res.push_back(res_arr[i]);
	}

	int file_size = final_res.size() * 4 * 2048;
	char *read_buffer = new char[file_size];
	in.read(read_buffer, file_size);

	for (int i = 0; i != final_res.size(); ++i) {
		int id = final_res[i]->getID();
		int from = i * 4 * 2048;
		sql::PreparedStatement *pst =
			main_server->mysql_con->prepareStatement(
				"insert into face_descriptors(res_id, data) VALUES (?, ?)"
			);
		std::string str(read_buffer + from, 4 * 2048);
		std::istringstream iss(str);
		pst->setInt(1, id);
		pst->setBlob(2, &iss);
		pst->execute();
		delete pst;
	}
}

void BackgroundCacher::saveResources(const std::vector<StorageResource*>& res_arr) {
	try {
		std::unique_lock<std::mutex> lock(main_server->mysql_mutex_);
		// insert into face_det
		saveFaceDetection(res_arr);
		saveFaceDescriptors(res_arr);
		main_server->mysql_con->commit();
		main_server->logger.log(et_notice, "Saving face detection and descriptors complete!");
	} catch (sql::SQLException& err) {
		main_server->logger.log(et_error, "MySQL Error saving face detection/descriptors: " + std::string(err.what()));
	} catch (std::exception& err) {
		main_server->logger.log(et_error, "Internal error saving face detection/descriptors: " + std::string(err.what()));
	}
}

void BackgroundCacher::saveClusters(const std::set<std::string>& out_ids) {
	for (auto& id : out_ids) {
		main_server->dbc.executeID(id);
	}
	main_server->logger.log(et_ok, "Clusters saved, pipe finished!");
}

void BackgroundCacher::Run() {
	main_server->logger.log(et_notice,
		"Started background encounters processing with min_interval = "
		+ std::to_string(min_delay) + "ms");
	auto last_attempt = std::chrono::high_resolution_clock::now();
	auto client_it = main_server->getClientMap()->begin();
	while (!need_stop) {
		//sleepUntil(last_attempt, min_delay);
		last_attempt = std::chrono::high_resolution_clock::now();
		if (need_pause)
			continue;
		if (main_server->getClientMap()->size() == 0)
			continue;
		if (client_it == main_server->getClientMap()->end())
			client_it = main_server->getClientMap()->begin();

		std::string current_id = client_it->first;
		json main_response = callGetPeople(client_it->second);
		std::vector<StorageResource*> res_arr;
		std::set<std::string> out_ids;
		if (main_response["status"].get<std::string>() != "ok") {
			main_server->logger.log(
				et_error,
				"BackgroundCacher| GET_ENCOUNTERS: "
					+ main_response["error_description"].get<std::string>()
			);
			continue;
		} else {
			res_arr = parseMainResponse(main_response, out_ids);
		}
		gpuFacePipe(res_arr);
		main_server->logger.log(et_notice, "Descriptor calculation finished!");
		saveResources(res_arr);
		saveClusters(out_ids);
		client_it++;
		if (client_it == main_server->getClientMap()->end())
			client_it = main_server->getClientMap()->begin();
	}
}
