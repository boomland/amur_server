#include "MainServer.h"
#include "FileSystem.cpp"
#include "boost/filesystem.hpp"

MainServer::MainServer(size_t _num_workers)
	: logger("logs/main.log", true),
	access_log("logs/access.log", true),
	console_parser(this),
	work_queue(),
	http_handler(this),
	dbc(this)
{
	initializeConfig(_num_workers);
	initializeWorkers();
	initializeMySQL();
	initializeHTTP();
	initializeNGROK();

	storage_manager = new StorageResourceManager(this, "cache/", 100);
	cacher = new BackgroundCacher(this, config_file_.getSection("cacher").getProperty<int>("delay", 30000));
	//cacher->start();
	if (config_file_.getSection("cacher").getProperty<int>("start_on_run", 0) == 0) {
		cacher->pause();
	}

	size_t num_clusters = config_file_.getSection("clusters").getProperty<int>("n_clusters", 100);

	cluster_predictor = new ClusterPredictor<float>(
		num_clusters,
		"cache/cluster_centers.dat");

	std::ifstream in_bl("blacklist.txt", std::ios::in);
	string cur_id;
	while (std::getline(in_bl, cur_id)) {
		black_list.insert(cur_id);
	}
	in_bl.close();

	logger.log(EventType::et_ok, "Server sucessfully initialized!");

}

MainServer::~MainServer() {
	if (fd_server != nullptr) {
		fd_server->destroy();
	}
	if (ngrok != nullptr) {
		ngrok->destroy();
	}
	HTTPClient client;
	client.performPostRequest("https://dating-ml.ru/set_server.php", "none", {});
}

void MainServer::soft_exit() {
	for (int i = 0; i != worker_pool_.size(); ++i) {
		worker_pool_[i].stop();
		logger.log(et_ok, "Sucessfully stopped worker #" + std::to_string(i + 1));
	}
	for (int i = 0; i != http_worker_pool_.size(); ++i) {
		http_worker_pool_[i].stop();
	}
	for (int i = 0; i != gpu_worker_pool_.size(); ++i) {
		gpu_worker_pool_[i].stop();
	}
	http_server->stop();
	logger.log(et_ok, "Sucessfully stopped http server!");
	http_thread.join();
	logger.log(et_ok, "Sucessfully stopped http_thread!");
}

void MainServer::loop() {
	std::string cmd;
	while (true) {
		getline(std::cin, cmd);
		std::vector<std::string> args;
		boost::split(args, cmd, boost::is_any_of(" "));
		console_parser.parse(args);
	}
}

void MainServer::initializeNGROK() {
	if (config_file_.getSection("fd_server").getProperty<int>("use", 0) == 1) {
		fd_server = new LaunchWrapper(
			config_file_.getSection("fd_server")
				.getProperty<std::string>("python_exec", "python"),
			{ config_file_.getSection("fd_server").getProperty<std::string>("path", "") },
			config_file_.getSection("fd_server").getProperty<int>("visible", 1));
		logger.log(et_ok, "FD_server now is running!");
	}
	if (config_file_.getSection("ngrok").getProperty<int>("use", 0) == 1) {
		std::vector<std::string> ngrok_params = { "http", std::to_string(http_port_) };
		std::string authtoken =
			config_file_.getSection("ngrok").getProperty<std::string>("authtoken", "");
		if (authtoken != "") {
			ngrok_params.push_back("authtoken");
			ngrok_params.push_back(authtoken);
		}
		ngrok = new LaunchWrapper(
			config_file_.getSection("ngrok").getProperty<std::string>("path", "ngrok"),
			ngrok_params,
			config_file_.getSection("ngrok").getProperty<int>("visible", 0)
		);
		logger.log(et_ok, "NGROK now is running!");

		HTTPClient client;
		size_t tunnels_count = 0;
		string url;
		while (tunnels_count == 0) {
			std::string resp = client.performGetRequest("http://localhost:4040/api/tunnels", {});
			json data = json::parse(resp);
			if (data["tunnels"].size() != 0) {
				for (auto it = data["tunnels"].begin(); it != data["tunnels"].end(); ++it) {
					auto addr = it->at("config").find("addr").value().get<string>();
					if (addr == ("localhost:" + std::to_string(http_port_))) {
						url = it->find("public_url").value().get<string>();
						tunnels_count++;
						break;
					}
				}
			}
		}
		logger.log(et_ok, "Found NGROK public url: " + url);
		logger.log(et_ok, "Found NGROK public url: " + url); 
  		std::string instance_name = config_file_.getSection("main").getProperty<std::string>("instance", ""); 
  		std::string website_url = "https://dating-ml.ru/set_server.php"; 
  		if (instance_name != "") { 
   			website_url += "?instance=" + instance_name; 
  		} 
  		string resp = client.performPostRequest(website_url, url, {});
		if (resp == url) {
			logger.log(et_ok, "Sucessfully saved URL at https://dating-ml.ru");
		} else {
			logger.log(et_fatal, "Error saving URL at https://dating-ml.ru!");
			throw std::exception();
		}
	}
}

