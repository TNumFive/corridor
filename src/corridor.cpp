#include "./corridor.hpp"
#include <limits>

using namespace std;
using namespace corridor;

inline artifact_index artifact_index::max()
{
    auto timestamp = numeric_limits<timestamp_type>::max();
    auto index = numeric_limits<index_type>::max();
    return {timestamp, index};
}

inline bool artifact_index::operator<(const artifact_index &other) const
{
    if (this->timestamp < other.timestamp)
    {
        return true;
    }
    else if (this->timestamp == other.timestamp)
    {
        if (this->index < other.index)
        {
            return true;
        }
    }
    return false;
}

inline bool artifact_index::operator==(const artifact_index &other) const
{
    if (this->timestamp == other.timestamp && this->index == other.index)
    {
        return true;
    }
    return false;
}

artifact::artifact()
{
    type = artifact_type::EMPTY;
    data_length = 0;
}

artifact::artifact(const artifact &other)
{
    clone(other);
}

artifact::artifact(artifact &&other)
{
    swap(std::move(other));
}

artifact &artifact::operator=(const artifact &other)
{
    return clone(other);
}

artifact &artifact::operator=(artifact &&other)
{
    return swap(std::move(other));
}

artifact &artifact::clone(const artifact &other)
{
    if (this != &other)
    {
        type = other.type;
        data = other.data;
        data_length = other.data_length;
    }
    return *this;
}

artifact &artifact::swap(artifact &&other)
{
    if (this != &other)
    {
        std::swap(type, other.type);
        data.swap(other.data);
        std::swap(data_length, other.data_length);
    }
    return *this;
}

passage::passage(index_type display_limit, index_type storage_limit)
{
    constexpr index_type min = 10;
    this->display_limit = display_limit < min ? min : display_limit;
    this->storage_limit = storage_limit < min ? min : storage_limit;
    artifact_index init{};
    init.timestamp = get_timestamp<time_precision>();
    artifact_map[init] = {};
}

void passage::decorate(artifact art)
{
    auto task = [this](artifact &&art)
    {
        auto current_index = artifact_map.rbegin()->first;
        for (auto &&p : storage_list)
        {
            current_index.timestamp = p.first;
            current_index.index += 1;
            artifact_map[current_index] = std::move(p.second);
        }
        storage_list.clear();
        current_index.timestamp = get_timestamp<time_precision>();
        current_index.index += 1;
        artifact_map[current_index] = std::move(art);
        waiting.notify_all();
    };
    unique_lock<mutex> work_sync_lock{work_sync};
    unique_lock<shared_timed_mutex> working_lock{working, defer_lock};
    if (working_lock.try_lock())
    {
        task(std::move(art));
        return;
    }
    if (storage_list.size() < storage_limit &&
        art.type != artifact_type::ENTER &&
        art.type != artifact_type::END &&
        art.type != artifact_type::ERROR)
    {
        auto now = get_timestamp<time_precision>();
        storage_list.emplace_back(now, std::move(art));
    }
    else
    {
        working_lock.lock();
        task(std::move(art));
    }
}

void passage::dismantle()
{
    unique_lock<shared_timed_mutex> working_lock{working};
    while (artifact_map.size() > display_limit)
    {
        artifact_map.erase(artifact_map.begin());
    }
}

void passage::clear_all_viewed()
{
    if (artifact_map.size() < display_limit)
    {
        return;
    }
    artifact_index newest_all_viewed = artifact_index::max();
    shared_lock<shared_timed_mutex> subscribing_lock{subscribing};
    for (const auto &p : viewer_map)
    {
        if (p.second < newest_all_viewed)
        {
            newest_all_viewed = p.second;
        }
    }
    subscribing_lock.unlock();
    unique_lock<shared_timed_mutex> working_lock{working};
    for (auto it = artifact_map.begin(); it != artifact_map.end();)
    {
        if (it->first < newest_all_viewed)
        {
            it = artifact_map.erase(it);
        }
        else
        {
            break;
        }
    }
}

void passage::subscribe(const viwer_id_type &id)
{
    unique_lock<shared_timed_mutex> subscribing_lock{subscribing};
    viewer_map[id] = {0, 0};
}

void passage::unsubscribe(const viwer_id_type &id)
{
    unique_lock<shared_timed_mutex> subscribe_lock{subscribing};
    viewer_map.erase(id);
}

index_type passage::number_of_displayed()
{
    return artifact_map.size();
}

index_type passage::size_of_storage()
{
    return storage_list.size();
}

list<artifact> passage::view(const viwer_id_type &id, size_t n)
{
    shared_lock<shared_timed_mutex> subscribing_lock{subscribing};
    auto viewer_it = viewer_map.find(id);
    if (viewer_it == viewer_map.end())
    {
        artifact error;
        error.type = artifact_type::ERROR;
        constexpr char error_msg[] = "viewer id not found";
        error.data.reset(new uint8_t[sizeof(error_msg)]);
        error.data_length = sizeof(error_msg);
        std::copy(error_msg, error_msg + sizeof(error_msg), error.data.get());
        return {error};
    }
    subscribing_lock.unlock();
    auto &viewer_artifact_index = viewer_it->second;
    shared_lock<shared_timed_mutex> working_lock{working};
    const auto &current_index = artifact_map.rbegin()->first;
    if (viewer_artifact_index == current_index)
    {
        return {};
    }
    auto it = artifact_map.find(viewer_artifact_index);
    if (it == artifact_map.end())
    {
        it = artifact_map.begin();
    }
    else
    {
        it++;
    }
    list<artifact> artifact_list;
    do
    {
        artifact_list.push_back(it->second);
        viewer_artifact_index = it->first;
        it++;
        n--;
    } while (it != artifact_map.end() && n);
    receiving.notify_one();
    return artifact_list;
}