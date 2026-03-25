# kvjet 使用说明

## 编译

```bash
cd /data/kvjet
mkdir build
cd build
cmake ..
make
```

## 运行测试

```bash
cd /data/kvjet/build
ctest --output-on-failure
```

服务器会自动启动，测试完成后自动清理。
