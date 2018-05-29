#pragma once
#include <string>

const std::string Q_FIND_USER = "SELECT * FROM users WHERE tinder_id = ?";

const std::string Q_UPDATE_USER = 
	"UPDATE users"
	"	SET name = ?,"
	"	gender = ?,"
	"	gender_filter = ?,"
	"	birthday = ?,"
	"	distance_filter = ?,"
	"	age_min = ?,"
	"	age_max = ?,"
	"	tinder_auth_token = ?"
	" WHERE tinder_id = ? ";

const std::string Q_INSERT_USER = 
"INSERT INTO users"
"(tinder_id, facebook_id, name, gender, gender_filter, birthday, distance_filter, age_min, age_max, tinder_auth_token)"
" VALUES(? , ? , ? , ? , ? , ? , ? , ? , ? , ? )";

const std::string Q_INSERT_GEO =
"INSERT INTO geodata(tinder_id, lat, lon, city_name) VALUES ( ?, ?, ?, ? )";

const std::string Q_INSERT_CACHE_ENCOUNTER =
"INSERT IGNORE INTO encounters_cache (tinder_id, name, gender, birth_date, calculated_age, calculated_city)"
" VALUES ";

const std::string Q_INSERT_CACHE_PHOTOS =
"INSERT IGNORE INTO encounters_photos (res_id, tinder_id, img_id, img_size) VALUES ";

const std::string Q_CACHE_PHOTOS_LAST_IDX =
"SELECT max(res_id) as max_id FROM encounters_photos";

const std::string Q_INSERT_CACHE_GEO =
"INSERT INTO encounters_geo (tinder_id, lat, lon, distance) VALUES ";

const std::string Q_BASE_FILTER =
"SELECT out_fd.res_id as res_id, "
"out_ids.tinder_id as tinder_id, "
"out_fd.data as data "
"FROM "
"(SELECT DISTINCT(ep.tinder_id) "
"	FROM encounters_cache  ec "
"	JOIN encounters_photos ep ON ep.tinder_id = ec.tinder_id "
"	JOIN face_descriptors  fd ON fd.res_id = ep.res_id "
"	LEFT JOIN "
"	(SELECT * "
"		FROM marks inner_m "
"		WHERE inner_m.client_tinder_id = ?) m "
"	ON m.person_id = ep.tinder_id "
"	WHERE ec.gender = ? "
"	and ec.calculated_age between ? and ? "
"	and ec.calculated_city = ? "
"	and m.person_id IS NULL "
"	LIMIT ?) out_ids "
"	JOIN encounters_photos out_ep ON out_ids.tinder_id = out_ep.tinder_id "
"	JOIN face_descriptors out_fd ON out_ep.res_id = out_fd.res_id ";


// 1 -- target_id
const std::string Q_CLUST_STATS =
"SELECT ec.cluster_idx, SUM(mx.vote) as positive, count(*) as overall "
"FROM encounters_cache ec "
"JOIN(SELECT * FROM marks m WHERE m.client_tinder_id = ?) mx ON mx.person_id = ec.tinder_id "
"GROUP BY ec.cluster_idx "
"ORDER BY ec.cluster_idx ";


// 1    -- target_id
// 2    -- gender to find
// 3, 4 -- min/max age
// 5    -- city
// 6    -- limit cnt
const std::string Q_FILTER_v2 =
"SELECT ec.* "
"FROM encounters_cache ec "
"LEFT JOIN "
"(SELECT * "
"	FROM marks inner_m "
"	WHERE inner_m.client_tinder_id = ?) m "
"	ON m.person_id = ec.tinder_id "
"WHERE ec.gender = ? "
"	and ec.calculated_age between ? and ? "
"	and ec.calculated_city = ? "
"	and m.person_id IS NULL "
"	and ec.cluster_idx IS NOT NULL "
"ORDER BY RAND() "
"LIMIT ? ";


const std::string Q_FILTER_v3 =
"SELECT ec.* "
"FROM encounters_cache ec "
"LEFT JOIN "
"(SELECT * "
"	FROM marks inner_m "
"	WHERE inner_m.client_tinder_id = ?) m "
"	ON m.person_id = ec.tinder_id "
"WHERE ec.gender = ? "
"	and ec.calculated_age between ? and ? "
"	and ec.calculated_city = ? "
"	and m.person_id IS NULL "
"	and ec.cluster_idx = ? "
"ORDER BY RAND() "
"LIMIT ? ";


// 1 -- target_id
const std::string Q_GET_PHOTOS_URLS =
"SELECT *, CONCAT('https://images-ssl.gotinder.com/', tinder_id, '/', img_size, '_', img_id) as url "
" FROM encounters_photos ec "
" WHERE ec.tinder_id = ? ";