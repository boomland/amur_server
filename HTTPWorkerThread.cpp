#include "HTTPWorkerThread.h"
#include "MainServer.h"


std::string HTTPWorkerThread::performJob(Task<HTTPTaskParams, std::string>* task)
{
	//server_->logger.log(et_notice, "Heey! Im HTTP hard worker, just got a nice job!");
	StorageResource *res = server_->storage_manager->getResource(task->getContent().res_id);
	//server_->logger.log(et_notice, "Got resource pointer");
	if (res == nullptr) {
		server_->logger.log(et_fatal, "Wow:( resource not found");
		return "";
	}
	client.downloadFile(task->getContent().url, res->getRealPath(), {});
	res->setReadyState();
	return res->getRealPath();
}
