#pragma once
#include <dlib/dnn.h>
#include <dlib/image_processing.h>
#include <dlib/image_processing/frontal_face_detector.h>
#include <vector>

#ifdef _WIN32
class CNNFaceDetector {
public:

	CNNFaceDetector(const std::string& filename) {
		dlib::deserialize(filename) >> net;
	}

	std::vector<dlib::rectangle> predict_single(const dlib::matrix<dlib::rgb_pixel>& image)
	{
		auto dets = net(image);
		std::vector<dlib::rectangle> result;
		for (auto&& d : dets) {
			result.push_back(d.rect);
		}
		net.clean();
		return result;
	}

	std::vector<std::vector<dlib::rectangle>>
		predict_batch(
			const std::vector<dlib::matrix<dlib::rgb_pixel>>& images,
			int batch_size)
	{
		auto dets = net(images, batch_size);
		std::vector<std::vector<dlib::rectangle>> results;
		for (auto&& im_det : dets) {
			std::vector<dlib::rectangle> im_result;
			for (auto &&det : im_det) {
				im_result.push_back(det.rect);
			}
			results.push_back(im_result);
		}
		net.clean();
		return results;
	}
private:
	template <long num_filters, typename SUBNET> using con5d = con<num_filters, 5, 5, 2, 2, SUBNET>;
	template <long num_filters, typename SUBNET> using con5 = con<num_filters, 5, 5, 1, 1, SUBNET>;

	template <typename SUBNET> using downsampler = relu<affine<con5d<32, relu<affine<con5d<32, relu<affine<con5d<16, SUBNET>>>>>>>>>;
	template <typename SUBNET> using rcon5 = relu<affine<con5<45, SUBNET>>>;

	using net_type = dlib::loss_mmod<dlib::con<1, 9, 9, 1, 1, rcon5<rcon5<rcon5<downsampler<dlib::input_rgb_image_pyramid<dlib::pyramid_down<6>>>>>>>>;

	net_type net;
};
#endif
#ifdef __linux__
class CNNFaceDetector {
public:
	CNNFaceDetector(const std::string& filename) {}

	std::vector<dlib::rectangle> predict_single(const dlib::matrix<dlib::rgb_pixel>& image) {
		return std::vector<dlib::rectangle>();
	}

	std::vector<std::vector<dlib::rectangle>>
		predict_batch(
			const std::vector<dlib::matrix<dlib::rgb_pixel>>& images,
			int batch_size)
	{
		return std::vector<std::vector<dlib::rectangle>>();
	}
};
#endif


class HOGFaceDetector {
public:
	HOGFaceDetector() {
		detector = dlib::get_frontal_face_detector();
	}

	std::vector<dlib::rectangle> detect(const dlib::matrix<dlib::rgb_pixel>& image) {
		auto dets = detector(image);
		std::vector<dlib::rectangle> results;

		for (auto&& d : dets) {
			results.push_back(d);
		}

		return results;
	}

private:
	dlib::frontal_face_detector detector;
};
