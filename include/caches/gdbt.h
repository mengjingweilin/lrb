//
// Created by zhenyus on 1/16/19.
//

#ifndef WEBCACHESIM_GDBT_H
#define WEBCACHESIM_GDBT_H

#include "cache.h"
#include "cache_object.h"
#include <unordered_map>
#include <vector>
#include <random>
#include <cmath>
#include <LightGBM/c_api.h>

using namespace std;


class GDBTMeta {
public:
    static uint8_t _max_n_past_timestamps;
    uint64_t _key;
    uint64_t _size;
    uint8_t _past_distance_idx;
    uint64_t _past_timestamp;
    vector<uint64_t> _past_distances;
    uint64_t _future_timestamp;

    GDBTMeta(const uint64_t & key, const uint64_t & size, const uint64_t & past_timestamp,
            const uint64_t & future_timestamp) {
        _key = key;
        _size = size;
        _past_timestamp = past_timestamp;
        _past_distances = vector<uint64_t >(_max_n_past_timestamps);
        _past_distance_idx = (uint8_t) 0;
        _future_timestamp = future_timestamp;
    }

    inline void update(const uint64_t &past_timestamp, const uint64_t &future_timestamp) {
        //distance
        _past_distances[_past_distance_idx%_max_n_past_timestamps] = past_timestamp - _past_timestamp;
        _past_distance_idx = _past_distance_idx + (uint8_t) 1;
        if (_past_distance_idx >= _max_n_past_timestamps * 2)
            _past_distance_idx -= _max_n_past_timestamps;
        //timestamp
        _past_timestamp = past_timestamp;
        _future_timestamp = future_timestamp;
    }
};


class GDBTTrainingData {
public:
    // training info
    vector<float> labels;
    vector<int32_t> indptr = {0};
    vector<int32_t> indices;
    vector<double> data;
};


class GDBTCache : public Cache
{
public:
    //key -> (0/1 list, idx)
    unordered_map<uint64_t, pair<bool, uint32_t>> key_map;
    vector<GDBTMeta> meta_holder[2];

    // sample_size
    uint sample_rate = 32;
    // threshold
    uint64_t threshold = 10000000;
    double log1p_threshold = log1p(threshold);
    // n_past_interval
    uint8_t max_n_past_intervals = 64;

    vector<GDBTTrainingData> pending_training_data;
    uint64_t gradient_window = 100000;  //todo: does this large enough

    BoosterHandle booster = nullptr;

    default_random_engine _generator = default_random_engine();
    uniform_int_distribution<std::size_t> _distribution = uniform_int_distribution<std::size_t>();

    GDBTCache()
        : Cache()
    {
    }

    void init_with_params(map<string, string> params) override {
        //set params
        for (auto& it: params) {
            if (it.first == "sample_rate") {
                sample_rate = stoul(it.second);
            } else if (it.first == "threshold") {
                threshold = stoull(it.second);
                log1p_threshold = std::log1p(threshold);
            } else if (it.first == "n_past_intervals") {
                max_n_past_intervals = (uint8_t) stoi(it.second);
            } else if (it.first == "gradient_window") {
                gradient_window = stoull(it.second);
            } else {
                cerr << "unrecognized parameter: " << it.first << endl;
            }
        }

        //init
        GDBTMeta::_max_n_past_timestamps = max_n_past_intervals;
    }

    virtual bool lookup(SimpleRequest& req);
    virtual void admit(SimpleRequest& req);
    virtual void evict(const uint64_t & t);
    void evict(SimpleRequest & req) {};
    void evict() {};
    //sample, rank the 1st and return
    pair<uint64_t, uint32_t > rank(const uint64_t & t);
    void try_train(uint64_t & t);
    void sample(uint64_t &t);
};

static Factory<GDBTCache> factoryGBDT("GDBT");
#endif //WEBCACHESIM_GDBT_H
