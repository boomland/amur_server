#pragma once

#include <curlpp/cURLpp.hpp>
#include <curlpp/Easy.hpp>
#include <curlpp/Options.hpp>
#include <curlpp/Exception.hpp>

static size_t FileWriteCallback(char* ptr, size_t size, size_t nmemb, void *f) {
	FILE *file = (FILE *)f;
	return fwrite(ptr, size, nmemb, file);
};

static size_t StringWriteCallback(char *ptr, size_t size, size_t nmemb, void *s) {
	std::string *str = (std::string *)s;
	str->append(ptr, size * nmemb);
	return nmemb;
}


class HTTPClient {
public:
	typedef std::list<std::string> HeaderList;
	typedef std::map<std::string, std::string> HeaderMap;
	typedef std::map<std::string, std::string> ParamsMap;

	HTTPClient() {}

	HeaderList prepareHeaders(const HeaderMap& headers) {
		HeaderList result;
		for (auto& cur : headers) {
			result.push_back(cur.first + ": " + cur.second);
		}
		return result;
	}

	std::string modifyUrl(const std::string& basic_url,
		const ParamsMap& params) {
		std::string result = basic_url;
		bool is_first = true;
		for (auto& elem : params) {
			if (is_first) {
				result += '?';
				is_first = false;
			} else {
				result += '&';
			}
			result += elem.first + '=' + elem.second;
		}
		return result;
	}

	std::string performGetRequest(const std::string basic_url,
		const ParamsMap& params,
		const HeaderMap& headers) {

		std::string url = modifyUrl(basic_url, params);
		return performBasicRequest(url, headers, "");
	}

	std::string performGetRequest(const std::string url,
		const HeaderMap& headers) {
		return performBasicRequest(url, headers, "");
	}

	std::string performPostRequest(const std::string url,
		const std::string& post_data,
		const HeaderMap& headers) {
		return performBasicRequest(url, headers, post_data);
	}

	bool downloadFile(const std::string url,
					  const std::string cache_file,
					  const std::map<std::string, std::string>& headers,
					const std::string post_data = "") {
		try
		{
			curlpp::Cleanup cleaner;
			curlpp::Easy request;
			curlpp::options::WriteFunctionCurlFunction
				fwriter(FileWriteCallback);

			FILE *file = nullptr;
			file = fopen(cache_file.c_str(), "wb");
			if (file == nullptr) {
				return false;
			}

			curlpp::OptionTrait<void *, CURLOPT_WRITEDATA>
				data(file);

			request.setOpt(fwriter);
			request.setOpt(data);
            request.setOpt(new curlpp::options::SslVerifyPeer(false));
			request.setOpt(new curlpp::options::Url(url));
			request.setOpt(new curlpp::options::Verbose(false));
			if (post_data.size() != 0) {
				request.setOpt(new curlpp::options::PostFields(post_data));
			}
			request.setOpt(
				new curlpp::options::HttpHeader(prepareHeaders(headers))
			);
			request.perform();
			fclose(file);
		}
		catch (curlpp::LogicError & e)
		{
			std::cout << e.what() << std::endl;
		}
		catch (curlpp::RuntimeError & e)
		{
			std::cout << e.what() << std::endl;
		}
		return true;
	}

	~HTTPClient() {}
private:

	std::string performBasicRequest(const std::string url,
		const std::map<std::string, std::string>& headers,
		const std::string& post_data) {
		// ------
		std::string result;
		try
		{
			curlpp::Easy request;
			curlpp::options::WriteFunctionCurlFunction
				fwriter(StringWriteCallback);


			curlpp::OptionTrait<void*, CURLOPT_WRITEDATA>
				data(&result);

			request.setOpt(fwriter);
			request.setOpt(data);
            request.setOpt(new curlpp::options::SslVerifyPeer(false));
			request.setOpt(new curlpp::options::Url(url));
			request.setOpt(new curlpp::options::Verbose(false));
			if (post_data.size() != 0) {
				request.setOpt(new curlpp::options::PostFields(post_data));
			}
			request.setOpt(
				new curlpp::options::HttpHeader(prepareHeaders(headers))
			);
			request.perform();
		}
		catch (curlpp::LogicError & e)
		{
			std::cout << e.what() << std::endl;
		}
		catch (curlpp::RuntimeError & e)
		{
			std::cout << e.what() << std::endl;
		}
		return result;
	}
};

