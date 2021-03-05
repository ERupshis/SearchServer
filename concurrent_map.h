#pragma once

#include <vector>
#include <map>
#include <mutex>



using namespace std::string_literals;


template <typename Key, typename Value>
class ConcurrentMap {
public:

    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct Access {
        Access(Value& value, std::mutex& mutex_value_)
            :ref_to_value(value), mutex_value_ref(mutex_value_)
        {
        }

        Value& ref_to_value;
        std::mutex& mutex_value_ref;

        ~Access()
        {
            mutex_value_ref.unlock();
        }
    };

    using Bucket = std::map<Key, Value>;

    explicit ConcurrentMap(size_t bucket_count)
        :bucketCount_(bucket_count), mutexes_(bucket_count), maps_(bucket_count)
    {
    }

    size_t Erase(const Key& key) {
        std::lock_guard<std::mutex> guard(mtx_erase);
        size_t mapId = static_cast<uint64_t>(key) % bucketCount_;
        std::lock_guard<std::mutex> guard_(mutexes_[mapId]);
        size_t result = maps_[mapId].erase(key);
        return result;
    }

    Access operator[](const Key& key) {
        std::lock_guard<std::mutex> guard(mtx_operator);
        size_t mapId = static_cast<uint64_t>(key) % bucketCount_;
        mutexes_[mapId].lock();
        return   Access(maps_[mapId][key], mutexes_[mapId]);
    }

    Bucket BuildOrdinaryMap() {
        std::lock_guard<std::mutex> guard(mtx_operator);
        std::lock_guard<std::mutex> guard_(mtx_erase);
        Bucket result;
        for (std::size_t idx = 0; idx < maps_.size(); ++idx) {
            std::lock_guard<std::mutex> guard(mutexes_[idx]);
            result.insert(maps_[idx].begin(), maps_[idx].end());
        }
        return result;
    }


private:
    size_t bucketCount_;
    std::vector<std::mutex> mutexes_;
    std::vector<Bucket> maps_;
    std::mutex mtx_operator;
    std::mutex mtx_erase;
};