# Corridor
> 基于在走廊摆放艺术品的概念做的消息队列  
> 走廊摆放艺术品，顾客自行观看  
> 消息队列不做消息分发，而由读者线程主动读取  

## 1. 设计思路
|模型|代码|
|-|-|
|走廊上摆放的艺术品是有限的|所能存储的消息队列的长度是有限的|
|仓库里可以存储一些艺术品|有一些信息可以先缓存起来|
|一次性只能安装一批艺术品|一个时刻只能有一个线程进行写入|
|不会拆除有人在欣赏的艺术品|没有线程进行读操作时才能进行写操作|
|一件艺术品可以同时被多个人欣赏|一条消息可以由多个人来接收|
|参观者买票后主动过来参观|队列不进行分发|
|展厅可以发消息通知用户来参观|新消息到达时唤醒读者线程|
|用户欣赏完后通知展馆|读者线程读取后唤醒线程进行清理工作|

## 2. 代码
1. 艺术品
```cpp
/**
 * @brief 艺术品类型 -- 消息类型
 *
 */
enum class artifact_type
{
    EMPTY,   // 空消息
    EMBEDED, // 有数据
    ENTER,   // 刷新缓冲区
    END,     // 结束
    ERROR,   // 报错
};

/**
 * @brief 艺术品标号 -- 消息标号
 *
 */
struct artifact_index
{
    timestamp_type timestamp; // 接收时间戳
    index_type index;         // 序列号
    static artifact_index max();
    bool operator<(const artifact_index &other) const;
    bool operator==(const artifact_index &other) const;
};

/**
 * @brief 艺术品 -- 消息
 *
 */
class artifact
{
public:
    artifact_type type;              // 类型
    std::shared_ptr<uint8_t[]> data; // 数据
    size_t data_length;              //数据长度

public:
    artifact();
    artifact(const artifact &other);
    artifact(artifact &&other);
    artifact &operator=(const artifact &other);
    artifact &operator=(artifact &&other);
    ~artifact(){};
};
```
2. 走廊
```cpp
/**
 * @brief 放置艺术品的走廊 -- 消息队列
 *
 */
class passage
{
public:
    /**
     * @brief 装饰艺术品 --  向队列写入新消息
     *
     * @param art
     */
    void decorate(artifact art);
    /**
     * @brief 拆除多余的艺术品 -- 清理消息
     *
     */
    void dismantle();
    /**
     * @brief 清理已被所有人观赏过的艺术品 -- 清理已被所有读者读取的消息
     *
     */
    void clear_all_viewed();
    /**
     * @brief 订阅 -- 读者线程注册
     *
     * @param id
     */
    void subscribe(viwer_id_type id);
    /**
     * @brief 退订 -- 读者线程注销
     *
     * @param id
     */
    void unsubscribe(viwer_id_type id);
    /**
     * @brief 目前正在展示的艺术品数量 -- 可被读取的消息队列中的消息数
     *
     * @return index_type
     */
    index_type number_of_displayed();
    /**
     * @brief 将要展示的艺术品数量 -- 被缓存起来的消息个数
     *
     * @return index_type
     */
    index_type size_of_storage();
    /**
     * @brief 欣赏艺术品 -- 读者线程读取消息
     *
     * @param id 入场凭证 -- 读者线程标识
     * @param n 将要欣赏的艺术品个数 -- 读取的消息个数
     * @return std::list<artifact>
     */
    std::list<artifact> view(viwer_id_type id, size_t n = 0);
    /**
     * @brief 等待新展品 -- 等待新消息
     *
     */
    void wait()
    {
        std::unique_lock<std::mutex> lock{notification};
        waiting.wait(lock);
    }
    /**
     * @brief 为了新展品等待一段时间 -- 为了新消息等待一段时间
     *
     * @tparam rep
     * @tparam period
     * @param rel_time
     */
    template <class rep, class period>
    void wait_for(const std::chrono::duration<rep, period> &rel_time = 0)
    {
        std::unique_lock<std::mutex> lock{notification};
        waiting.wait_for(lock, rel_time);
    }
    /**
     * @brief 等待顾客的反馈 -- 等待读者线程更新读取信息的情况
     *
     */
    void receive()
    {
        std::unique_lock<std::mutex> lock{confirmation};
        receiving.wait(lock);
    }
    /**
     * @brief 为了等顾客的反馈而等待一段时间 -- 为了等读者线程读取而等待一段时间
     *
     */
    template <class rep, class period>
    void receive_for(const std::chrono::duration<rep, period> &rel_time = 0)
    {
        std::unique_lock<std::mutex> lock{confirmation};
        receiving.wait_for(lock, rel_time);
    }

public:
    passage(index_type display_limit = 100, index_type storage_limit = 100);
    passage(const passage &other) = delete;
    passage(passage &&other) = delete;
    passage operator=(const passage &other) = delete;
    passage operator=(passage &&other) = delete;
    ~passage(){};

private:
    using artifact_map_type = std::map<artifact_index, artifact>;
    using storage_list_type = std::list<std::pair<timestamp_type, artifact>>;
    using viewer_map_type = std::map<viwer_id_type, artifact_index>;
    index_type display_limit;            // 展品个数 -- 消息个数
    index_type storage_limit;            // 仓储的展品数 -- 缓存消息个数
    artifact_map_type artifact_map;      // 展品列表 -- 消息列表
    storage_list_type storage_list;      // 仓储列表 -- 缓存消息列表
    viewer_map_type viewer_map;          // 顾客浏览记录 -- 读者线程读取进度
    std::mutex work_sync;                // 装修线程同步 -- 写线程同步
    std::shared_timed_mutex subscribing; // 订阅 -- 读者线程注册
    std::shared_timed_mutex working;     // 工作同步
    std::mutex notification;             // 更新消息提醒
    std::condition_variable waiting;     // 展品信息更新等待处 -- 读者线程等待
    std::mutex confirmation;             // 顾客浏览进度更新
    std::condition_variable receiving;   // 顾客浏览信息更新等待处 -- 写线程等待
};
```
## 3. 使用示例
```cpp
void set_art(passage &gallery)
{
    auto start_time = get_timestamp<chrono::nanoseconds>();
    uint64_t sum_up = 0;
    for (size_t i = 0; i < 1000000; i++)
    {
        artifact art;
        art.type = corridor::artifact_type::EMBEDED;
        art.data_length = 8;
        art.data = shared_ptr<uint8_t[]>(new uint8_t[8]);
        *((uint64_t *)art.data.get()) = i;
        sum_up += i;
        gallery.decorate(std::move(art));
        gallery.clear_all_viewed();
    }
    // 注意在消息分发结束时，要发送一个END类型的消息确保缓存消息也被加入队列
    {
        artifact art;
        art.type = corridor::artifact_type::END;
        art.data_length = 1;
        art.data = shared_ptr<uint8_t[]>(new uint8_t[1]);
        art.data[0] = 0;
        gallery.decorate(std::move(art));
    }
    auto end_time = get_timestamp<chrono::nanoseconds>();
    lock_guard<mutex> lock{out};
    cout << "gallery \tsum_up: " << std::to_string(sum_up)
         << "\ttime: " << (end_time - start_time) / 1e9 << "s" << endl;
}

void view_all_show(passage &gallery, int id)
{
    auto start_time = get_timestamp<chrono::nanoseconds>();
    gallery.subscribe(id);
    uint64_t sum_up = 0;
    bool if_end = false;
    while (!if_end)
    {
        gallery.wait_for(chrono::milliseconds(1));
        auto art_list = gallery.view(id);
        for (auto &&art : art_list)
        {
            if (art.type == corridor::artifact_type::EMBEDED)
            {
                uint64_t i = *((uint64_t *)art.data.get());
                if (i != last + 1)
                {
                    lock_guard<mutex> lock{out};
                    cout << "viewer: " << id
                         << " \tlast: " << std::to_string(last)
                         << " \ti: " << std::to_string(i)
                         << endl;
                }
                last = i;
                sum_up += i;
            }
            else if (art.type == corridor::artifact_type::END)
            {
                if_end = true;
                break;
            }
            else if (art.type == corridor::artifact_type::ERROR)
            {
                if_end = true;
                break;
            }
        }
    }
    auto end_time = get_timestamp<chrono::nanoseconds>();
    lock_guard<mutex> lock{out};
    cout << "viewer: " << id << " \tsum_up: " << std::to_string(sum_up)
         << "\ttime: " << (end_time - start_time) / 1e9 << "s" << endl;
}

int main(int argc, char const *argv[])
{
    cout << "hello corridor message" << endl;
    passage gallery{100, 100};
    auto start_time = get_timestamp<chrono::nanoseconds>();
    auto viewer = new thread(view_all_show, std::ref(gallery), 1);
    auto worker = new thread(set_art, std::ref(gallery));
    viewer->join();
    worker->join();
    auto end_time = get_timestamp<chrono::nanoseconds>();
    cout << "time: " << (end_time - start_time) / 1e9 << "s" << endl;
    return 0;
}
```