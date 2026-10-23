## GateServer 架构分析

```cpp
main()
├── io_context (事件循环核心)
├── CServer (服务器监听器)
│ └── HttpConnection (连接管理器)
│ └── LogicSystem (业务路由处理器)
客户端连接

→ CServer::async_accept (接受连接)
→ 创建HttpConnection管理连接
→ HttpConnection::async_read (读取HTTP数据)
→ HandleReq() (解析请求类型)
→ LogicSystem路由分发 (GET/POST)
→ 执行具体业务处理函数
→ WriteResponse() (发送响应)
→ 关闭连接
```



# GateServer 模块架构文档

## 概述
GateServer 是一个基于 **Boost.Beast** 和 **Asio** 的异步 HTTP 服务器，采用单线程事件循环模型，支持 GET/POST 请求路由，并内置 JSON 解析、URL 编解码、超时检测等功能。整体遵循 **单例** + **责任链** 设计，模块间低耦合。

---

## 模块划分

| 模块           | 文件                    | 职责                                                         |
| -------------- | ----------------------- | ------------------------------------------------------------ |
| **服务器入口** | `GateServer.cpp`        | 启动 `io_context`，注册信号处理，创建并启动 `CServer`        |
| **监听器**     | `CSever.h/.cpp`         | 监听端口，接受新连接，为每个连接生成 `HttpConnection` 对象   |
| **连接管理**   | `HttpConnection.h/.cpp` | 管理单个 HTTP 连接的生命周期：读取请求、解析 URL/参数、调用业务逻辑、写回响应、超时检测 |
| **业务路由**   | `LogicSystem.h/.cpp`    | 单例类，维护 GET/POST 路由表，提供注册与分发接口             |
| **工具类**     | `Singleton.h`           | 线程安全的单例模板（基于 `std::call_once`）                  |
| **公共头**     | `const.h`               | 引入 Boost 命名空间、错误码枚举、JSON 别名等                 |

---

## 核心类设计

### 1. `CServer`（监听器）
- 继承 `std::enable_shared_from_this<CServer>`
- 成员：`io_context&`、`tcp::acceptor`、`tcp::socket`
- 方法：`Start()` 循环调用 `async_accept`，每次成功接收后创建 `HttpConnection` 并调用其 `Start()`

### 2. `HttpConnection`（连接处理器）
- 继承 `std::enable_shared_from_this<HttpConnection>`
- 成员：
  - `tcp::socket _socket` —— 底层 socket
  - `flat_buffer _buffer` —— 读缓存（8KB）
  - `http::request<dynamic_body> _request` —— 请求对象
  - `http::response<dynamic_body> _response` —— 响应对象
  - `steady_timer _deadline` —— 超时定时器（默认60秒）
  - `std::string _get_url` 与 `unordered_map _get_params` —— GET 请求解析结果
- 关键方法：
  - `Start()`：异步读取请求头+体
  - `HandleReq()`：根据请求方法（GET/POST）分发至 `LogicSystem`
  - `PreParseGetParam()`：解析 URL 中的查询字符串，填充 `_get_params`
  - `WriteResponse()`：异步写入响应并关闭 socket
  - `CheckDeadline()`：超时后强制关闭连接

### 3. `LogicSystem`（业务路由）
- 单例类（`Singleton<LogicSystem>`）
- 成员：
  - `unordered_map<string, HttpHandler> _get_handlers`
  - `unordered_map<string, HttpHandler> _post_handlers`
- 方法：
  - `RegGet(path, handler)` / `RegPost(path, handler)` —— 注册路由
  - `HandleGet(path, connection)` / `HandlePost(path, connection)` —— 分发请求
- 构造函数中内置了两个示例路由：
  - `GET /get_test`：回显所有查询参数
  - `POST /get_varifycode`：解析 JSON 体，提取 `email` 字段并返回

### 4. `Singleton<T>`
- 模板类，使用 `std::once_flag` 和 `std::call_once` 保证线程安全的延迟初始化
- 提供静态 `GetInst()` 返回 `shared_ptr<T>`

---

## 请求处理流程

```
客户端发起 HTTP 请求
         │
         ▼
┌────────────────────────┐
│   CServer::Start()     │
│  async_accept 等待连接 │
└───────────┬────────────┘
            │ 连接建立
            ▼
┌────────────────────────┐
│ 创建 HttpConnection    │
│ 调用 Start()           │
└───────────┬────────────┘
            │ async_read 读取请求
            ▼
┌────────────────────────┐
│ HttpConnection::       │
│ HandleReq()            │
│  - 判断 method         │
│  - GET 解析 URL 参数    │
│  - POST 直接读取 body   │
└───────────┬────────────┘
            │ 调用 LogicSystem
            ▼
┌────────────────────────┐
│ LogicSystem::          │
│ HandleGet/Post()       │
│  - 查找路由表          │
│  - 执行 handler        │
└───────────┬────────────┘
            │ handler 填充 _response
            ▼
┌────────────────────────┐
│ HttpConnection::       │
│ WriteResponse()        │
│  - async_write 回包    │
│  - shutdown send       │
│  - 取消定时器          │
└────────────────────────┘
```

---

## 关键交互细节

- **超时检测**：`_deadline` 定时器在 `Start()` 中设置，若60秒内未完成读写则主动关闭 socket。
- **URL 解码**：`PreParseGetParam()` 调用 `UrlDecode` 处理 `%xx` 和 `+` 转义。
- **JSON 处理**：POST 示例路由使用 `nlohmann::json` 解析 body，捕获解析异常并返回错误码 `Error_Json`。
- **连接关闭**：每次响应后关闭发送端（`shutdown_send`），等待客户端关闭，之后取消定时器。
- **错误处理**：所有异步回调中捕获 `std::exception`，避免服务崩溃。

---

## 扩展点

- **新增路由**：在 `LogicSystem` 构造函数或外部调用 `RegGet/RegPost` 注册自定义处理函数。
- **修改超时**：调整 `HttpConnection` 中 `_deadline` 的构造参数（当前60秒）。
- **支持 HTTPS**：可替换 `tcp::socket` 为 `ssl::stream`，并修改读写操作。
- **多线程**：当前为单线程（`io_context` 运行一个线程），可增加线程数，但需注意共享数据的线程安全（当前 `LogicSystem` 只读路由表，写入仅在构造阶段，可安全共享）。

---

## 依赖库

- Boost.Beast / Boost.Asio
- Boost.System
- nlohmann/json
- C++17 标准库（`std::shared_ptr`, `std::function`, `std::unordered_map`, `std::once_flag` 等）

---

## 启动示例

```bash
./GateServer
# 输出：Gate Server listen on port: 8080
```

**测试 GET**：
```bash
curl "http://localhost:8080/get_test?name=John&age=30"
```
响应：
```
receive get_test req
param 1 key is: name param 1 value is: John
param 2 key is: age param 2 value is: 30
```

**测试 POST**：
```bash
curl -X POST http://localhost:8080/get_varifycode -H "Content-Type: application/json" -d '{"email":"test@example.com"}'
```
响应：
```json
{"error":0,"email":"test@example.com"}
```





