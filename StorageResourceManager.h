#pragma once
#include "StorageResource.h"
#include <boost/functional/hash.hpp>

class MainServer;

struct ImageInfo {
	std::string tinder_id;
	std::string img_id;
	std::string img_size;
	std::string url;
};

class StorageResourceManager
{
public:
	StorageResourceManager(MainServer* main_server, const std::string& cache_path, int num_partitions);
	~StorageResourceManager();

	// getting resources
	StorageResource* getResource(int id);
	StorageResource* getResource(const std::string& virtual_path);
	// adding resources
	int addResource(const std::string & vpath, StorageResource ** entity);
	int addResource(int id, const std::string & vpath, StorageResource ** entity);
	void processURLs(std::vector<std::string>& urls, json& result);
	// free resource
	int freeResource(int id);
	int freeResource(const std::string& virtual_path);
	int freeResource(StorageResource* cur);
	
	int getLastResID() const {
		return last_res_id;
	}

private:

	MainServer* server;
	std::string base_cache_path;
	std::map<int, StorageResource*> idx_map;
	std::unordered_map<std::string, StorageResource*> virtual_map;
	std::mutex mut;

	boost::hash<std::string> hasher;
	
	int last_res_id;
	int partitions;
};

