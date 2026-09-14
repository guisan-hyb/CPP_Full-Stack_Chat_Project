## 对比解析Redis：string操作、list操作、hash操作

在 Redis 中，数据不是简单地存在数据库里的，而是被组织成不同的**数据结构**。理解它们的特性和适用场景，是学好 Redis 的关键。

对于 C++ 开发者来说，你可以用 STL 容器来做类比：
*   **String** 就像 `std::variant<int, std::string, binary_data>`（能存一切单值）。
*   **List** 就像 `std::deque`（双向链表，头尾操作极快）。
*   **Hash** 就像 `std::unordered_map<std::string, std::string>`（键值对映射表）。

下面为你详细对比解析这三种最常用的数据结构：

---

### 1. String 类型（字符串）
这是 Redis 最基础的数据类型。它不仅是文本，还能存数字、甚至是二进制数据（比如图片、序列化的 Protobuf 字节流）。**一个 Key 只对应一个 Value。**

#### 核心操作命令
*   `SET key value`：设置值
*   `GET key`：获取值
*   `INCR key` / `DECR key`：原子递增/递减（极其重要！）
*   `SETEX key seconds value`：设置值并带上过期时间

#### 适用场景
1.  **验证码 / 缓存**：`SET verify_code:12345@qq.com 9527 EX 300`（5分钟过期）。
2.  **计数器（原子性）**：比如统计网站访问量、某服务器在线人数。利用 `INCR`，即使有100个线程同时调用，Redis 底层单线程也能保证不出现并发冲突，绝不需要像 C++ 里那样加 `std::mutex`。
3.  **分布式锁**：利用 `SET key value NX PX timeout`（不存在才设置），实现跨进程的互斥锁。

#### C++ (`redis-plus-plus`) 代码演示
```cpp
// 存验证码，设置 5 分钟过期
_redis->set("verify_code:user@qq.com", "9527", std::chrono::seconds(300));

// 取出
auto val = _redis->get("verify_code:user@qq.com");
if (val) { std::cout << *val << std::endl; } // 输出 9527

// 原子递增 (统计 GateServer 启动次数)
_redis->incr("gate_server_start_count"); 
```

---

### 2. List 类型（列表）
这是一个双向链表结构。插入和删除头部（左端）或尾部（右端）的元素极快（时间复杂度 O(1)），但如果在中间插入或按索引查找，速度会很慢（O(N)）。

#### 核心操作命令
*   `LPUSH key value`：在**左侧（头部）**插入数据
*   `RPUSH key value`：在**右侧（尾部）**插入数据
*   `LPOP key`：从**左侧**弹出一个数据
*   `RPOP key`：从**右侧**弹出一个数据
*   `LRANGE key start end`：获取指定范围的元素（常用于分页查询）

#### 适用场景
1.  **消息队列（重点）**：这是你全栈聊天项目里非常常用的模式。GateServer 收到消息，`LPUSH` 进一个 List，LogicServer 用 `RPOP` 取出来处理（先进先出 FIFO 队列）。
2.  **最新消息排行榜**：比如“最新注册的10个用户”，每次新用户注册 `LPUSH`，然后 `LTRIM` 裁剪只保留前 10 个。
3.  **操作日志**：记录系统最近的操作记录。

#### C++ (`redis-plus-plus`) 代码演示
```cpp
// 生产者：GateServer 收到消息压入队列
_redis->lpush("message_queue:gate_to_logic", "user_login:1001");
_redis->lpush("message_queue:gate_to_logic", "user_login:1002");

// 消费者：LogicServer 从另一端取出处理 (先进先出)
auto msg = _redis->rpop("message_queue:gate_to_logic");
if (msg) { std::cout << "处理消息: " << *msg << std::endl; } // 输出 user_login:1001
```

---

### 3. Hash 类型（哈希表）
它相当于一个嵌套的字典。**一个 Redis Key 对应一个哈希表，哈希表里面又包含多个 Field 和 Value。** 非常适合存储对象。

#### 核心操作命令
*   `HSET key field value`：设置哈希表中的某个字段
*   `HGET key field`：获取哈希表中的某个字段
*   `HGETALL key`：获取该 Key 下所有的字段和值
*   `HDEL key field`：删除哈希表中的某个字段

#### 适用场景
1.  **存储用户/商品对象**：比如存一个用户的信息。如果用 String 存，你需要把对象 JSON 序列化成一个超长字符串，每次改一个字段（比如改年龄）都要把整个 JSON 取出来反序列化、修改、再序列化存回。**用 Hash，你可以直接修改对象的某一个字段，无需读取整个对象，极大地节省网络开销和 CPU。**
2.  **部分更新**：比如只更新用户的“在线状态”，不影响其他字段。

#### C++ (`redis-plus-plus`) 代码演示
```cpp
// 存储用户信息 (Key是用户ID，里面是各种属性)
_redis->hset("user:1001", "username", "zhangsan");
_redis->hset("user:1001", "level", "15");
_redis->hset("user:1001", "online_status", "1");

// 只读取用户的等级 (不需要把整个用户对象取出来)
auto level = _redis->hget("user:1001", "level");
if (level) { std::cout << "等级: " << *level << std::endl; }

// 只修改用户的在线状态 (不影响其他字段)
_redis->hset("user:1001", "online_status", "0");

// 获取整个用户信息表 (返回 std::unordered_map)
std::unordered_map<std::string, std::string> user_info;
_redis->hgetall("user:1001", std::inserter(user_info, user_info.begin()));
```

---

### 总结对比表

| 维度             | String                          | List                  | Hash                                                |
| :--------------- | :------------------------------ | :-------------------- | :-------------------------------------------------- |
| **C++ 模拟对象** | `std::string` / `int`           | `std::deque`          | `std::unordered_map`                                |
| **数据结构**     | 简单的 KV                       | 双向链表              | 哈希表（嵌套KV）                                    |
| **优势操作**     | 全局读写、原子计数              | 头尾插入/弹出         | 单字段读写、局部更新                                |
| **劣势操作**     | 修改内部一部分需全量替换        | 查找中间元素慢(O(N))  | 占用内存相对较大                                    |
| **项目典型用途** | 验证码、Token、分布式锁、计数器 | 消息队列、最新列表    | 用户对象信息缓存、Session                           |
| **过期机制**     | 可以对整个 Key 设过期           | 可以对整个 Key 设过期 | **只能对整个 Key 设过期，不能单独给某个字段设过期** |

**实战建议：**
在你的聊天项目里，如果 GateServer 需要验证用户的邮箱验证码，用 **String** 存。如果要保存用户登录后的基本信息（昵称、头像URL、等级等）供后续逻辑读取，用 **Hash** 存。如果 GateServer 需要把离线消息推给 LogicServer 处理，用 **List** 做消息队列。