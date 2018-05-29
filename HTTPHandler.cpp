#include "HTTPHandler.h"
#include "MainServer.h"

#define API_PATH "api"
#define CACHE_PATH "cache/"
#define UPLOAD_PATH "upload"
#define SLASH std::string("/")

void HTTPHandler::operator()(server::request const &request, server::connection_ptr connection) {
	if (request.destination == SLASH + API_PATH) {
		std::map<std::string, std::string> headers = {
			{ "Content-Type", "application/json" },
		};

		std::ostringstream data;
		size_t content_length = getContentLength(request.headers);

		std::string post_data = readPostData(content_length, connection);
		TextTask* task = new TextTask(main_server->last_work_id++, post_data);
		main_server->getWorkQueue()->push(task);
		bool result = task->waitFor();
		if (!result) {
			data << "Internal error" << std::endl;
			main_server->logger.log(et_fatal, "Are workers dead? Work is lost");
		} else {
			data << task->getResult();
		}
		delete task;
		connection->set_status(server::connection::ok);
		connection->set_headers(headers);
		connection->write(data.str());
	} else if (request.destination.find(SLASH + CACHE_PATH) != std::string::npos) {
		std::map<std::string, std::string> headers = {};
		std::string path = request.destination.substr(
			request.destination.find(SLASH + CACHE_PATH) + strlen(CACHE_PATH) + 1
		);
		main_server->access_log.log(et_notice, "Access to " + path);
		std::ifstream fin(CACHE_PATH + path, std::ios::binary);
		if (!fin.is_open()) {
			headers["Content-Type"] = "text/html";
			connection->set_status(server::connection::ok);
			connection->set_headers(headers);
			connection->write("Not found!");
			return;
		}
		std::filebuf* pbuf = fin.rdbuf();
		std::size_t size = pbuf->pubseekoff(0, fin.end, fin.in);
		pbuf->pubseekpos(0, fin.in);
		char* buffer = new char[size];
		pbuf->sgetn(buffer, size);
		std::vector<char> data(buffer, buffer + size);
		fin.close();
		headers["Content-Length"] = std::to_string(size);
		connection->set_status(server::connection::ok);
		connection->set_headers(headers);
		connection->write(data);
		delete buffer;
	} else if (request.destination.find(SLASH + UPLOAD_PATH) != std::string::npos) {
		std::map<std::string, std::string> headers = {};
		std::vector<char> data = readBinaryPostData(getContentLength(request.headers), connection);
		std::ofstream fout("cache/cached.tmp", std::ios::binary);
		fout.write((char*)&data[0], data.size());
		fout.close();
		connection->set_status(server::connection::ok);
		connection->set_headers(headers);
		connection->write("OK");
	}
}