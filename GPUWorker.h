#pragma once
#include "WorkerThread.h"
#include "Task.h"

#include "FaceDetection.h"
#include <dlib/image_io.h>

enum GPUTaskType {
	cnn_face_detect,
	hog_face_detect,
	gpu_descriptor
};

struct GPUTaskParams {
	GPUTaskType task_type;
	std::vector<int> res_ids;
};

class MainServer;

class GPUWorker : public WorkerThread<Task<GPUTaskParams, std::string>>
{
public:
	GPUWorker(size_t idx, MainServer *server, TaskQueue<Task<GPUTaskParams, std::string>*> *wqueue)
		: WorkerThread(idx, server, wqueue)
	{
		cnn_face_detector = new CNNFaceDetector("models/mmod_human_face_detector.dat");
		hog_face_detector = new HOGFaceDetector();
		initParams();
	}

	GPUWorker(GPUWorker&& r)
		: WorkerThread(std::forward<WorkerThread>(r))
	{
		cnn_face_detector = r.cnn_face_detector;
		hog_face_detector = r.hog_face_detector;
		initParams();
	}

	std::string performJob(Task<GPUTaskParams, std::string>* task);
	void initParams();
private:
	CNNFaceDetector* cnn_face_detector;
	HOGFaceDetector* hog_face_detector;

	size_t gpu_face_det_batch_size;
	size_t face_det_target_rows;
	size_t face_det_target_cols;
};