void MainServer::initializeHTTP() {
	HTTPHandler::server::options options(http_handler);
	options.thread_pool(
		std::make_shared<boost::network::utils::thread_pool>(num_workers));
	http_server = new HTTPHandler::server(options
		.address("0.0.0.0")
		.port(std::to_string(http_port_))
	);
	http_thread = std::thread([&] { http_server->run(); });
	logger.log(et_notice, "It seems like HTTP server has been runned :3");
}

void MainServer::initializeWorkers() {
	int api_workers_num = config_file_.getSection("main").getProperty<int>("api_workers", 8);
	int http_workers_num = config_file_.getSection("main").getProperty<int>("http_workers", 8);
	int gpu_workers_num = config_file_.getSection("main").getProperty<int>("gpu_workers", 1);

	logger.log(et_notice, "Workers| "
		"API: " + std::to_string(api_workers_num) +
		". HTTP: " + std::to_string(http_workers_num) +
		". GPU: " + std::to_string(gpu_workers_num));

	// ---- API Workers
	for (size_t i = 0; i != api_workers_num; ++i)
		worker_pool_.push_back(APIWorkerThread(i, this, getWorkQueue()));
	for (size_t i = 0; i != api_workers_num; ++i)
		worker_pool_[i].start();
	// ---- HTTP Workers
	for (size_t i = 0; i != http_workers_num; ++i)
		http_worker_pool_.push_back(HTTPWorkerThread(i, this, &http_queue));
	for (size_t i = 0; i != http_workers_num; ++i)
		http_worker_pool_[i].start();
	// ---- GPU Workers
	for (size_t i = 0; i != gpu_workers_num; ++i)
		gpu_worker_pool_.push_back(GPUWorker(i, this, &gpu_queue));
	for (size_t i = 0; i != gpu_workers_num; ++i)
		gpu_worker_pool_[i].start();
}


void MainServer::initializeConfig(size_t _num_workers) {
	try {
		config_file_ = INIReader("config.ini");
	}
	catch (std::exception& error) {
		logger.log(et_fatal, error.what());
		throw std::exception();
	}
	last_work_id = 0;
	num_workers = _num_workers;
	if (num_workers == 0) {
		num_workers = config_file_.getSection("main").getProperty<int>("workers", 4);
	}
	http_port_ = config_file_.getSection("main").getProperty<int>("port", 8880);
	logger.log(et_notice,
		"Starting server with "
			+ std::to_string(num_workers) + " on port " + std::to_string(http_port_));

}

void MainServer::initializeMySQL() {
	std::string mysql_server = config_file_.getSection("mysql").getProperty<std::string>("ip", "127.0.0.1");
	std::string mysql_port = config_file_.getSection("mysql").getProperty<std::string>("port", "3306");
	std::string mysql_user = config_file_.getSection("mysql").getProperty<std::string>("user", "root");
	std::string mysql_password = config_file_.getSection("mysql").getProperty<std::string>("password", "root");
	std::string mysql_db = config_file_.getSection("mysql").getProperty<std::string>("db", "dating2");


	logger.log(et_notice,
		"Connecting to tcp://" + mysql_server + ":" + mysql_port + "/"
		+ mysql_db + "@" + mysql_user + " with password=" + mysql_password);
	try {
		mysql_driver = get_driver_instance();
		std::string server = "tcp://" + mysql_server + ":" + mysql_port;
		mysql_con = mysql_driver->connect(server.c_str(), mysql_user.c_str(), mysql_password.c_str());
		mysql_con->setSchema(mysql_db.c_str());
	}
	catch (sql::SQLException err) {
		std::string err_explain = err.what();
		logger.log(et_fatal, "Connection to database error: " + err_explain);
		throw std::exception();
	}
}

TaskQueue<MainServer::TextTask*> *MainServer::getWorkQueue() {
	return &work_queue;
}

TaskQueue<Task<HTTPTaskParams, std::string>*>* MainServer::getHTTPQueue()
{
	return &http_queue;
}

TaskQueue<Task<GPUTaskParams, std::string>*>* MainServer::getGPUQueue()
{
	return &gpu_queue;
}

AuthorizedClient *MainServer::getClient(const std::string& id) {
	std::unique_lock<std::mutex> lock(clients_mutex);
	auto c = clients.find(id);
	if (clients.find(id) != clients.end()) {
		return c->second;
	} else {
		return nullptr;
	}
}

int MainServer::addClient(AuthorizedClient* client) {
	std::unique_lock<std::mutex> lock(clients_mutex);
	if (clients.find(client->getID()) == clients.end()) {
		clients.insert(make_pair(client->getID(), client));
		return 1;
	} else {
		delete clients[client->getID()];
		clients[client->getID()] = client;
		return 0;
	}
}

std::map<std::string, AuthorizedClient*>* MainServer::getClientMap()
{
	return &clients;
}
