#include "MainServer.h"
#include "StorageResourceManager.h"
#include "StorageResource.h"
#include "FileSystem.cpp"

#include "MainServer.h"

#include <boost/algorithm/string.hpp>


StorageResourceManager::StorageResourceManager(
	MainServer* main_server, const std::string& cache_path, int num_partitions)
	:server(main_server), last_res_id(0), partitions(num_partitions)
{
	server->logger.log(et_notice, "Initializing of StorageResourceManager...");
	/* TODO: Initialization */
	base_cache_path = cache_path;
	for (int i = 0; i != num_partitions; ++i) {
		string cur = cache_path + "encounters/cache-" + std::to_string(i);
		if (!FileSystem::isExists(cur)) {
			FileSystem::createDir(cur);
		}
	}
	server->logger.log(et_ok, "StorageResourceManager initialized!");
}


StorageResourceManager::~StorageResourceManager()
{
	server->logger.log(et_notice, "Destroying StorageResourceManager...");
	/*TODO: Destroy manager */
	server->logger.log(et_ok, "Destroyed StorageResourceManager");
}

StorageResource * StorageResourceManager::getResource(int id)
{
	std::unique_lock<std::mutex> lock(mut);
	auto it = idx_map.find(id);
	if (it != idx_map.end()) {
		return it->second;
	} else {
		return nullptr;
	}
}

StorageResource * StorageResourceManager::getResource(const std::string& virtual_path)
{
	std::unique_lock<std::mutex> lock(mut);
	auto it = virtual_map.find(virtual_path);
	if (it != virtual_map.end()) {
		return it->second;
	} else {
		return nullptr;
	}
}

int StorageResourceManager::addResource(const std::string & vpath, StorageResource ** entity)
{
	return addResource(last_res_id + 1, vpath, entity);
}

int StorageResourceManager::addResource(int id, const std::string & vpath, StorageResource ** entity)
{
	std::unique_lock<std::mutex> lock(mut);
	if (idx_map.find(id) != idx_map.end()) {
		server->logger.log(et_error, "Attempt to create resource with existing ID = " + std::to_string(id) + "!");
		return -1;
	}
	if (virtual_map.find(vpath) != virtual_map.end()) {
		server->logger.log(et_error, "Attempt to create resource with existing vpath = " + vpath);
		return -1;
	}
	int new_id = id;
	last_res_id = id;

	std::vector<std::string> parsed;
	boost::split(parsed, vpath, boost::is_any_of("/"));

	std::string real_path = base_cache_path;
	if (parsed[0] == "enc") {
		real_path += "encounters/";
	} else if (parsed[0] == "usr") {
		real_path += "users/";
	} else {
		return -1;
	}

	int part = hasher(parsed[1]) % partitions;
	real_path += "cache-" + std::to_string(part) + "/" + parsed[1] + "/";
	if (!FileSystem::isExists(real_path)) {
		FileSystem::createDir(real_path);
	}
	real_path += parsed[2] + "_";
	std::vector<std::string> file_n;
	boost::split(file_n, parsed[3], boost::is_any_of("@"));
	real_path += file_n[0] + "." + file_n[1];

	//server->logger.log(et_notice, "Created new StorageResource #" + std::to_string(new_id) + ": " + real_path);

	StorageResource *cur = new StorageResource(new_id, vpath, real_path);
	idx_map[new_id] = cur;
	virtual_map[vpath] = cur;

	if (entity != nullptr)
		*entity = cur;
	return new_id;
}

void StorageResourceManager::processURLs(std::vector<std::string>& urls, json& result)
{
	std::map<std::string, std::vector<ImageInfo>> data;
	for (int i = 0; i != urls.size(); ++i) {
		std::vector<std::string> items;
		boost::algorithm::split(items, urls[i], boost::is_any_of("/"));
		std::string tinder_id = items[items.size() - 2];
		std::string img_id_size = items[items.size() - 1];
		size_t pos = img_id_size.find("_");
		ImageInfo img_inf;
		img_inf.tinder_id = tinder_id;
		img_inf.img_size = "";
		img_inf.url = urls[i];
		if (pos != std::string::npos) {
			img_inf.img_id = img_id_size.substr(pos + 1);
			img_inf.img_size = img_id_size.substr(0, pos);
		} else {
			img_inf.img_id = img_id_size;
		}
		data[tinder_id].push_back(img_inf);
	}
	std::string sql = "SELECT tinder_id FROM encounters_cache WHERE tinder_id IN (";
	for (auto& it : data)
		sql += "'" + it.first + "',";
	sql[sql.size() - 1] = ')';
	sql::Statement *st = server->mysql_con->createStatement();
	sql::ResultSet *res = st->executeQuery(sql.c_str());
	while (res->next()) {
		std::string current_id = res->getString("tinder_id").c_str();
		data.erase(current_id);
	}
	delete res;
	delete st;
	st = server->mysql_con->createStatement();
	res = st->executeQuery(Q_CACHE_PHOTOS_LAST_IDX.c_str());
	res->next();
	int last_idx = res->getInt(1);
	sql = Q_INSERT_CACHE_PHOTOS;

	for (auto& it : data) {
		for (int i = 0; i != it.second.size(); ++i) {
			std::string img_id = it.second[i].img_id;
			std::string vpath = "enc/" + it.first + "/full/" + img_id.substr(0, img_id.size() - 4) + "@jpg";
			if (addResource(last_idx, vpath, nullptr) == -1) {
				server->logger.log(et_error, "Couldn`t create resource with vpath: " + vpath);
			} else {
				last_idx += 1;
				result.push_back({
					{ "res_id", last_idx },
					{ "url", it.second[i].url },
					{ "tinder_id", it.first }
				});
				sql += "(" + std::to_string(last_idx)
					+ ", '" + it.first + "', '"
					+ it.second[i].img_id + "', '"
					+ it.second[i].img_size + "'),";
			}
		}
	}
	sql[sql.size() - 1] = ' ';
	st->execute(sql.c_str());
	delete st;
	delete res;
}

int StorageResourceManager::freeResource(int id)
{
	return freeResource(getResource(id));
}



int StorageResourceManager::freeResource(const std::string & vpath)
{
	return freeResource(getResource(vpath));
}

int StorageResourceManager::freeResource(StorageResource * cur)
{
	std::unique_lock<std::mutex> lock(mut);

	if (cur == nullptr) {
		return -1; // no such StorageResource
	}

	if (cur->checkValid() == 0) {
		return -2; // not a valid StorageResource
	}

	std::string virtual_path = cur->getVirtualPath();
	int cur_id = cur->getID();

	if (cur->free() < 0) {
		return -3; //free error
	}
	delete cur;
	idx_map.erase(cur_id);
	virtual_map.erase(virtual_path);

	return 0;
}