#pragma once

#include <math.h>
#include <istream>
#include <string>
#include <valarray>
#include <vector>

template <typename FloatType>
class ClusterPredictor
{
public:
	
	ClusterPredictor();
	ClusterPredictor(size_t _n_clusters, const std::string & filename);

	static void bufferToValArray(char *buffer, std::valarray<FloatType>& arr);
	static void streamToValArray(std::istream* is, std::valarray<FloatType>& arr);
	static FloatType euclidianDistance(const std::valarray<FloatType>& a,
		const std::valarray<FloatType>& b);
	
	std::vector<size_t> predictBatch(const std::vector<std::valarray<FloatType>>& data);
	size_t predictSingle(const std::valarray<FloatType>& data);

	size_t clusterCount() const;
	const std::valarray<FloatType>& getCentroid(size_t idx) const;

private:
	size_t n_clusters;
	std::vector<std::valarray<FloatType>> centers;
};

