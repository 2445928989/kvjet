// RespParser.h
#pragma once

#include "RespValue.h"
#include <optional>
#include <stack>
// RESP解码器
// 利用一个状态栈保留已解析信息，支持截断的数据字符串
namespace resp {
    class RespParser {
    public:
        // 添加待解析串并尝试解析
        void append(const std::string &app);
        // 重置解析器状态
        void reset();
        // 解析是否完成
        bool hasResult();
        // 获取解析后的对象，若解析未完成则返回nullopt
        std::optional<RespValue> getResult();
        RespParser();

    private:
        // 栈内状态
        struct RespState {
            RespValue value;
            // 总共有几个对象/原始字符串总长度
            size_t total;
            RespState(RespValue &&value, size_t tot) : value(std::move(value)), total(tot) {}
            RespState() : value(RespValue()), total(0) {}
        };
        // 状态机
        enum ParseState {
            WaitType,  // 等待符号
            ReadSize,  // 读大小直到\r
            WaitLFSZ,  // 等待大小结束的\n
            WaitNil,   // 等待-1的1
            WaitNilCR, // 等待-1后面的\r
            ReadLine,  // 读正文直到\r
            WaitLF,    // 等待正文后的\n
            ReadBulk   // 读bulk正文直到\r
        } state;
        void push(RespState state);

        // 正在读取的对象
        RespState node;
        // 状态栈，用于保留截断字符串前的状态
        std::stack<RespState> stateStack;
    };
};