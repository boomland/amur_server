#include "ConsoleParser.h"
#include "MainServer.h"
#include "PrettyTable.h"

ConsoleParser::ConsoleParser(MainServer *main_server) : server(main_server) {}

void ConsoleParser::parse(const std::vector<std::string>& args)
{
	std::string command = args[0];
	if (command == "exit") {
		server->soft_exit();
		_exit(0);
	} else if (command == "cls") {
		system("cls");
	} else if (command == "cacher") {
		parseCacher(args);
	} else if (command == "reclusterize") {
		server->dbc.execute();
	} else if (command == "blacklist") {
		parseBlackList(args);
	} else if (command == "clients") {
		parseClients(args);
	} else if (command == "get_recs") {
		parseGetRecs(args);
	} else if (command == "koshunin") {
		server->logger.log(et_notice, "Dobrii дзен, Anton Sergeevich!");
	} else if (command == "SQL") {
		parseExecSQL(args);
	} else {
		server->logger.log(et_notice, "Unrecognized command: " + command);
	}
}

void ConsoleParser::parseExecSQL(const std::vector<std::string>& args) {
	std::string query;
	for (int i = 2; i != args.size(); ++i) {
		query += args[i] + " ";
	}
	try {
		sql::Statement *st = server->mysql_con->createStatement();
		if (args[1] == "no-result") {
			st->execute(query.c_str());
			server->mysql_con->commit();
		} else {
			sql::ResultSet *res = st->executeQuery(query.c_str());
			server->logger.log(et_notice, "Rows count: " + std::to_string(res->rowsCount()));
			auto data = res->getMetaData();
			int cols = data->getColumnCount();
			std::vector<PrettyColumn> columns;
			for (int i = 0; i != cols; ++i) {
				columns.push_back({ std::string(data->getColumnName(i).c_str()), data->getColumnDisplaySize(i), AT_LEFT });
			}
			PrettyTable table("SQL Result", columns);
			while (res->next()) {
				/* Coming soon */
			}
			server->logger.log(et_notice, "\n" + table.dump());
		}
	} catch (sql::SQLException& exc) {
		server->logger.log(et_error, "SQL Exception: " + std::string(exc.what()));
	} catch (std::exception& exc) {
		server->logger.log(et_error, "Some exception" + std::string(exc.what()));
	}
}

void ConsoleParser::parseGetRecs(const std::vector<std::string>& args) {
	if (args.size() < 2) {
		server->logger.log(et_notice, "Syntax: get_recs [client_id]");
		return;
	}
	HTTPClient h;
	json req = {
		{ "action", "GET_RECS" },
		{ "tinder_id", args[1] },
		{ "tinder_auth_token", "" },
	};

	Task<std::string, std::string> *task =
		new Task<std::string, std::string>(0, req.dump());
	server->getWorkQueue()->push(task);
	task->waitFor();

	json result = json::parse(task->getResult());

	PrettyTable table("Recommend system results", { { "tinder_id", 50, AT_LEFT }, {"prob, %", 10, AT_LEFT}, {"URL", 512, AT_LEFT} });
	for (int i = 0; i != result["data"].size(); ++i) {
		table.addRow({
			result["data"][i]["tinder_id"].get<std::string>(), 
			result["data"][i]["prob"].get<double>(),
			result["data"][i]["photos"][0]["url"].get<std::string>()
		});
	}
	server->logger.log(et_notice, "\n" + table.dump());
}

