#ifndef CORRIDOR_HPP
#define CORRIDOR_HPP
#include "./common.hpp"
#include <condition_variable>
#include <list>
#include <map>
#include <memory>
#include <shared_mutex>

namespace corridor
{
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

    private:
        artifact &clone(const artifact &other);
        artifact &swap(artifact &&other);
    };

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
        void subscribe(const viwer_id_type &id);
        /**
         * @brief 退订 -- 读者线程注销
         *
         * @param id
         */
        void unsubscribe(const viwer_id_type &id);
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
        std::list<artifact> view(const viwer_id_type &id, size_t n = 0);
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

} // namespace corridor

#endif