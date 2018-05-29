#include "GPUWorker.h"
#include "MainServer.h"
#include "StorageResource.h"
#include <sstream>

#define NOTICE(x) server_->logger.log(et_notice, x)
#define itos(x) std::to_string(x)

void GPUWorker::initParams() {
	gpu_face_det_batch_size = server_->config_file_.getSection("face_detection").getProperty<int>("batch_size", 8);
	face_det_target_rows = server_->config_file_.getSection("face_detection").getProperty<int>("target_rows", 1080);
	face_det_target_cols = server_->config_file_.getSection("face_detection").getProperty<int>("target_cols", 1080);
	
	if (face_det_target_cols != face_det_target_rows) {
		server_->logger.log(et_warning, "Current version of backend does NOT"
			"supports operating images with not equal face_detection target size!");
	}

	NOTICE("Target size: " + itos(face_det_target_rows) + "x" + itos(face_det_target_cols));
}

std::string GPUWorker::performJob(Task<GPUTaskParams, std::string>* task)
{
	if (task->getContent().task_type == gpu_descriptor) {
		HTTPClient http_client;
		std::string data;
		size_t num_samples = task->getContent().res_ids.size();
		boost::filesystem::path full_path(boost::filesystem::current_path());
		for (int i = 0; i != num_samples; ++i) {
			StorageResource *res = server_->storage_manager->getResource(task->getContent().res_ids[i]);
			data += full_path.string() + "/" +  res->getRealPath();
			data += ",";
			if (res->is_single_detected != 1) {
				server_->logger.log(et_error, 
					"Resource #" + std::to_string(res->getID()) 
					+ " not single face detected! Can`t continue descriptor work.");
				return "";
			}
			data += std::to_string(res->face_det.left()) + ","
				+ std::to_string(res->face_det.right()) + ","
				+ std::to_string(res->face_det.top()) + ","
				+ std::to_string(res->face_det.bottom());
			if (i != num_samples - 1) {
				data += ";";
			}
		}
		http_client.downloadFile("http://localhost:3333/", "cache/descr_cache.tmp", {}, data);
		return "";
	}
	size_t num_samples = task->getContent().res_ids.size();
	std::vector<dlib::matrix<dlib::rgb_pixel>> imgs;
	std::vector<StorageResource*> res_arr;
	std::vector<double> scale_factors;
	for (int i = 0; i != num_samples; ++i) {
		StorageResource* res = server_->storage_manager->getResource(task->getContent().res_ids[i]);
		if (res == nullptr) {
			server_->logger.log(et_fatal, "GPU Worker| Resource not found");
			continue;
		}
		dlib::matrix<dlib::rgb_pixel> cur_img;
		try {
			dlib::load_image(cur_img, res->getRealPath());
		} catch (dlib::image_load_error e) {
			server_->logger.log(et_fatal, e.what());
			res->is_single_detected = 0;
			continue;
		}
		if (cur_img.nr() != cur_img.nc()) {
			server_->logger.log(et_warning, 
				"Image resource #" + std::to_string(task->getContent().res_ids[i]) 
				+ " cannot be resized. nr() != nc()");
			res->is_single_detected = 0;
			continue;
		}
		if (face_det_target_rows != cur_img.nr()) {
			double scale_factor = (double)face_det_target_rows / (double)cur_img.nr();
			scale_factors.push_back(scale_factor);
			resize_image(scale_factor, cur_img);
		} else {
			scale_factors.push_back(1.0);
		}
		
		imgs.push_back(cur_img);
		res_arr.push_back(res);
	}
	num_samples = imgs.size();
	server_->logger.log(et_notice, "Prepared " + std::to_string(num_samples) + " good images.");
	if (task->getContent().task_type == cnn_face_detect) {
		std::vector<std::vector<dlib::rectangle>> dets = cnn_face_detector->predict_batch(imgs,
			std::min(gpu_face_det_batch_size, num_samples));
		
		for (int i = 0; i != dets.size(); ++i) {
			if (dets[i].size() == 1) {
				// left, top, right, bottom
				dlib::rectangle cur = dets[i][0];
				dlib::rectangle rect(
					cur.left() / scale_factors[i],
					cur.top() / scale_factors[i],
					cur.right() / scale_factors[i],
					cur.bottom() / scale_factors[i]);
				res_arr[i]->face_det = rect;
				res_arr[i]->is_single_detected = 1;
			} else {
				res_arr[i]->is_single_detected = 0;
			}
		}
	} else {
		std::vector<dlib::rectangle> dets = hog_face_detector->detect(imgs[0]);
		/* -------- NOT IMPLEMENTED -----------  */
	}
	return "";
}
