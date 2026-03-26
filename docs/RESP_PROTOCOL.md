# RESP协议规范

**REdis Serialization Protocol** - Redis客户端与服务器通讯协议

## 5种基本数据类型

### 1. Simple String `+`
- **格式**：`+字符串\r\n`
- **用途**：状态消息（成功响应）
- **例子**：`+OK\r\n`

### 2. Error `-`
- **格式**：`-错误消息\r\n`
- **用途**：错误响应
- **例子**：`-ERR unknown command\r\n`

### 3. Integer `:`
- **格式**：`:数字\r\n`
- **用途**：整数响应（计数、ID等）
- **例子**：`:1000\r\n`、`:0\r\n`、`:-1\r\n`

### 4. Bulk String `$`
- **格式**：`$长度\r\n数据\r\n`
- **用途**：二进制安全的字符串（含任意字节）
- **长度**：字节数（不含`\r\n`）
- **例子**：
  - `$6\r\nfoobar\r\n` ← 6字节字符串"foobar"
  - `$0\r\n\r\n` ← 空字符串
  - `$-1\r\n` ← nil/null值

### 5. Array `*`
- **格式**：`*元素数\r\n[元素数个元素]\r\n`
- **用途**：命令与多元素响应
- **元素**：可以是任意类型（String、Integer、Array等）
- **例子**：
  ```
  *2\r\n$3\r\nfoo\r\n$3\r\nbar\r\n
  ← 数组，2个Bulk String元素："foo"、"bar"
  
  *3\r\n:1\r\n:2\r\n:3\r\n
  ← 数组，3个Integer元素：1、2、3
  
  *-1\r\n
  ← nil数组
  ```

## 命令格式

**客户端发送：**
```
*N\r\n$len1\r\narg1\r\n$len2\r\narg2\r\n...$lenN\r\nargN\r\n
```
其中N是参数总数（命令+参数）

**例子：**
```
SET key value
→ *3\r\n$3\r\nSET\r\n$3\r\nkey\r\n$5\r\nvalue\r\n

GET key
→ *2\r\n$3\r\nGET\r\n$3\r\nkey\r\n

DEL key1 key2 key3
→ *4\r\n$3\r\nDEL\r\n$4\r\nkey1\r\n$4\r\nkey2\r\n$4\r\nkey3\r\n
```

## 支持的命令与响应格式

| 命令 | 请求格式 | 返回格式 | 例子 |
|------|---------|---------|------|
| SET key value | `*3\r\n...` | Simple String | `+OK\r\n` |
| GET key | `*2\r\n...` | Bulk String / nil | `$5\r\nhello\r\n` 或 `$-1\r\n` |
| DEL key1 key2 ... | `*N\r\n...` | Integer | `:3\r\n` |
| EXISTS key1 key2 ... | `*N\r\n...` | Integer | `:2\r\n` |
| MGET key1 key2 ... | `*N\r\n...` | Array | `*2\r\n$5\r\nhello\r\n$5\r\nworld\r\n` |