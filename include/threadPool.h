//
// Created by 缪浩楚 on 2024/6/24.
//
#include <functional>
#include <memory>
#include <future>
#include <array>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <thread>

#ifndef THREADPOOL_THREADPOOL_H
#define THREADPOOL_THREADPOOL_H

enum TASK_PRIORITY {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2
};

class ITaskBase {
public:
    virtual ~ITaskBase() = default;
    virtual void run() = 0;
//    virtual int get() = 0;

    inline TASK_PRIORITY getPriority() const {
        return m_taskPriority;
    }

    void setPriority(TASK_PRIORITY p) {
        m_taskPriority = p;
    }

protected:
    TASK_PRIORITY m_taskPriority = HIGH;
};

template <typename R>
class ITask : public ITaskBase {
public:
    virtual ~ITask() = default;
    virtual R get() = 0;

    ITask& operator=(const ITask&) = delete;
    ITask(const ITask&) = delete;
    void run() override = 0;

protected:
    ITask() = default;
};

template<typename F, typename R = std::invoke_result_t<F>>
class Task : public ITask<R> {
public:
    Task() : m_func([] {}) {
        this->setPriority(HIGH);
    }
    explicit Task(F&& func, TASK_PRIORITY priority = HIGH) {
        this->setPriority(priority);
        m_func = std::forward<F>(func);
    }
    ~Task() = default;

    void run() override {
        try {
            if constexpr (std::is_void_v<R>) {
                m_func();
                m_result.set_value(); // For void type
            } else {
                m_result.set_value(m_func());
            }
        } catch (...) {
            m_result.set_exception(std::current_exception());
        }
    }

    R get() override {
        return m_result.get_future().get();
    }

private:
    std::function<R()> m_func;
    std::promise<R> m_result;
};

class TaskFactory {
public:
    template<typename F>
    static auto createTask(F&& func, TASK_PRIORITY priority = HIGH) {
        using R = std::invoke_result_t<F>;
        return std::make_shared<Task<F, R>>(std::forward<F>(func), priority);
    }

    TaskFactory() = delete;
    ~TaskFactory() = delete;
};

class IThreadPool {
protected:
    IThreadPool() = default;

public:
    using concurrency_t = unsigned int;
    using size_t = std::size_t;
    virtual ~IThreadPool() = default;
    virtual void push(std::shared_ptr<ITaskBase> task) = 0;
};

class ThreadPool : public IThreadPool {
public:
    static ThreadPool& instance() {
        static ThreadPool threadPool;
        return threadPool;
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    void init() {
        m_num_threads = std::thread::hardware_concurrency();
        init_workers();
    }

    void push(std::shared_ptr<ITaskBase> task) override {
        {
            std::unique_lock<std::mutex> lock(tasks_mutex);
            m_queues[task->getPriority()].push(task);
        }
        m_eventVar.notify_one();
    }

    [[nodiscard]] size_t getTasksWaiting() {
        std::unique_lock<std::mutex> lock(tasks_mutex);
        size_t total = 0;
        for (const auto& queue : m_queues) {
            total += queue.size();
        }
        return total;
    }

    size_t getTasksRunning() {
        std::unique_lock<std::mutex> lock(tasks_mutex);
        return m_running;
    }

    size_t getTasksTotal() {
        return getTasksRunning() + getTasksWaiting();
    }

    void clear() {
        std::unique_lock<std::mutex> lock(tasks_mutex);
        for (auto& queue : m_queues) {
            while (!queue.empty()) {
                queue.pop();
            }
        }
    }

    [[nodiscard]] bool empty() {
        std::unique_lock<std::mutex> lock(tasks_mutex);
        for (const auto& queue : m_queues) {
            if (!queue.empty()) {
                return false;
            }
        }
        return true;
    }

private:
    ThreadPool() {
        init();
    }
    explicit ThreadPool(int numThreads) : m_num_threads(numThreads) {
        init_workers();
    }
    ~ThreadPool() override {
        stop();
    }

    void init_workers() {
        for (size_t i = 0; i < m_num_threads; ++i) {
            m_threads.emplace_back([this]() {
                while (true) {
                    std::shared_ptr<ITaskBase> task;
                    {
                        std::unique_lock<std::mutex> lock(this->tasks_mutex);
                        this->m_eventVar.wait(lock, [this] {
                            return this->m_stopping || !this->allQueuesEmpty();
                        });
                        if (this->m_stopping && this->allQueuesEmpty()) return;
                        task = popOne();
                        ++m_running;
                    }
                    if (task) {
                        task->run();
                        {
                            std::unique_lock<std::mutex> lock(this->tasks_mutex);
                            --m_running;
                        }
                    }
                }
            });
        }
    }

    [[nodiscard]] bool allQueuesEmpty() const {
        for (const auto& queue : m_queues) {
            if (!queue.empty()) return false;
        }
        return true;
    }

    void stop() noexcept {
        {
            std::unique_lock<std::mutex> lock(tasks_mutex);
            m_stopping = true;
        }
        m_eventVar.notify_all();
        for (auto& thread : m_threads) {
            thread.join();
        }
    }

    std::shared_ptr<ITaskBase> popOne() {
        for (int i = HIGH; i >= LOW; --i) {
            if (!m_queues[i].empty()) {
                auto task = m_queues[i].front();
                m_queues[i].pop();
                return task;
            }
        }
        return nullptr;
    }

    concurrency_t m_num_threads = 0;
    size_t m_running = 0;
    std::mutex tasks_mutex;
    std::condition_variable m_eventVar;
    bool m_stopping = false;

    std::array<std::queue<std::shared_ptr<ITaskBase>>, 3> m_queues{};
    std::vector<std::thread> m_threads{};
};

#endif //THREADPOOL_THREADPOOL_H

