#ifndef NTORRENT_NETLIB_SAFEQUEUE_H
#define NTORRENT_NETLIB_SAFEQUEUE_H
#include "netlib_header.h"
namespace netlib {
    template<typename T>
    class SafeQueue {
    public:
        SafeQueue() = default;

        SafeQueue(const SafeQueue<T> &) = delete;

        virtual ~SafeQueue() { clear(); }

        T &front() {
            std::scoped_lock lock(mutex_);
            return deque_.front();
        }

        T &back() {
            std::scoped_lock lock(mutex_);
            return deque_.back();
        }

        void push_back(const T &item) {
            std::scoped_lock lock(mutex_);
            deque_.push_back(item);
        }

        void push_front(const T &item) {
            std::scoped_lock lock(mutex_);
            deque_.push_front(item);
        }

        bool empty() {
            std::scoped_lock lock(mutex_);
            return deque_.empty();
        }

        size_t size() {
            std::scoped_lock lock(mutex_);
            return deque_.size();
        }

        void clear() {
            std::scoped_lock lock(mutex_);
            deque_.clear();
        }

        T pop_front() {
            std::scoped_lock lock(mutex_);
            T item = std::move(deque_.front());
            deque_.pop_front();
            return item;
        }

        T pop_back() {
            std::scoped_lock lock(mutex_);
            T item = std::move(deque_.back());
            deque_.pop_back();
            return item;
        }

    protected:
        std::deque<T> deque_;
        std::mutex mutex_;
    };
}
#endif //NTORRENT_NETLIB_SAFEQUEUE_H
