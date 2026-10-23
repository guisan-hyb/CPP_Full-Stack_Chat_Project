## Server架构分析2.0：node.js完成邮箱验证服务

我按当前工作区代码整理了一版架构。现在这个项目已经是“**三端 + 一份共享协议 + 三套依赖管理**”的形态：

```
Qt 客户端  --HTTP/JSON-->  GateServer(C++)  --gRPC-->  VerifyServer(Node.js)  --SMTP-->  邮箱
    :8080                                       :50051                     smtp.qq.com:465
```

## 一、当前组件划分

| 组件     | 目录                         | 技术栈                               | 职责                                                         | 构建方式                       |
| -------- | ---------------------------- | ------------------------------------ | ------------------------------------------------------------ | ------------------------------ |
| 客户端   | `client/`                    | Qt6 Widgets、QtNetwork               | 登录/注册界面、HTTP 请求、响应分发与提示                     | QMake (`client.pro`)           |
| 网关服务 | `server/GateServer/`         | C++17、Boost.Beast/Asio、gRPC C++    | 对外 HTTP 接口、路由、调用验证服务、返回 JSON                | CMake + vcpkg                  |
| 验证服务 | `server/VerifyServer/`       | Node.js、`@grpc/grpc-js`、nodemailer | 接收邮箱、生成验证码、发送邮件                               | npm (`package.json`)           |
| 共享协议 | `server/proto/message.proto` | protobuf                             | 定义 `VerifyService/GetVerifyCode` 及请求响应结构            | CMake 代码生成 / Node 动态加载 |
| 协议产物 | `build/generated/`           | 自动生成                             | `message.pb.*`、`message.grpc.pb.*`，封装成 `chat_proto` 供 C++ 使用 | CMake custom command           |

## 二、各层内部结构

**客户端层**

- `mainwindow` / `logindialog` / `registerdialog`：界面与用户交互。
- `httpmgr`：统一封装 HTTP POST，通过信号把响应分发给对应模块。
- `global`：`ReqId`、`Modules`、`ErrorCodes`、`gate_url_prefix` 等全局定义。
- `config.ini`：GateServer 地址配置。

**GateServer 层**

- `GateServer.cpp`：读取配置、创建 io_context、启动监听。
- `CServer`：接受 TCP 连接，为每个连接创建 `HttpConnection`。
- `HttpConnection`：HTTP 报文读写、GET 参数解析、超时处理。
- `LogicSystem`：注册 GET/POST 路由。当前接口是 `POST /get_verifycode`。
- `VerifyGrpcClient`：作为 gRPC 客户端，连接 `127.0.0.1:50051`。
- `ConfigMgr`：解析 `config.ini`，目前包含 GateServer 端口和 VerifyServer 端口。

**VerifyServer 层**

- `server.js`：注册 `GetVerifyCode` 并监听 `0.0.0.0:50051`。
- `proto.js`：从 `../proto/message.proto` 动态加载协议，与 C++ 生成代码共用同一份 proto。
- `email.js`：通过 QQ 邮箱 SMTP 发送验证码。
- `config.js` / `config.json`：邮箱账号、授权码、MySQL/Redis 配置。
- `const.js`：错误码 `Success / RedisErr / Exception`。

## 三、一次“获取验证码”的完整链路

1. 客户端校验邮箱格式，`POST http://localhost:8080/get_verifycode`，Body 为 `{"email": "..."}`。
2. GateServer 的 `LogicSystem` 解析 JSON，打印邮箱。
3. `VerifyGrpcClient` 通过 gRPC 调用 `VerifyService/GetVerifyCode`。
4. Node 端 `server.js` 生成 UUID 短验证码并调用 `email.js` 发送邮件。
5. 发送成功返回 `error = 0`，异常返回 `error = 2`。
6. GateServer 把 `error` 和 `email` 装配成 JSON 返回给客户端。
7. 客户端根据 `error` 决定显示“验证码已发送”还是错误提示。

这条链路现在已经闭合，上次“参数错误”的直接原因也已经解决：Node 服务实现了与当前 proto 一致的 `VerifyService/GetVerifyCode`，GateServer 也从 `0.0.0.0` 改为连接 `127.0.0.1:50051`。

## 四、当前架构中还没有打通的部分

1. **验证码没有存储，注册环节接不上。**
   `server.js` 只是生成并发送验证码，没有写入 Redis，也没有把 `code` 返回给 GateServer。响应中的 `code` 字段一直为空。后续 `user_register` 接口没有办法校验用户填写的验证码是否正确、是否过期。配置里虽然预留了 Redis 信息，但目前 `redis.js` 尚未实现。
2. **注册功能还停留在“获取验证码”阶段。**
   客户端只有发送验证码的请求；GateServer 只有 `/get_verifycode` 和 `/get_test` 两条路由。一个完整的注册闭环至少还需要：
   - Node 端把验证码以 `code_<email>` 为键写入 Redis，并设置过期时间；
   - GateServer 增加 `/user_register`；
   - 注册时从 Redis 取出验证码做比对，并执行业务入库；
   - 客户端补充注册请求与结果处理。
3. **配置使用还不彻底。**
   - C++ 侧：`config.ini` 有 `[VerifyServer] Port = 50051`，但 `VerifyGrpcClient.cpp` 仍然硬编码 `127.0.0.1:50051`，没有真正使用 `ConfigMgr`。
   - Node 侧：`config.json` 中的邮箱账号没有完全作为唯一配置源，`server.js` 里的发件人地址是另外硬编码的。
   - `config.js` 用 `fs.readFileSync('config.json')` 按“当前工作目录”读取，只有从 `server/VerifyServer` 目录启动 Node 才正常。建议改成 `path.join(__dirname, 'config.json')`。
4. **密钥管理存在风险。**
   `server/VerifyServer/config.json` 中直接保存了 QQ 邮箱授权码，以及 MySQL/Redis 密码。当前 `.gitignore` 只忽略了 `node_modules/`，没有忽略该文件，存在提交到仓库的风险。建议：
   - 将 `config.json` 加入 `.gitignore`；
   - 增加 `config.example.json` 作为模板；
   - 敏感信息改用环境变量注入。
5. **构建系统是三种并存。**
   客户端 QMake、GateServer CMake、VerifyServer npm，这是合理的分工，但根目录缺少统一的启动说明或脚本。建议加一个根目录 `README.md`，把三件事写清楚：
   - `npm install` 与 `npm run serve`（VerifyServer）
   - CMake 配置并启动 GateServer
   - qmake 或 Qt Creator 构建并启动客户端

## 五、推荐的下一步架构形态

```
client/
    Qt 界面 + HttpMgr + 注册/登录请求

server/
    proto/
        message.proto          <-- 唯一协议源
    GateServer/                <-- HTTP 网关 + gRPC 客户端
    VerifyServer/              <-- Node gRPC 服务 + 邮件发送
        config.example.json
        redis.js               <-- 后续：验证码存取
```

短期内最值得做的三件事是：**Node 端补 Redis 验证码存取、GateServer 增加注册路由、把 config.json 移出 Git 并模板化**。这样从“能发验证码”到“能完成注册”的闭环就真正建立起来了。

