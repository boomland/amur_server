#pragma once

#include <mutex>
#include <thread>
#include <functional>
#include <condition_variable>

template <typename TaskContent, typename TaskResult>
class Task {
public:
	typedef TaskResult TaskResultType;
	~Task() {}

	Task() : id_(-2), thread_id_(-1), should_callback(false) {}

	Task(const Task& tsk)
		: should_callback(tsk.should_callback),
		callback_(tsk.callback_)
	{
		content_ = tsk.content_;
		result_ = tsk.result_;
		id_ = tsk.id_;
		is_finished_ = tsk.is_finished_;
		t_start_ = tsk.t_start_;
		t_finish_ = tsk.t_finish_;
	}

	Task(int id, TaskContent _cont) {
		content_ = _cont;
		should_callback = false;
		id_ = id;
		is_finished_ = 0;
		t_start_ = std::chrono::high_resolution_clock::now();
	}

	Task(int id,
		TaskContent _cont,
		std::function<void(TaskResult&)> callback)
		: Task(id, _cont)
	{
		callback_ = callback;
		should_callback = true;
	}

	void finish(std::thread::id thread_id, TaskResult _result) {
		result_ = _result;
		t_finish_ = std::chrono::high_resolution_clock::now();
		std::unique_lock<std::mutex> lock(mut_);
		is_finished_ = true;
		thread_id_ = thread_id_;
		if (should_callback) {
			callback_(result_);
		}
		cv_finished_.notify_one();
	}

	TaskContent& getContent() {
		return content_;
	}

	TaskResult& getResult() {
		return result_;
	}

	std::mutex& getMutex() {
		return mut_;
	}

	std::condition_variable& getVariable() {
		return cv_finished_;
	}

	bool isFinished() {
		return is_finished_;
	}

	bool waitFor() {
		std::unique_lock<std::mutex> lock(getMutex());
		if (isFinished()) {
			return true;
		}
		getVariable().wait(lock,
			[&] { return isFinished(); }
		);
		return isFinished();
	}

private:
	TaskContent content_;
	TaskResult result_;
	std::function<void(TaskResult&)> callback_;

	std::mutex mut_;
	std::condition_variable cv_finished_;

	int id_;
	bool is_finished_;
	bool should_callback;
	std::thread::id thread_id_;

	std::chrono::time_point<std::chrono::high_resolution_clock> t_start_;
	std::chrono::time_point<std::chrono::high_resolution_clock> t_finish_;
};

