#pragma once

#include <queue>
#include <mutex>
#include <thread>
#include <iostream>
#include <condition_variable>

template <typename TaskType>
class TaskQueue {
 public:
	TaskQueue() {}

	~TaskQueue() {}

	void push(TaskType task) {
		std::unique_lock<std::mutex> lock(mtx_);
		queue_.push(task);
		cv_.notify_one();
	}

	bool pop(TaskType& task) {
		std::unique_lock<std::mutex> lock(mtx_);
		//std::cout << "Inside pop" << std::endl;
		cv_.wait_for(lock,
			std::chrono::seconds(2),
			[&] { return !queue_.empty(); }
		);

		if (!queue_.empty()) {
			task = queue_.front();
			queue_.pop();
		}
		else {
			return false;
		}
		return true;
	}

 private:
	std::mutex mtx_;
	std::condition_variable cv_;
	std::queue<TaskType> queue_;
};
