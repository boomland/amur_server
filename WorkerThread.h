#pragma once

#include <thread>
#include "TaskQueue.h"
#include "HTTPClient.h"


class MainServer;

template <typename TaskType>
class WorkerThread {
public:
	WorkerThread(size_t idx, MainServer *server, TaskQueue<TaskType*> *wqueue) 
		:idx_(idx),
		wthread(),
		tasks(wqueue),
		server_(server) {}

	WorkerThread(WorkerThread&& r) : wthread(std::move(r.wthread)) {
		server_ = r.server_;
		tasks = r.tasks;
		stop_thread = false;
		idx_ = r.idx_;
	}

	~WorkerThread() {
		stop_thread = true;
		if (wthread.joinable())
			wthread.join();
	}

	void stop() {
		stop_thread = true;
	}

	void start() {
		wthread = std::thread(&WorkerThread::Run, this);
	}

protected:
	HTTPClient client;
	bool stop_thread = false;
	std::thread wthread;
	TaskQueue<TaskType*> *tasks;
	MainServer *server_;
	size_t idx_;

	void Run() {
		while (!stop_thread) {
			TaskType* task;
			bool result = tasks->pop(task);
			if (result) {
				task->finish(std::this_thread::get_id(), performJob(task));
			}
		}
	}

	virtual std::string performJob(TaskType *task) { return ""; }
};