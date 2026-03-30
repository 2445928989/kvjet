#include "../src/resp/RespEncoder.h"
#include "../src/resp/RespParser.h"
#include "../src/resp/RespValue.h"
#include <cassert>
#include <iostream>
#include <memory>

using namespace resp;

void test_simple_string() {
    std::cout << "Testing SimpleString...\n";
    RespValue val(SimpleString{"OK"});
    std::string encoded;
    encodeSimpleString(*std::get_if<SimpleString>(val.getPtr()), encoded);
    assert(encoded == "+OK\r\n");
    std::cout << "✓ SimpleString encoding works\n";
}

void test_error() {
    std::cout << "Testing Error...\n";
    RespValue val(Error{"ERR something"});
    std::string encoded;
    encodeError(*std::get_if<Error>(val.getPtr()), encoded);
    assert(encoded == "-ERR something\r\n");
    std::cout << "✓ Error encoding works\n";
}

void test_integer() {
    std::cout << "Testing Integer...\n";

    // Positive
    RespValue val(int64_t(123));
    std::string encoded;
    encodeInteger(*std::get_if<int64_t>(val.getPtr()), encoded);
    assert(encoded == ":123\r\n");

    // Negative
    RespValue val2(int64_t(-456));
    std::string encoded2;
    encodeInteger(*std::get_if<int64_t>(val2.getPtr()), encoded2);
    assert(encoded2 == ":-456\r\n");

    std::cout << "✓ Integer encoding works\n";
}

void test_bulk_string() {
    std::cout << "Testing BulkString...\n";

    // Regular
    RespValue val(BulkString{std::string("hello")});
    std::string encoded;
    encodeBulkString(*std::get_if<BulkString>(val.getPtr()), encoded);
    assert(encoded == "$5\r\nhello\r\n");

    // Nil
    RespValue val_nil(BulkString{std::nullopt});
    std::string encoded_nil;
    encodeBulkString(*std::get_if<BulkString>(val_nil.getPtr()), encoded_nil);
    assert(encoded_nil == "$-1\r\n");

    std::cout << "✓ BulkString encoding works\n";
}

void test_array() {
    std::cout << "Testing Array...\n";

    // Empty array
    Array empty_arr;
    empty_arr.value = std::vector<std::unique_ptr<RespValue>>();
    RespValue val_empty(std::move(empty_arr));
    std::string encoded_empty;
    encodeArray(*std::get_if<Array>(val_empty.getPtr()), encoded_empty);
    assert(encoded_empty == "*0\r\n");

    // Nil array
    Array nil_arr;
    RespValue val_nil(std::move(nil_arr));
    std::string encoded_nil;
    encodeArray(*std::get_if<Array>(val_nil.getPtr()), encoded_nil);
}

void test_parser_simple() {
    std::cout << "Testing RespParser (simple)...\n";

    RespParser parser;
    parser.append("+OK\r\n");
    assert(parser.hasResult());
    auto result = parser.getResult();
    assert(result.has_value());
    if (auto it = std::get_if<SimpleString>(result->getPtr())) {
        assert(it->value == "OK");
    } else {
        assert(false);
    }

    std::cout << "✓ Parser simple string works\n";
}

void test_parser_truncated() {
    std::cout << "Testing RespParser (truncated)...\n";

    RespParser parser;
    parser.append("+O"); // 不完整
    assert(!parser.hasResult());

    parser.append("K\r\n"); // 补充
    assert(parser.hasResult());
    auto result = parser.getResult();
    assert(result.has_value());
    if (auto it = std::get_if<SimpleString>(result->getPtr())) {
        assert(it->value == "OK");
    } else {
        assert(false);
    }

    std::cout << "✓ Parser truncated handling works\n";
}

void test_parser_integer() {
    std::cout << "Testing RespParser (integer)...\n";

    RespParser parser;
    parser.append(":123\r\n");
    assert(parser.hasResult());
    auto result = parser.getResult();
    assert(result.has_value());
    if (auto it = std::get_if<int64_t>(result->getPtr())) {
        std::cout << "fuck???????????????????????????????????????????????????????????" << *it << '\n';
        assert(*it == 123);
    } else {
        assert(false);
    }

    // Negative
    RespParser parser2;
    parser2.append(":-456\r\n");
    assert(parser2.hasResult());
    auto result2 = parser2.getResult();
    assert(result2.has_value());
    if (auto it = std::get_if<int64_t>(result2->getPtr())) {
        assert(*it == -456);
    } else {
        assert(false);
    }

    std::cout << "✓ Parser integer works\n";
}

void test_parser_bulk_string() {
    std::cout << "Testing RespParser (bulk string)...\n";

    RespParser parser;
    parser.append("$5\r\nhello\r\n");
    assert(parser.hasResult());
    auto result = parser.getResult();
    assert(result.has_value());
    if (auto it = std::get_if<BulkString>(result->getPtr())) {
        assert(it->value.has_value());
        assert(*it->value == "hello");
    } else {
        assert(false);
    }

    // Nil
    RespParser parser_nil;
    parser_nil.append("$-1\r\n");
    assert(parser_nil.hasResult());
    auto result_nil = parser_nil.getResult();
    assert(result_nil.has_value());
    if (auto it = std::get_if<BulkString>(result_nil->getPtr())) {
        assert(!it->value.has_value());
    } else {
        assert(false);
    }

    std::cout << "✓ Parser bulk string works\n";
}

void test_parser_array() {
    std::cout << "Testing RespParser (array)...\n";

    RespParser parser;
    parser.append("*2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n");
    assert(parser.hasResult());
    auto result = parser.getResult();
    assert(result.has_value());
    if (auto it = std::get_if<Array>(result->getPtr())) {
        assert(it->value.has_value());
        assert(it->value->size() == 2);
    } else {
        assert(false);
    }

    std::cout << "✓ Parser array works\n";
}

void test_parser_empty_array() {
    std::cout << "Testing RespParser (empty array)...\n";

    RespParser parser;
    parser.append("*0\r\n");
    assert(parser.hasResult());
    auto result = parser.getResult();
    assert(result.has_value());
    if (auto it = std::get_if<Array>(result->getPtr())) {
        assert(it->value.has_value());
        assert(it->value->size() == 0);
    } else {
        assert(false);
    }

    std::cout << "✓ Parser empty array works\n";
}

int main() {
    std::cout << "=== RESP Protocol Tests ===\n\n";

    test_simple_string();
    test_error();
    test_integer();
    test_bulk_string();
    test_array();

    std::cout << "\n";

    test_parser_simple();
    test_parser_truncated();
    test_parser_integer();
    test_parser_bulk_string();
    test_parser_array();
    test_parser_empty_array();

    std::cout << "\n=== All RESP tests passed! ===\n";
    return 0;
}