void ConsoleParser::parseClients(const std::vector<std::string>& args) {
	if (args.size() < 2) {
		server->logger.log(et_notice, "At least one argument should be given!");
		return;
	}

	if (args[1] == "list") {
		if (args.size() >= 3 && args[2] == "verbose") {
			PrettyTable table("Verbose client list:",
				{
					{"client_id", 50, AT_LEFT},
					{"param_name", 40, AT_LEFT},
					{"param_value", 50, AT_LEFT}
				});
			for (auto it = server->getClientMap()->begin();
				it != server->getClientMap()->end(); ++it)
			{
				AuthorizedClient *client = it->second;
				for (auto it2 = client->getData().begin(); it2 != client->getData().end(); ++it2) {
					table.addRow({it->first, it2->first, it2->second});
				}
			}
			server->logger.log(et_notice, "\n" + table.dump() + "\n");
		} else {
			PrettyTable table("Client list:", { {"client", 50, AT_LEFT} });
			for (auto it = server->getClientMap()->begin();
				it != server->getClientMap()->end(); ++it)
			{
				table.addRow({it->first});
			}
			server->logger.log(et_notice, "\n" + table.dump() + "\n");
		}
	} else if (args[1] == "awake") {
		if (args.size() < 3) {
			server->logger.log(et_notice, "Syntax: clients awake [client_id]");
			return;
		}
		//std::unique_lock<std::mutex> lock(server->mysql_mutex_);
		std::string query = "SELECT tinder_id, tinder_auth_token FROM users WHERE tinder_id = '" + args[2] + "'";
		try {
			sql::Statement *st = server->mysql_con->createStatement();
			sql::ResultSet *res = st->executeQuery(query.c_str());
			size_t num = res->rowsCount();
			if (num == 0) {
				server->logger.log(et_notice, "No user with id = " + args[2]  + " found!" );
				return;
			}
			res->next();
			std::string token = res->getString("tinder_auth_token").c_str();
			json request = {
				{"tinder_id", args[2]},
				{"tinder_auth_token", token},
				{"action", "SET_AUTH_TOKEN"}
			};
			MainServer::TextTask *task = new MainServer::TextTask(0, request.dump());
			server->getWorkQueue()->push(task);
			task->waitFor();
			server->logger.log(et_notice, "Awake returned: " + task->getResult());
		} catch (sql::SQLException& exc) {
			server->logger.log(et_error, "SQL Error: " + std::string(exc.what()));
		}

	} else if (args[1] == "edit") {
		if (args.size() < 5) {
			server->logger.log(et_notice, "Syntax: clients edit [client_id] [param_name] [param_value]");
			return;
		}
		AuthorizedClient *client = server->getClient(args[2]);
		if (client == nullptr) {
			server->logger.log(et_notice, "No client with tinder_id = " + args[2]);
			return;
		}
		client->getData()[args[3]] = args[4];
		server->logger.log(et_notice, 
			"Changed! Clients[" + args[2] + "][" + args[3] + "] = " + client->getData()[args[3]]);
	}
}


void ConsoleParser::parseCacher(const std::vector<std::string>& args) {
	if (args.size() < 2) {
		server->logger.log(et_notice, "At least one argument should be given!");
		return;
	}
	if (args[1] == "pause") {
		server->cacher->pause();
	} else if (args[1] == "resume") {
		server->cacher->resume();
	} else if (args[1] == "set-delay") {
		server->cacher->setInterval(std::stoi(args[2]));
	} else {
		server->logger.log(et_notice, "No such subcommand for cacher");
	}
}

void ConsoleParser::parseBlackList(const std::vector<std::string>& args) {
	if (args.size() < 2) {
		server->logger.log(et_notice, "At least one argument should be given!");
		return;
	}
	if (args[1] == "add") {
		server->black_list.insert(args[2]);
		server->logger.log(et_notice, "User " + args[2] + " inserted to blacklist!");
	} else if (args[1] == "remove") {
		server->black_list.erase(args[2]);
		server->logger.log(et_notice, "User " + args[2] + " removed from blacklist!");
	} else if (args[1] == "list") {
		server->logger.log(et_notice, "Black list:");
		for (auto& id : server->black_list) {
			server->logger.log(et_notice, "---- " + id);
		}
	} else if (args[1] == "flush") {
		std::ofstream out_bl("blacklist.txt", std::ios::out | std::ios::trunc);
		for (auto& id : server->black_list) {
			out_bl << id << std::endl;
		}
		out_bl.close();
	} else if (args[1] == "clear") {
		server->black_list.clear();
		server->logger.log(et_notice, "Black list is clear now!");
	} else {
		server->logger.log(et_notice, "Unknown argument for blacklist");
	}
}