#include "../src/util/KVStore.h"
#include "../src/util/AOF.h"
#include <filesystem>
#include <fstream>
#include <cassert>
#include <vector>
#include<iostream>
void test_aof_append_and_recover() {
    std::string path_str = std::filesystem::current_path();
    path_str+="/log.log";
    std::filesystem::remove(path_str);
    std::cerr<<"path!"<<path_str<<"\n";
    {
        KVStore<std::string> kv;
        AOF aof(path_str);
        kv.set("alpha", "1");
        aof.append("SET alpha 1");

        kv.set("beta", "2");
        aof.append("SET beta 2");

        kv.del("alpha");
        aof.append("DEL alpha");
    }

    {
        std::ifstream in(path_str);
        std::string line;
        std::vector<std::string> lines;
        while (std::getline(in, line)) {
            lines.push_back(line);
        }
        assert(lines.size() == 3);
        assert(lines[0] == "SET alpha 1");
        assert(lines[1] == "SET beta 2");
        assert(lines[2] == "DEL alpha");
    }

    {
        KVStore<std::string> kv;
        AOF aof(path_str);
        aof.recover(&kv);

        auto v1 = kv.get("alpha");
        assert(!v1.has_value());

        auto v2 = kv.get("beta");
        assert(v2.has_value());
        assert(v2.value() == "2");
        assert(kv.checkexist("beta"));
    }

    std::filesystem::remove(path_str);
}

int main() {
    test_aof_append_and_recover();
    return 0;
}
