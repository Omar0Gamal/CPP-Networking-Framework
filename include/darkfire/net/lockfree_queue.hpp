#pragma once

#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <mutex>
#include <condition_variable>
#include <deque>

#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable: 4324) // structure was padded due to alignment specifier
#endif

namespace darkfire::net {

    // Cache-line size to prevent false sharing
    constexpr size_t CacheLineSize = 64;

    // SPSCQueue: Lock-free single-producer single-consumer ring buffer
    template <typename T, size_t Capacity = 1024>
    class SPSCQueue {
        static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");
        static_assert(Capacity >= 2, "Capacity must be at least 2");

    public:
        SPSCQueue() : m_head(0), m_tail(0) {}

        ~SPSCQueue() {
            T item;
            while (Pop(item)) {}
        }

        SPSCQueue(const SPSCQueue&) = delete;
        SPSCQueue& operator=(const SPSCQueue&) = delete;

        template <typename... Args>
        bool Emplace(Args&&... args) {
            const size_t head = m_head.load(std::memory_order_relaxed);
            const size_t tail = m_tail.load(std::memory_order_acquire);

            if ((head - tail) >= Capacity) {
                return false; // Queue full
            }

            m_storage[head & Mask].Construct(std::forward<Args>(args)...);
            m_head.store(head + 1, std::memory_order_release);
            return true;
        }

        bool Push(const T& item) {
            return Emplace(item);
        }

        bool Push(T&& item) {
            return Emplace(std::move(item));
        }

        bool Pop(T& out) {
            const size_t tail = m_tail.load(std::memory_order_relaxed);
            const size_t head = m_head.load(std::memory_order_acquire);

            if (tail == head) {
                return false; // Queue empty
            }

            out = std::move(*m_storage[tail & Mask].Get());
            m_storage[tail & Mask].Destroy();
            m_tail.store(tail + 1, std::memory_order_release);
            return true;
        }

        std::optional<T> Pop() {
            T item;
            if (Pop(item)) {
                return item;
            }
            return std::nullopt;
        }

        [[nodiscard]] size_t Size() const noexcept {
            const size_t head = m_head.load(std::memory_order_relaxed);
            const size_t tail = m_tail.load(std::memory_order_relaxed);
            return (head >= tail) ? (head - tail) : 0;
        }

        [[nodiscard]] bool Empty() const noexcept {
            return m_head.load(std::memory_order_relaxed) == m_tail.load(std::memory_order_relaxed);
        }

        [[nodiscard]] constexpr size_t GetCapacity() const noexcept {
            return Capacity;
        }

    private:
        static constexpr size_t Mask = Capacity - 1;

        struct Storage {
            alignas(alignof(T)) uint8_t data[sizeof(T)];

            template <typename... Args>
            void Construct(Args&&... args) {
                ::new (static_cast<void*>(data)) T(std::forward<Args>(args)...);
            }

            T* Get() noexcept {
                return reinterpret_cast<T*>(data);
            }

            const T* Get() const noexcept {
                return reinterpret_cast<const T*>(data);
            }

            void Destroy() noexcept {
                Get()->~T();
            }
        };

        alignas(CacheLineSize) std::atomic<size_t> m_head{ 0 };
        alignas(CacheLineSize) std::atomic<size_t> m_tail{ 0 };
        alignas(CacheLineSize) Storage m_storage[Capacity];
    };

    // MPSCQueue: Lock-free multi-producer single-consumer unbounded queue (Vyukov node queue)
    template <typename T>
    class MPSCQueue {
    public:
        MPSCQueue() {
            auto* dummy = new Node();
            m_head.store(dummy, std::memory_order_relaxed);
            m_tail = dummy;
        }

        ~MPSCQueue() {
            T item;
            while (Pop(item)) {}
            delete m_tail;
        }

        MPSCQueue(const MPSCQueue&) = delete;
        MPSCQueue& operator=(const MPSCQueue&) = delete;

        template <typename... Args>
        void Emplace(Args&&... args) {
            auto* node = new Node(std::forward<Args>(args)...);
            Node* prev = m_head.exchange(node, std::memory_order_acq_rel);
            prev->next.store(node, std::memory_order_release);
        }

        void Push(const T& item) {
            Emplace(item);
        }

        void Push(T&& item) {
            Emplace(std::move(item));
        }

        bool Pop(T& out) {
            Node* tail = m_tail;
            Node* next = tail->next.load(std::memory_order_acquire);

            if (next != nullptr) {
                out = std::move(next->data);
                m_tail = next;
                delete tail;
                return true;
            }
            return false;
        }

        std::optional<T> Pop() {
            T item;
            if (Pop(item)) {
                return item;
            }
            return std::nullopt;
        }

        [[nodiscard]] bool Empty() const noexcept {
            return m_tail->next.load(std::memory_order_relaxed) == nullptr;
        }

    private:
        struct Node {
            std::atomic<Node*> next{ nullptr };
            T data{};

            Node() = default;
            template <typename... Args>
            explicit Node(Args&&... args) : next(nullptr), data(std::forward<Args>(args)...) {}
        };

        alignas(CacheLineSize) std::atomic<Node*> m_head;
        alignas(CacheLineSize) Node* m_tail;
    };

    // ThreadSafeQueue: Concurrent blocking queue
    template <typename T>
    class ThreadSafeQueue {
    public:
        ThreadSafeQueue() = default;
        ~ThreadSafeQueue() { Clear(); }

        ThreadSafeQueue(const ThreadSafeQueue&) = delete;
        ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

        void Push(const T& item) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_deque.push_back(item);
            }
            m_cv.notify_one();
        }

        void Push(T&& item) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_deque.push_back(std::move(item));
            }
            m_cv.notify_one();
        }

        template <typename... Args>
        void Emplace(Args&&... args) {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_deque.emplace_back(std::forward<Args>(args)...);
            }
            m_cv.notify_one();
        }

        bool Pop(T& out) {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_deque.empty()) {
                return false;
            }
            out = std::move(m_deque.front());
            m_deque.pop_front();
            return true;
        }

        std::optional<T> Pop() {
            std::lock_guard<std::mutex> lock(m_mutex);
            if (m_deque.empty()) {
                return std::nullopt;
            }
            T item = std::move(m_deque.front());
            m_deque.pop_front();
            return item;
        }

        void Wait() {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return !m_deque.empty() || m_bStopped; });
        }

        template <typename Rep, typename Period>
        bool WaitFor(const std::chrono::duration<Rep, Period>& duration) {
            std::unique_lock<std::mutex> lock(m_mutex);
            return m_cv.wait_for(lock, duration, [this]() { return !m_deque.empty() || m_bStopped; });
        }

        bool WaitPop(T& out) {
            std::unique_lock<std::mutex> lock(m_mutex);
            m_cv.wait(lock, [this]() { return !m_deque.empty() || m_bStopped; });
            if (m_deque.empty()) {
                return false;
            }
            out = std::move(m_deque.front());
            m_deque.pop_front();
            return true;
        }

        [[nodiscard]] bool Empty() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_deque.empty();
        }

        [[nodiscard]] size_t Size() const {
            std::lock_guard<std::mutex> lock(m_mutex);
            return m_deque.size();
        }

        void Clear() {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_deque.clear();
        }

        void Stop() {
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_bStopped = true;
            }
            m_cv.notify_all();
        }

    private:
        mutable std::mutex m_mutex;
        std::deque<T> m_deque;
        std::condition_variable m_cv;
        bool m_bStopped = false;
    };

} // namespace darkfire::net

#ifdef _MSC_VER
#pragma warning(pop)
#endif
