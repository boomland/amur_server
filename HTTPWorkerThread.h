#pragma once
#include "WorkerThread.h"
#include "Task.h"
#include "StorageResource.h"

class MainServer;

struct HTTPTaskParams {
	std::string url;
	int res_id;
};

class HTTPWorkerThread : public WorkerThread<Task<HTTPTaskParams, std::string>>
{
public:
	HTTPWorkerThread(size_t idx, MainServer *server, TaskQueue<Task<HTTPTaskParams, std::string>*> *wqueue) : WorkerThread(idx, server, wqueue) {}
	HTTPWorkerThread(HTTPWorkerThread&& r) : WorkerThread(std::forward<WorkerThread>(r)) {}

	std::string performJob(Task<HTTPTaskParams, std::string> *task);
};

