#pragma once

#include <vector>
#include <istream>
#include <valarray>
#include <string>

class MainServer;

class DBClusterizer
{
public:
	DBClusterizer(MainServer* _server);
	void execute();
	void executeID(const std::string& id);

	~DBClusterizer();
private:
	MainServer *server;
};

