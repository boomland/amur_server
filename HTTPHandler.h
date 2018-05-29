#pragma once

#include <boost/network/protocol/http/server.hpp>
#include <iostream>
#include <chrono>
#include <thread>

namespace http = boost::network::http;

#include "Task.h"

class MainServer;

class HTTPHandler {
public:
	typedef http::server<HTTPHandler> server;
	typedef Task<std::string, std::string> TextTask;
	MainServer* main_server;

	HTTPHandler(MainServer* m_server) {
		main_server = m_server;
	}

	void operator()(server::request const & request, server::connection_ptr connection);

private:
	template <typename T>
	size_t getContentLength(T& headers) {
		size_t content_length = 0;
		for (auto& elem : headers) {
			if (elem.name == "Content-Length") {
				content_length = std::stoi(elem.value);
				break;
			}
		}
		return content_length;
	}

	std::vector<char> readBinaryPostData(size_t content_length, server::connection_ptr& connection) {
		typedef http::async_connection<http::tags::http_server, HTTPHandler>::input_range input_range;
		typedef boost::system::error_code ec;

		size_t buffered = 0;
		std::vector<char> post_data;
		while (buffered < content_length) {
			bool reading_finished = false;
			std::mutex mut_;
			std::condition_variable cv;

			connection->read([&](input_range range, ec err_code, std::size_t len, server::connection_ptr con) {
				for (auto it = range.begin(); it != range.end(); ++it) {
					post_data.push_back(*it);
				}
				buffered += len;
				std::unique_lock<std::mutex> lock(mut_);
				reading_finished = true;
				cv.notify_one();
			});
			std::unique_lock<std::mutex> lock(mut_);
			if (!reading_finished) {
				cv.wait(lock,
					[&] { return reading_finished; }
				);
			}
		}
		return post_data;
	}

	std::string readPostData(size_t content_length, server::connection_ptr& connection) {
		typedef http::async_connection<http::tags::http_server, HTTPHandler>::input_range input_range;
		typedef boost::system::error_code ec;

		size_t buffered = 0;
		std::string post_data;
		while (buffered < content_length) {
			bool reading_finished = false;
			std::mutex mut_;
			std::condition_variable cv;

			connection->read([&](input_range range, ec err_code, std::size_t len, server::connection_ptr con) {
				std::string chunk;
				for (auto it = range.begin(); it != range.end(); ++it) {
					chunk += *it;
				}
				post_data += chunk;
				buffered += len;
				std::unique_lock<std::mutex> lock(mut_);
				reading_finished = true;
				cv.notify_one();
			});
			std::unique_lock<std::mutex> lock(mut_);
			if (!reading_finished) {
				cv.wait(lock,
					[&] { return reading_finished; }
				);
			}
		}
		return post_data;
	}

};
