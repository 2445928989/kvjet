## 功能需求

### 命令支持

- **SET key value** - 设置键值对
- **GET key** - 获取值
- **DEL key1 key2 ...** - 删除键（支持多个）
- **EXISTS key1 key2 ...** - 检查键存在（返回计数）
- **MGET key1 key2 ...** - 批量获取值

### 响应格式

客户端应将RESP格式转换为以下格式显示：

#### 成功操作
```
SET key value      →  OK
GET key            →  "value" 或 (nil)
DEL key1 key2      →  (integer) 2
EXISTS k1 k2 k3    →  (integer) 3
```

#### 错误处理
```
GET (ERR missing arguments)  →  (error) ERR unknown command
SET key                       →  (error) ERR wrong number of arguments
```

#### 数组返回
```
MGET k1 k2 k3
→ 1) "value1"
  2) "value2"
  3) (nil)
```

## 输出规范

| 类型 | 格式 |
|------|------|
| 字符串 | `"value"` (含双引号) |
| 整数 | `(integer) N` |
| OK状态 | `OK` |
| 错误 | `(error) ERR message` |
| nil值 | `(nil)` |
| 数组 | 编号列表，每行一个元素 |