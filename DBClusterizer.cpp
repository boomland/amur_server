#include "DBClusterizer.h"
#include "MainServer.h"
#include <cppconn/driver.h>
#include <cppconn/exception.h>
#include <cppconn/resultset.h>
#include <cppconn/statement.h>
#include <cppconn/prepared_statement.h>

#include "ClusterPredictor.h"

DBClusterizer::DBClusterizer(MainServer * _server)
{
	server = _server;
}

void DBClusterizer::executeID(const std::string& id) {
	sql::Statement *st = server->mysql_con->createStatement();
	std::string query = "SELECT fd.res_id as res_id, fd.data as data "
		"FROM face_descriptors fd "
		"JOIN encounters_photos ep ON ep.res_id = fd.res_id "
		"WHERE ep.tinder_id = '" + id + "' ";
	sql::ResultSet *res = st->executeQuery(query.c_str());
	size_t num_d = res->rowsCount();
	if (num_d == 0) {
		return;
	}
	std::string clusters;
	std::valarray<float> avg(0.0f, 2048);

	while (res->next()) {
		std::valarray<float> descr;
		ClusterPredictor<float>::streamToValArray(res->getBlob("data"), descr);
		avg += descr;

		size_t cluster_idx = server->cluster_predictor->predictSingle(descr);
		clusters += std::to_string(cluster_idx) + " ";
	}

	avg /= (float)num_d;
	size_t avg_num = server->cluster_predictor->predictSingle(avg);
	query = "UPDATE encounters_cache SET cluster_idx = " + std::to_string(avg_num) + " WHERE tinder_id = '" + id + "'";
	st->executeUpdate(query.c_str());
	server->logger.log(et_notice, "ID: " + id + ". Clusters: " + clusters + ". Average cluster: " + std::to_string(avg_num));
	delete st;
	delete res;
}


void DBClusterizer::execute()
{
	try {
		sql::Statement *st = server->mysql_con->createStatement();
		std::string query = "SELECT tinder_id from encounters_cache";
		sql::ResultSet *res = st->executeQuery(query.c_str());
		std::vector<std::string> ids;
		while (res->next())
			ids.push_back(res->getString("tinder_id").c_str());
		delete st;
		delete res;
		server->logger.log(et_notice, "Acquired " + std::to_string(ids.size()) + " ids");
		for (int i = 0; i != ids.size(); ++i) {
			executeID(ids[i]);
		}
		server->mysql_con->commit();
	} catch (sql::SQLException& err) {
		server->logger.log(et_error, "SQL Exception: " + std::string(err.what()));
	}
}

DBClusterizer::~DBClusterizer()
{
}
