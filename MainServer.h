#pragma once

#include <iostream>
#include <vector>
#include <string>
#include <fstream>


#include "Task.h"
#include "TaskQueue.h"
#include "APIWorkerThread.h"
#include "HTTPWorkerThread.h"
#include "GPUWorker.h"
#include "FileSystem.cpp"
#include "BackgroundCacher.h"
#include "ClusterPredictor.h"
#include "DBClusterizer.h"
#include "ConsoleParser.h"

#include "INIReader.h"
#include "HTTPHandler.h"
#include "AuthorizedClient.h"

#include "mysql_connection.h"

#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>

#include "BaseLogger.h"

#include "LaunchWrapper.h"

#include "StorageResourceManager.h"
#include "MySQLQueries.cpp"


using std::string;
using std::vector;
using std::cout;
using std::endl;

class MainServer {
public:
	size_t last_work_id;
	typedef Task<string, string> TextTask;

	BaseLogger logger;
	BaseLogger access_log;
	StorageResourceManager* storage_manager;
	BackgroundCacher* cacher;
	ClusterPredictor<float>* cluster_predictor;
	DBClusterizer dbc;
	std::set<std::string> black_list;
	LaunchWrapper *ngrok;
	LaunchWrapper *fd_server;

	sql::Driver *mysql_driver;
	sql::Connection *mysql_con;
	std::mutex mysql_mutex_;

	INIReader config_file_;

	MainServer(size_t _num_workers = 0);
	~MainServer();

	void soft_exit();
	void loop();

	TaskQueue<TextTask*> *getWorkQueue();
	TaskQueue<Task<HTTPTaskParams, std::string>*> *getHTTPQueue();
	TaskQueue<Task<GPUTaskParams, std::string>*> *getGPUQueue();

	std::map<std::string, AuthorizedClient*> *getClientMap();
	AuthorizedClient *getClient(const std::string& id);
	int addClient(AuthorizedClient* client);

private:
	void initializeConfig(size_t _num_workers);
	void initializeMySQL();
	void initializeWorkers();
	void initializeHTTP();
	void initializeNGROK();

	ConsoleParser console_parser;

	HTTPHandler http_handler;
	HTTPHandler::server* http_server;
	std::thread http_thread;

	TaskQueue<TextTask*> work_queue;
	TaskQueue<Task<HTTPTaskParams, std::string>*> http_queue;
	TaskQueue<Task<GPUTaskParams, std::string>*> gpu_queue;

	std::vector<APIWorkerThread> worker_pool_;
	std::vector<HTTPWorkerThread> http_worker_pool_;
	std::vector<GPUWorker> gpu_worker_pool_;
	std::vector<TextTask*> tasks;

	std::map<std::string, AuthorizedClient*> clients;
	std::mutex clients_mutex;

	size_t num_workers;
	size_t http_port_;
};

