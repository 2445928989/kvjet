#include "../src/util/KVStore.h"
#include <cassert>
#include <iostream>
#include <thread>
#include <vector>
#include <string> 

void test_basic_operations() {
    std::cout << "Testing basic operations...\n";
    KVStore<std::string> store;

    // set/get
    store.set("key1", "value1");
    auto result = store.get("key1");
    assert(result.has_value());
    assert(result.value() == "value1");
    std::cout << "set/get works\n";

    // non-existent key
    auto result2 = store.get("nonexistent");
    assert(!result2.has_value());
    std::cout << "get non-existent key returns nullopt\n";

    // multiple keys
    store.set("key2", "value2");
    store.set("key3", "value3");
    assert(store.get("key2").value() == "value2");
    assert(store.get("key3").value() == "value3");
    std::cout << "multiple keys work\n";

    // erase
    bool erased = store.del("key2");
    assert(erased);
    assert(!store.get("key2").has_value());
    std::cout << "erase works\n";

    // erase non-existent
    bool erased2 = store.del("nonexistent");
    assert(!erased2);
    std::cout << "erase non-existent key returns false\n";

    // overwrite
    store.set("key1", "new_value");
    assert(store.get("key1").value() == "new_value");
    std::cout << "overwrite value works\n";

    // checkexist
    assert(store.checkexist("key1") == true);
    assert(store.checkexist("key2") == false);
    std::cout << "checkexist works\n";
}

void test_concurrent_reads() {
    std::cout << "Testing concurrent reads...\n";
    KVStore<std::string> store;

    for (int i = 0; i < 100; i++) {
        store.set("key" + std::to_string(i), "value" + std::to_string(i));
    }

    std::vector<std::thread> threads;
    std::atomic<bool> all_success{true};

    for (int t = 0; t < 10; t++) {
        threads.emplace_back([&store, &all_success, t]() {
            for (int i = 0; i < 100; i++) {
                auto result = store.get("key" + std::to_string(i));
                if (!result.has_value() || result.value() != "value" + std::to_string(i)) {
                    all_success = false;
                }
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    assert(all_success);
    std::cout << "concurrent reads work\n";
}

void test_concurrent_writes() {
    std::cout << "Testing concurrent writes and reads...\n";
    KVStore<std::string> store;
    std::vector<std::thread> threads;

    for (int t = 0; t < 5; t++) {
        threads.emplace_back([&store, t]() {
            for (int i = 0; i < 20; i++) {
                store.set("key_" + std::to_string(t) + "_" + std::to_string(i),
                          "val_" + std::to_string(i));
            }
        });
    }

    for (int t = 0; t < 5; t++) {
        threads.emplace_back([&store, t]() {
            for (int i = 0; i < 20; i++) {
                store.get("key_" + std::to_string(t) + "_" + std::to_string(i));
            }
        });
    }

    for (auto &th : threads) {
        th.join();
    }

    std::cout << "concurrent reads and writes work\n";
}

void test_large_dataset() {
    std::cout << "Testing with large dataset...\n";
    KVStore<std::string> store;

    const int COUNT = 1000;
    for (int i = 0; i < COUNT; i++) {
        store.set("key" + std::to_string(i), "value" + std::to_string(i));
    }

    for (int i = 0; i < COUNT; i++) {
        auto result = store.get("key" + std::to_string(i));
        assert(result.has_value());
        assert(result.value() == "value" + std::to_string(i));
    }

    std::cout << "large dataset with rehashing works\n";
}

int main() {
    try {
        test_basic_operations();
        test_concurrent_reads();
        test_concurrent_writes();
        test_large_dataset();
        std::cout << "\nAll tests passed!\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "Test failed with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception\n";
        return 1;
    }
}