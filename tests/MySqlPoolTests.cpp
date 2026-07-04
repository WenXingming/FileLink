#include "MySqlPool.h"
#include "MySqlConfig.h"

#include <gtest/gtest.h>

#include <cstdlib>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace {

filelink::MySqlConfig test_options() {
    filelink::MySqlConfig options;
    options.user = "filelink";
    options.database = "filelink";
    options.poolSize = 2; // 测试时用比较小的连接池

    const char* password = std::getenv("FILELINK_TEST_MYSQL_PASSWORD");
    if (password != nullptr) {
        options.password = password;
    }
    const char* port = std::getenv("FILELINK_TEST_MYSQL_PORT");
    if (port != nullptr) {
        options.port = static_cast<uint16_t>(std::stoi(port));
    }
    return options;
}

bool mysql_test_enabled() {
    return std::getenv("FILELINK_TEST_MYSQL_PASSWORD") != nullptr;
}

} // namespace

TEST(MySqlPoolTest, RejectsZeroPoolSize) {
    filelink::MySqlConfig options;
    options.poolSize = 0;

    EXPECT_THROW(filelink::MySqlPool pool(options), std::invalid_argument);
}

TEST(MySqlPoolTest, AcquireAndReleaseConnections) {
    if (!mysql_test_enabled()) {
        GTEST_SKIP() << "未设置 FILELINK_TEST_MYSQL_PASSWORD";
    }

    filelink::MySqlConfig options = test_options();
    options.poolSize = 2;
    filelink::MySqlPool pool(options);

    {
        auto conn1 = pool.acquire();
        EXPECT_TRUE(conn1->ping());

        auto conn2 = pool.acquire();
        EXPECT_TRUE(conn2->ping());

        // 此时池子已空。离开作用域后，conn1 和 conn2 将被自动归还
    }

    // 再次 acquire 应该能立刻成功，因为上面已经归还
    auto conn3 = pool.acquire();
    EXPECT_TRUE(conn3->ping());
}

TEST(MySqlPoolTest, ThreadSafeAcquire) {
    if (!mysql_test_enabled()) {
        GTEST_SKIP() << "未设置 FILELINK_TEST_MYSQL_PASSWORD";
    }

    filelink::MySqlConfig options = test_options();
    options.poolSize = 3;
    filelink::MySqlPool pool(options);

    auto worker = [&pool]() {
        for (int i = 0; i < 5; ++i) {
            auto conn = pool.acquire();
            EXPECT_TRUE(conn->ping());
            // 稍作延迟模拟业务，迫使连接池竞争
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
        }
    };

    std::vector<std::thread> threads;
    // 启动比连接池容量更多的线程，测试阻塞和唤醒逻辑
    for (int i = 0; i < 5; ++i) {
        threads.emplace_back(worker);
    }

    for (auto& t : threads) {
        t.join();
    }
}
