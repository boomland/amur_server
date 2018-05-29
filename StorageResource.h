#pragma once

#include <string>
#include <mutex>
#include <map>
#include <unordered_map>
#include "FaceDetection.h"

class StorageResource
{
private:
	int id;
	std::string virtualPath; // {enc|usr}/$tinder_id/$photo_id@ext
	std::string realPath;
	bool isReady;
	bool isFreed;

	std::mutex mut;
	std::condition_variable cv;
public:
	StorageResource();
	StorageResource(int _id,
		const std::string& _virtualPath,
		const std::string& _realPath);

	int waitForResource(int timeout);
	void setReadyState();

	std::string getVirtualPath();
	std::string getRealPath();

	int getID();
	bool checkValid();
	bool checkReady();

	int free();

	bool is_single_detected;
	dlib::rectangle face_det;
	std::vector<float> descr;
};