#include "ClusterPredictor.h"
#include <fstream>
#include <string.h>

template class ClusterPredictor<float>;

template<typename FloatType>
ClusterPredictor<FloatType>::ClusterPredictor()
{
}

template<typename FloatType>
ClusterPredictor<FloatType>::ClusterPredictor(size_t _n_clusters, const std::string & filename)
{
	n_clusters = _n_clusters;
	const int DIMS = 2048;
	std::ifstream in(filename, std::ios::in | std::ios::binary);
	int file_size = n_clusters * sizeof(FloatType) * DIMS;
	char buffer[DIMS * sizeof(FloatType)];
	centers.resize(n_clusters);
	for (size_t cluster = 0; cluster != n_clusters; ++cluster) {
		in.read(buffer, DIMS * sizeof(FloatType));
		bufferToValArray(buffer, centers[cluster]);
	}
}

template<typename FloatType>
void ClusterPredictor<FloatType>::bufferToValArray(char * buffer, std::valarray<FloatType>& arr)
{
	float data[2048];
	arr.resize(2048);
	for (int i = 0; i != 2048; ++i) {
		memcpy(data + i, buffer + i * sizeof(FloatType), sizeof(FloatType));
		arr[i] = data[i];
	}
}

template<typename FloatType>
void ClusterPredictor<FloatType>::streamToValArray(std::istream * is, std::valarray<FloatType>& arr)
{
	char buffer[8192];
	is->read(buffer, 8192);
	ClusterPredictor<float>::bufferToValArray(buffer, arr);
}

template<typename FloatType>
FloatType ClusterPredictor<FloatType>::euclidianDistance(const std::valarray<FloatType>& a, const std::valarray<FloatType>& b)
{
	FloatType result = 0;
	for (int i = 0; i != 2048; ++i) {
		float cur = a[i] - b[i];
		result += cur * cur;
	}
	return std::sqrt(result);
}

template<typename FloatType>
std::vector<size_t> ClusterPredictor<FloatType>::predictBatch(const std::vector<std::valarray<FloatType>>& data)
{
	std::vector<size_t> answer(data.size());
	for (size_t i = 0; i != data.size(); ++i) {
		answer[i] = predictSingle(data[i]);
	}
	return answer;
}

template<typename FloatType>
size_t ClusterPredictor<FloatType>::predictSingle(const std::valarray<FloatType>& data)
{
	FloatType min_dist = euclidianDistance(data, centers[0]);
	int idx = 0;
	for (int i = 0; i != n_clusters; ++i) {
		FloatType cur_dist = euclidianDistance(data, centers[i]);
		if (cur_dist < min_dist) {
			min_dist = cur_dist;
			idx = i;
		}
	}
	return idx;
}

template<typename FloatType>
size_t ClusterPredictor<FloatType>::clusterCount() const
{
	return n_clusters;
}

template<typename FloatType>
const std::valarray<FloatType>& ClusterPredictor<FloatType>::getCentroid(size_t idx) const
{
	return centers[idx];
}
