#pragma once

#include <vector>
#include <string>

class MainServer;

class ConsoleParser
{
public:
	ConsoleParser(MainServer *main_server);
	
	void parse(const std::vector<std::string>& args);

	void parseExecSQL(const std::vector<std::string>& args);

	void parseGetRecs(const std::vector<std::string>& args);

	void parseClients(const std::vector<std::string>& args);

	void parseCacher(const std::vector<std::string>& args);

	void parseBlackList(const std::vector<std::string>& args);

private:
	MainServer *server;
};

