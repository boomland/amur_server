#pragma once
#include <thread>
#include <vector>
#include <set>

#include "AuthorizedClient.h"
#include "StorageResource.h"

#include <nlohmann/json.hpp>
using json = nlohmann::json;


class MainServer;

class BackgroundCacher
{
public:
	BackgroundCacher(MainServer *server, int ms_interval)
		: main_server(server), min_delay(ms_interval) {}

	BackgroundCacher(BackgroundCacher&& r)
		: wthread(std::move(r.wthread)), main_server(r.main_server) {}

	void start() {
		wthread = std::thread(&BackgroundCacher::Run, this);
	}

	~BackgroundCacher() {
		need_stop = true;
		if (wthread.joinable()) {
			wthread.join();
		}
	}

	void stop() {
		need_stop = true;
	}

	void pause() {
		need_pause = true;
	}

	void resume() {
		need_pause = false;
	}

	void setInterval(int ms) {
		min_delay = ms;
	}

private:
	void sleepUntil(std::chrono::steady_clock::time_point& prev, int wait_ms) {
		while (1) {
			/*auto now = std::chrono::high_resolution_clock::now();
			std::chrono::duration<double, std::milli> fp_ms = now - prev;*/
			int passed = 999999/*fp_ms.count()*/;
			if (passed < wait_ms) {
				std::this_thread::sleep_for(std::chrono::milliseconds(wait_ms - passed));
			} else {
				break;
			}
		}
	}
	void Run();
	json callGetPeople(const AuthorizedClient* a_client);
	std::vector<StorageResource*> parseMainResponse(const json& main_response, std::set<std::string>& out_ids);
	void gpuFacePipe(const std::vector<StorageResource*>& res_arr);
	void saveFaceDetection(const std::vector<StorageResource*>& res_arr);
	void saveFaceDescriptors(const std::vector<StorageResource*>& res_arr);
	void saveResources(const std::vector<StorageResource*>& res_arr);
	void saveClusters(const std::set<std::string>& out_ids);

	MainServer *main_server;
	std::thread wthread;

	bool need_stop = false;
	bool need_pause = false;
	int min_delay;
};

