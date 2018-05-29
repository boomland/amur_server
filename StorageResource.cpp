#include "StorageResource.h"
#include "FaceDetection.h"

StorageResource::StorageResource() : id(-1) {}

StorageResource::StorageResource(int _id, const std::string & _virtualPath, const std::string & _realPath)
	: id(_id), isReady(0), isFreed(0), virtualPath(_virtualPath), realPath(_realPath), is_single_detected(0)
{
}

int StorageResource::waitForResource(int timeout)
{
	std::unique_lock<std::mutex> lock(mut);
	if (checkReady()) {
		return true;
	}
	cv.wait(lock,
		[&] { return checkReady(); }
	);
	return checkReady();
}

void StorageResource::setReadyState()
{
	std::unique_lock<std::mutex> lock(mut);
	isReady = 1;
	cv.notify_all();
}

std::string StorageResource::getVirtualPath()
{
	if (!checkValid()) {
		return "";
	}
	return virtualPath;
}

std::string StorageResource::getRealPath()
{
	if (!checkValid()) {
		return "";
	}
	return realPath;
}

int StorageResource::getID()
{
	if (!checkValid()) {
		return -1;
	}
	return id;
}

bool StorageResource::checkValid()
{
	return !isFreed;
}

bool StorageResource::checkReady()
{
	return isReady;
}

int StorageResource::free()
{
	//std::remove(realPath.c_str());
	isFreed = 1;
	return 0;
}
