//
// Created by 缪浩楚 on 2024/6/25.
//
#include "gtest/gtest.h"
#include "threadPool.h"

// 测试 ITask 接口的析构函数
//TEST(ITaskTest, Destructor) {
//    class MockTask : public ITask<int> {
//    public:
//        void run() override {}
//        [[nodiscard]] int get() override { return 0; }
//    };
//
//    auto* mock_task = new MockTask();
//    delete mock_task;
//}

// 测试 Task 类的构造函数和 run 方法
//TEST(TaskTest, Run) {
////    Task<std::function<int()>, int> task([]{ return 1; });
//    auto task = TaskFactory::createTask([](){ return 1;});
//    task->run();
//    EXPECT_EQ(1, task->get());
//}
//
//// 测试 Task 类的异常处理
//TEST(TaskTest, ExceptionHandling) {
//    auto task = TaskFactory::createTask([]{ throw std::runtime_error("error"); });
//    task->run();
//    EXPECT_THROW(task->get(), std::runtime_error);
//}
//

class ThreadPoolTest : public ::testing::Test {
protected:
    void SetUp() override {
        pool = &ThreadPool::instance();
    }

    void TearDown() override {
        pool->clear();
    }

    ThreadPool* pool;
};

TEST_F(ThreadPoolTest, TaskExecution) {
    auto future1 = TaskFactory::createTask([]() { return 1; }, HIGH);
    auto future2 = TaskFactory::createTask([]() { return 2; }, MEDIUM);
    auto future3 = TaskFactory::createTask([]() { return 3; }, LOW);

    pool->push(future1);
    pool->push(future2);
    pool->push(future3);

    EXPECT_EQ(future1->get(), 1);
    EXPECT_EQ(future2->get(), 2);
    EXPECT_EQ(future3->get(), 3);
}

TEST_F(ThreadPoolTest, TaskPriority) {
    std::atomic<int> counter{0};

    auto future1 = TaskFactory::createTask([&counter]() { std::this_thread::sleep_for(std::chrono::milliseconds(100)); counter++; return 1; }, LOW);
    auto future2 = TaskFactory::createTask([&counter]() { counter++; return 2; }, HIGH);

    pool->push(future1);
    pool->push(future2);

    EXPECT_EQ(future2->get(), 2); // high priority task should execute first
    EXPECT_EQ(future1->get(), 1);
    EXPECT_EQ(counter.load(), 2);
}

TEST_F(ThreadPoolTest, TaskCount) {
    auto future1 = TaskFactory::createTask([]() { return 1; }, HIGH);
    auto future2 = TaskFactory::createTask([]() { return 2; }, MEDIUM);
    auto future3 = TaskFactory::createTask([]() { return 3; }, LOW);

    pool->push(future1);
    pool->push(future2);
    pool->push(future3);

    EXPECT_EQ(pool->getTasksTotal(), 3);
    future1->get();
    future2->get();
    future3->get();
    EXPECT_EQ(pool->getTasksTotal(), 0);
}

TEST_F(ThreadPoolTest, TaskClear) {
    auto future1 = TaskFactory::createTask([]() { std::this_thread::sleep_for(std::chrono::milliseconds(100)); return 1; }, LOW);
    auto future2 = TaskFactory::createTask([]() { return 2; }, HIGH);

    pool->push(future1);
    pool->push(future2);

    pool->clear();

    EXPECT_EQ(pool->getTasksTotal(), 0);
}


TEST(TaskTest, VoidReturnType) {
    bool executed = false;
    auto task = TaskFactory::createTask([&executed]{executed = true;});
    task->run(); // 对于 void 类型，这里不期望有返回值
    EXPECT_TRUE(executed);
}



int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}