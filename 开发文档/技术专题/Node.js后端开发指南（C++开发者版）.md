# Node.js 后端开发指南（C++ 开发者版）

> 目标读者：熟悉 C++、网络编程和并发基础，但 JavaScript / Node.js 零基础的后端开发者。
>
> 项目背景：这是一个 Qt 客户端 + C++ GateServer + Node.js VerifyServer 的全栈聊天项目。GateServer 用 Boost.Beast/Asio 提供 HTTP 服务，通过 gRPC 调用 Node.js 的 VerifyServer；VerifyServer 负责生成验证码、写入 Redis，并通过 SMTP 发送邮件。
>
> 阅读方式：建议从第 1 章顺序读到第 9 章；如果你赶时间，只想把当前 VerifyServer 跑通并修好，可以先读第 5 章和第 6 章。
>
> 代码约定：文中代码默认使用 TypeScript + ESM；示例以 Windows 开发和 Linux 部署为前提；命令在 PowerShell 和 Bash 下分别标注。

---

## 0. 这份指南解决什么问题

这份指南不是一本泛泛的 Node.js 语法书，而是一份面向你当前项目的工程化路线图。它要帮你完成三件事：

1. **建立正确的运行时模型**：知道 Node.js 为什么是事件驱动、什么代码会阻塞、哪些地方可以并发、哪些地方不能。
2. **把 VerifyServer 讲透**：逐个文件解释现有 JS 实现，指出当前代码里的真实问题，并给出一份可运行的 TypeScript 重构版本。
3. **具备后端工程化能力**：配置管理、日志、错误处理、测试、Redis、Docker、Linux 部署、安全与性能。

先给一个最重要的结论，后面所有内容都围绕它展开：

> **Node.js 的性能来自“不要阻塞事件循环”，而不是“单线程很能打”。你的 C++ 并发经验在这里不会浪费，但表达方式会从线程、锁、回调，变成事件循环、Promise、背压和 Worker。**

---

## 1. 从 C++ 到 Node.js：先换思维

### 1.1 编译器、运行时和“程序入口”

在 C++ 里，代码先编译成机器码或字节码，程序从 `main()` 开始执行；类型在编译期确定，链接期解决符号依赖。

在 Node.js 里，代码由 V8 直接执行 JavaScript；TypeScript 先被转译成 JavaScript；程序从入口模块的第一行开始执行。模块的加载顺序由依赖图决定，入口模块的顶层代码本身就是“main”。

```ts
// C++: int main() { ... }
// Node.js: 入口文件顶层直接执行

import { config } from './config.js';
import { startGrpcServer } from './transport/grpc.js';

async function main() {
  await startGrpcServer();
}

main().catch((err) => {
  console.error(err);
  process.exit(1);
});
```

这里有两个 C++ 开发者一开始最容易不适应的点：

- `main()` 不是语言规定的入口，Node 只是从你指定的入口文件开始加载。
- 异步函数返回 `Promise`，必须 `await` 或给 `.catch()`，否则异常不会被外层 `try/catch` 捕获。

### 1.2 C++ 与 Node.js 概念对照表

| C++ 世界 | Node.js / TypeScript 世界 | 需要特别注意 |
|---|---|---|
| CMakeLists.txt + vcpkg | package.json + package-lock.json + npm | 锁文件必须提交；`npm ci` 用于 CI |
| `.h` / `.cpp` | ESM 模块 `import` / `export` | ESM 导入本地文件通常要写 `.js` 后缀 |
| `main()` | 入口文件的顶层代码 | 入口通常写成 `main().catch(...)` |
| `std::thread` | `worker_threads` | JS 主线程尽量不要做 CPU 密集计算 |
| `boost::asio::io_context` | 事件循环 + libuv | 一个负责事件多路复用，一个负责 I/O 调度 |
| 回调 / Future / 协程 | callback / Promise / async-await | `await` 之后的代码是微任务，不是新线程 |
| RAII / 析构函数 | GC + `try/finally` + `AbortController` | 没有确定性析构，资源要显式关闭 |
| `std::optional` / `nullptr` | `undefined` / `null` | `undefined` 表示未赋值，`null` 表示显式空值 |
| `std::vector` / `std::map` | `Array` / `Map` / `Object` | `Object` 的键是字符串或 Symbol；`Map` 可以任意键 |
| `std::string` + UTF-8 | `string`（UTF-16）+ `Buffer` | 二进制数据用 `Buffer`，不要用字符串硬塞 |
| `virtual` / 抽象类 | `interface` / 结构化类型 | TypeScript 类型只在编译期存在 |
| gtest / Google Benchmark | Vitest / autocannon / k6 | 单元测试和压测工具链不同 |
| systemd / Windows 服务 | PM2 / Docker / systemd | Linux 上优先容器化或用 systemd 托管 |

### 1.3 五个必须先记住的结论

1. **JavaScript 的执行是单线程的**，但 Node.js 整个运行时不是单线程：文件、DNS、加密、压缩等部分工作交给 libuv 线程池。
2. **事件循环只负责调度**，它不会把阻塞代码变成非阻塞。一个 `while(true)` 或同步大文件读取，会让整个进程停止响应。
3. **异步不等于并发执行**。`async` 函数在 `await` 之前仍然是同步执行的，只有真正让出控制权后，其他任务才有机会运行。
4. **Promise 是“未来会有结果”的抽象**，不是新线程。要用并发，要么同时发起多个 Promise，要么用 Worker 处理 CPU 密集任务。
5. **错误必须显式处理**。Node 里未处理的 Promise rejection 可能导致进程退出，而且 `try/catch` 对没有 `await` 的异步调用无效。

---

## 2. Node.js 运行模型与并发

### 2.1 单线程指的是什么

严格说，Node.js 的“单线程”指的是 **JavaScript 代码在 V8 中只有一个执行线程**。但一个 Node 进程通常包含：

- 1 个主线程：执行 JS、运行事件循环。
- 1 个 libuv 线程池：默认 4 个线程，处理文件 I/O、`dns.lookup`、`crypto`、`zlib` 等。
- 若干底层线程：V8 的 GC、编译、平台相关的网络轮询等。

所以更准确的说法是：**JS 代码单线程，I/O 由底层异步机制和线程池支撑。**

### 2.2 事件循环的六个阶段

事件循环可以粗略理解为“一个不断轮询任务的调度器”，每一轮大致经过这些阶段：

```text
   ┌───────────────────────────┐
   │           timers          │  setTimeout / setInterval
   ├───────────────────────────┤
   │     pending callbacks     │  部分系统回调
   ├───────────────────────────┤
   │       idle, prepare       │  Node 内部使用
   ├───────────────────────────┤
   │           poll            │  I/O 事件，主要停留阶段
   ├───────────────────────────┤
   │           check           │  setImmediate
   ├───────────────────────────┤
   │      close callbacks      │  socket close 等
   └───────────────────────────┘
```

每个阶段之间，Node 会先清空微任务队列：

- `process.nextTick` 队列优先级最高。
- `Promise.then / await` 属于微任务。

一个常见的执行顺序例子：

```ts
setTimeout(() => console.log('timeout'), 0);
setImmediate(() => console.log('immediate'));
Promise.resolve().then(() => console.log('promise'));
process.nextTick(() => console.log('nextTick'));
console.log('sync');
```

在典型的顶层环境中，输出顺序通常是：

```text
sync
nextTick
promise
timeout
immediate
```

不要死记顺序，理解“同步代码先执行完，再轮询各阶段，阶段之间清微任务”就够了。

### 2.3 和 Boost.Asio 的对照

你的 C++ GateServer 里：

- `io_context` 是事件分发器，负责把完成的异步操作回调派发给线程。
- `AsioIOServicePool` 创建多个 io_context，每个跑在一个线程上，把 socket 分配给不同的 reactor。
- `work_guard` 防止 `run()` 在没任务时退出。

Node.js 里：

- 事件循环等价于“内置的、不可替换的 io_context”。
- libuv 负责底层事件多路复用和线程池。
- 你不能手动创建新的“主事件循环”，但可以用 `worker_threads` 创建新的 JS 执行线程，或用 `cluster` 创建多个进程。

一个非常重要的区别：**Asio 允许你控制事件循环和线程的绑定关系；Node.js 的主事件循环是一个全局单例，你只能通过拆分进程或 Worker 来扩展。**

### 2.4 什么代码会阻塞事件循环

以下操作在主线程执行时都会造成阻塞：

```ts
// 1. 同步文件读取
const data = fs.readFileSync('./big.log');

// 2. 同步加密
const hash = crypto.pbkdf2Sync('password', 'salt', 1_000_000, 64, 'sha512');

// 3. 大 JSON 解析
const obj = JSON.parse(hugeJsonString);

// 4. 紧密循环
while (Date.now() - start < 5000) {}

// 5. 复杂正则回溯
/^(a+)+$/.test(longString);
```

正确的写法是尽量使用异步 API：

```ts
import { readFile } from 'node:fs/promises';

const data = await readFile('./big.log', 'utf8');
```

对于确实无法拆分的 CPU 密集任务，用 Worker：

```ts
import { Worker } from 'node:worker_threads';

function runCpuTask(input: string): Promise<number> {
  return new Promise((resolve, reject) => {
    const worker = new Worker(new URL('./cpu-worker.js', import.meta.url), {
      workerData: input,
    });
    worker.once('message', resolve);
    worker.once('error', reject);
  });
}
```

结合你的项目：VerifyServer 里的主要工作是 Redis 读写、SMTP 发送、验证码生成，这些都是 I/O 或轻量计算，不需要 Worker。真正的 CPU 密集任务通常出现在图片处理、压缩、复杂加密、大规模 JSON 转换等场景。

### 2.5 并发的四种手段

Node.js 里扩大并发有四种常见手段，优先级从高到低：

1. **异步 I/O**：首选。用 Promise 化的 API 让事件循环在等待 I/O 时继续处理其他请求。
2. **连接池**：Redis、MySQL、SMTP、HTTP 客户端都通过连接池复用连接，控制并发上限。
3. **Worker Threads**：处理 CPU 密集任务，一个 Worker 一个独立 JS 线程。
4. **Cluster / 多实例**：用多个进程利用多核，适合横向扩展，但要注意端口共享和会话状态。

对 gRPC 服务要多说一句：gRPC-js 的 Server 通常建议一个进程监听一个端口；如果你要用 Cluster，最好先验证端口共享行为，或者干脆用多个容器 + 客户端负载均衡来扩展。不要想当然地套用 HTTP 服务的 cluster 方案。

### 2.6 EventEmitter 与背压

Node 的很多核心对象继承自 `EventEmitter`，例如 socket、stream、http server。它的模式类似 C++ 的观察者：

```ts
import { EventEmitter } from 'node:events';

class Mailer extends EventEmitter {
  send(to: string) {
    this.emit('sent', to);
  }
}

const mailer = new Mailer();
mailer.on('sent', (to) => console.log('sent to', to));
```

但 Node 的流还有一个重要概念：**背压**。当生产者比消费者快时，必须让生产者暂停，否则内存会不断上涨。Web 服务里对应的是“请求队列不能无限增长”。池化、限流、队列上限，本质上都是背压手段。

---

## 3. 环境搭建（Windows 开发）

### 3.1 安装 Node.js

推荐使用 Node.js LTS 版本。2026 年建议优先使用 22 LTS 或更新的 LTS；不要用奇数版本做生产环境。

下载安装包后，确认：

```powershell
node -v
npm -v
```

如果项目需要多个 Node 版本，建议安装 `nvm-windows`，然后用：

```powershell
nvm install lts
nvm use lts
```

### 3.2 npm 配置

国内网络环境下可以设置镜像，但要记住：镜像只影响下载来源，不影响依赖版本和锁文件。

```powershell
npm config set registry https://registry.npmmirror.com
npm config get registry
```

团队协作时不要随意删除 `package-lock.json`。它记录的是精确依赖树，类似 vcpkg 的版本基线，对可复现构建非常重要。

### 3.3 编辑器与调试

推荐 VS Code，并安装：

- ESLint
- Prettier
- Docker

调试 Node.js 有两种方式：

```powershell
# 启动时带调试端口
node --inspect-brk dist/index.js

# 或者在 VS Code 里直接调试 TS
```

VS Code 的 `.vscode/launch.json` 可以配置 `tsx` 直跑 TypeScript。后面调试章节会给出配置。

### 3.4 初始化工程

进入 `server/VerifyServer`，初始化 TypeScript 工程：

```powershell
cd server/VerifyServer
npm init -y
npm pkg set type=module
npm pkg set main=dist/index.js

npm install @grpc/grpc-js @grpc/proto-loader ioredis nodemailer pino zod dotenv
npm install -D typescript tsx @types/node @types/nodemailer vitest
```

注意：你当前项目的 `package.json` 里声明的是 `redis`，但 `redis.js` 里 `require("ioredis")`。这两个客户端 API 不同，必须二选一。本指南后续统一使用 `ioredis`，因为它与当前课程代码的 API 最接近。

---

## 4. TypeScript 工程基线与目录结构

### 4.1 tsconfig.json

TypeScript 的价值不是“让运行时变安全”，而是把类型错误提前到编译期。对 C++ 开发者来说，可以把它理解成“带类型擦除的静态检查层”。

```json
{
  "compilerOptions": {
    "target": "ES2022",
    "module": "NodeNext",
    "moduleResolution": "NodeNext",
    "lib": ["ES2022"],
    "outDir": "dist",
    "rootDir": "src",
    "strict": true,
    "noUncheckedIndexedAccess": true,
    "esModuleInterop": true,
    "forceConsistentCasingInFileNames": true,
    "skipLibCheck": true,
    "sourceMap": true,
    "resolveJsonModule": true,
    "types": ["node"]
  },
  "include": ["src/**/*.ts"],
  "exclude": ["node_modules", "dist"]
}
```

其中最重要的两个选项是：

- `strict: true`：开启严格模式，尤其要注意 `null` / `undefined` 检查。
- `noUncheckedIndexedAccess`：数组下标访问返回可能为 `undefined`，强迫你处理越界情况。

### 4.2 推荐的目录结构

保持“接口层、业务层、基础设施层”的分离，和你熟悉的 C++ 分层保持一致：

```text
server/VerifyServer/
├── package.json
├── tsconfig.json
├── .env.example
├── .gitignore
├── Dockerfile
├── docker-compose.yml
├── proto/
│   └── message.proto          # 与 C++ 共用的协议文件
└── src/
    ├── index.ts               # 进程入口、启动、优雅退出
    ├── config.ts              # 环境变量解析与校验
    ├── logger.ts              # 结构化日志
    ├── errors.ts              # 错误码与业务异常
    ├── infra/
    │   ├── redis.ts           # Redis 连接与关闭
    │   └── mailer.ts          # SMTP 连接池与发信
    ├── services/
    │   └── verifyCodeService.ts
    └── transport/
        └── grpc.ts            # gRPC 服务定义、绑定、关闭
```

层次关系：

```text
transport  →  services  →  infra
   协议层       业务层       基础设施层
```

规则很简单：

- `transport` 只做协议适配，不写业务规则。
- `services` 只写业务规则，不直接依赖 gRPC 对象。
- `infra` 封装外部资源，对上层暴露简单函数。

### 4.3 npm scripts

```json
{
  "scripts": {
    "dev": "tsx watch src/index.ts",
    "build": "tsc -p tsconfig.json",
    "start": "node --enable-source-maps dist/index.js",
    "typecheck": "tsc --noEmit",
    "test": "vitest run",
    "test:watch": "vitest"
  }
}
```

开发时用 `tsx watch` 热重载，生产环境用 `npm run build` 生成 `dist/` 后运行 `npm start`。不要把 `tsx` 直接用于生产环境。

### 4.4 ESM 与 CommonJS

你现有的项目用的是 CommonJS：

```js
const grpc = require('@grpc/grpc-js');
module.exports = { GetRedis };
```

新的 TypeScript 工程建议使用 ESM：

```ts
import * as grpc from '@grpc/grpc-js';
export function GetRedis() {}
```

使用 ESM 时注意：

- `package.json` 里设置 `"type": "module"`。
- 导入本地文件要写完整后缀：`import { config } from './config.js'`。
- `__dirname` 不可用，用 `fileURLToPath(import.meta.url)` 获取当前文件路径。
- 不要混用 `require` 和 `import`。

如果你只是想把现有 JS 快速跑起来，可以暂时保持 CommonJS；但新代码建议直接上 ESM。

## 5. VerifyServer 现状逐文件剖析

### 5.1 它在这个项目中的位置

当前调用链：

```text
Qt 客户端
   │ HTTP POST /get_verifycode { email }
   ▼
C++ GateServer (Boost.Beast / Asio)
   │ gRPC  GetVerifyCode
   ▼
Node.js VerifyServer
   ├── Redis：保存验证码
   └── SMTP：发送验证码邮件
```

VerifyServer 的职责边界应该非常清楚：

- 它只负责“生成验证码、写 Redis、发邮件、返回结果”。
- 它不应该参与用户注册、密码校验、好友关系等业务。
- 它不应该把验证码返回给客户端，验证码只能出现在用户邮箱里。

### 5.2 server.js：当前的入口与主要问题

当前实现大致如下：

```js
const grpc = require('@grpc/grpc-js')
const message_proto = require('./proto')
const const_module = require('./const')
const { v4: uuidv4 } = require('uuid');
const emailModule = require('./email');
// const redis_module = require('./redis')
```

第一个问题很直接：`redis_module` 被引用了，但 `require` 被注释掉了，运行时一定会抛 `ReferenceError`。同时 `package.json` 声明的是 `redis`，而 `redis.js` 里 `require("ioredis")`，依赖名称也对不上。这两处任意一处都会让服务启动或首次请求失败。

第二个问题在验证码逻辑：

```js
let query_res = await redis_module.GetRedis(const_module.code_prefix + call.request.email);
let uniqueId = query_res;
if (uniqueId.length > 4) {
    uniqueId = uniqueId.substring(0, 4);
}

if (query_res == null) {
    uniqueId = uuidv4();
    // ...
}
```

当 Redis 里没有这个邮箱的验证码时，`query_res` 是 `null`，但代码先执行 `uniqueId.length`，会直接抛 `TypeError`。这类错误会被外层 `catch` 吞掉，最终返回 `Exception`，表现出来就是“验证码发送失败”，但真实原因是空值处理顺序写反了。

第三个问题是验证码本身：用 UUID 截取前 4 位，既不是密码学安全的随机数，也有重复概率；验证码应该使用 `crypto.randomInt` 生成 6 位数字。

第四个问题是缺少限流：同一个邮箱可以在 1 秒内请求 100 次，SMTP 账号很快就会被风控或封禁。必须有“同一邮箱冷却时间”和“每日上限”。

第五个问题是配置路径：

```js
let config = JSON.parse(fs.readFileSync('config.json', 'utf8'));
```

它按“当前工作目录”读取，只有从 `server/VerifyServer` 目录启动才对。从仓库根目录执行 `node server/VerifyServer/server.js` 会直接报文件不存在。

### 5.3 redis.js：连接与重连

当前实现使用 ioredis，但有几个需要修正的点：

```js
RedisCli.on("error", function (err) {
    console.error("Redis connection error:", err);
    RedisCli.connect();
});
```

ioredis 本身就会自动重连，在 `error` 事件里手动调用 `connect()` 可能在已连接状态下重复调用，导致新的错误。更合理的做法是只记录日志，必要时在启动时做一次健康检查。

另外，`SetRedisExpire` 用了两条命令：

```js
await RedisCli.set(key, value)
await RedisCli.expire(key, exptime);
```

这不是原子操作。如果两条命令之间进程崩溃，就会残留一个没有过期时间的验证码。正确写法是一条命令：

```ts
await redis.set(key, value, 'EX', ttlSeconds);
```

如果要实现“不存在才设置”的冷却锁，用：

```ts
await redis.set(key, '1', 'EX', ttlSeconds, 'NX');
```

### 5.4 email.js：SMTP 与发件人一致性

当前代码：

```js
const transport = nodemailer.createTransport({
    host: 'smtp.qq.com',
    port: 465,
    secure: true,
    auth: {
        user: config_module.email_user,
        pass: config_module.email_pass
    }
});
```

而 `server.js` 里写的是：

```js
from: 'secondtonone1@163.com'
```

如果 SMTP 认证账号是 QQ 邮箱，发件人却写 163 邮箱，很多邮件服务会直接拒绝，或者被判定为伪造发件人。`from` 必须和 SMTP 认证账号一致，或者使用该账号被允许的别名。

此外，SMTP 也建议开启连接池：

```ts
const transporter = nodemailer.createTransport({
  pool: true,
  maxConnections: 5,
  maxMessages: 100,
  host: config.SMTP_HOST,
  port: config.SMTP_PORT,
  secure: config.SMTP_SECURE,
  auth: {
    user: config.SMTP_USER,
    pass: config.SMTP_PASS,
  },
});
```

### 5.5 config.json：密钥不能进仓库

当前 `config.json` 里保存了邮箱授权码、MySQL 密码和 Redis 密码。这个文件如果被提交，等同于把生产密钥公开。必须做三件事：

1. 立刻把 `config.json` 加入 `.gitignore`。
2. 如果这些密钥已经出现在任何聊天记录、截图或仓库历史中，立即去对应的邮箱和服务器后台重置密码。
3. 提供 `config.example.json` 作为模板，真实值通过环境变量或本地未跟踪文件注入。

推荐最终形态：

```text
server/VerifyServer/
├── .env                 # 本地真实配置，已 gitignore
├── .env.example         # 只包含键名和示例值，可提交
└── src/config.ts        # 用 zod 校验环境变量
```

### 5.6 现状问题清单

| 文件 | 问题 | 后果 | 修复方向 |
|---|---|---|---|
| server.js | `redis_module` 未 require | 运行时 ReferenceError | 恢复引入或删除 |
| package.json / redis.js | 声明 `redis`，代码用 `ioredis` | 模块找不到 | 统一为一个客户端 |
| server.js | `uniqueId.length` 在 null 检查之前 | 首次请求必崩 | 先判空再处理 |
| server.js | UUID 截取作为验证码 | 不安全、可能重复 | `crypto.randomInt` |
| server.js | 无冷却与限流 | 邮件轰炸、账号被封 | Redis `SET NX EX` |
| server.js | 无优雅退出 | SIGTERM 时丢连接 | tryShutdown + 关闭资源 |
| config.js | 相对工作目录读文件 | 启动目录一变就失败 | `path` + `__dirname`/环境变量 |
| config.json | 明文密钥且未忽略 | 密钥泄露 | `.env` + `.gitignore` |
| email.js | `from` 与认证账号不一致 | 邮件被拒 | 统一配置 |
| redis.js | set + expire 非原子 | 残留无 TTL 的键 | `SET ... EX` |
| redis.js | error 中手动 connect | 重复连接 | 交给 ioredis 自动重连 |
| proto | 响应里有 `code` 字段 | 容易误把验证码返回客户端 | 置空或删除字段 |

---

## 6. 从零重写 VerifyServer：可运行的 TypeScript 版本

这一章给出一份可以直接落地的实现。你可以把它放在 `server/VerifyServer/src` 下，逐步替换原来的 JS 文件。

### 6.1 .env.example

```dotenv
NODE_ENV=development

GRPC_HOST=0.0.0.0
GRPC_PORT=50051
PROTO_PATH=../proto/message.proto

REDIS_HOST=127.0.0.1
REDIS_PORT=6379
REDIS_PASSWORD=
REDIS_DB=0

SMTP_HOST=smtp.qq.com
SMTP_PORT=465
SMTP_SECURE=true
SMTP_USER=your_account@qq.com
SMTP_PASS=your_smtp_authorization_code
MAIL_FROM=your_account@qq.com

CODE_TTL_SECONDS=600
CODE_RESEND_INTERVAL_SECONDS=60
LOG_LEVEL=info
```

注意 `SMTP_PASS` 是邮箱的 SMTP 授权码，不是登录密码。

### 6.2 src/config.ts

```ts
import 'dotenv/config';
import { z } from 'zod';

const booleanFromString = z
  .enum(['true', 'false', '1', '0'])
  .transform((value) => value === 'true' || value === '1');

const envSchema = z.object({
  NODE_ENV: z.enum(['development', 'test', 'production']).default('development'),

  GRPC_HOST: z.string().default('0.0.0.0'),
  GRPC_PORT: z.coerce.number().int().min(1).max(65535).default(50051),
  PROTO_PATH: z.string().default('../proto/message.proto'),

  REDIS_HOST: z.string().default('127.0.0.1'),
  REDIS_PORT: z.coerce.number().int().min(1).max(65535).default(6379),
  REDIS_PASSWORD: z.string().optional(),
  REDIS_DB: z.coerce.number().int().min(0).default(0),

  SMTP_HOST: z.string().default('smtp.qq.com'),
  SMTP_PORT: z.coerce.number().int().min(1).max(65535).default(465),
  SMTP_SECURE: booleanFromString.default('true'),
  SMTP_USER: z.string().min(1),
  SMTP_PASS: z.string().min(1),
  MAIL_FROM: z.string().email(),

  CODE_TTL_SECONDS: z.coerce.number().int().positive().default(600),
  CODE_RESEND_INTERVAL_SECONDS: z.coerce.number().int().positive().default(60),
  LOG_LEVEL: z
    .enum(['fatal', 'error', 'warn', 'info', 'debug', 'trace', 'silent'])
    .default('info'),
});

const parsed = envSchema.safeParse(process.env);

if (!parsed.success) {
  console.error('环境变量校验失败:');
  console.error(parsed.error.flatten().fieldErrors);
  process.exit(1);
}

export const config = parsed.data;
export type AppConfig = typeof config;
```

这里体现了 C++ 开发者需要建立的一个习惯：**TypeScript 的类型只在编译期存在，外部输入必须做运行时校验。** 环境变量、HTTP 请求体、gRPC 请求都属于外部输入，不能只靠类型声明。

### 6.3 src/logger.ts

日志不要用 `console.log` 拼接字符串，生产环境要用结构化日志。Pino 性能好、输出 JSON，适合 Linux 日志采集。

```ts
import pino from 'pino';
import { config } from './config.js';

export const logger = pino({
  level: config.LOG_LEVEL,
  base: {
    service: 'verify-server',
    env: config.NODE_ENV,
  },
  redact: {
    paths: ['password', 'pass', 'token', '*.password', '*.pass'],
    remove: true,
  },
});
```

开发时可以用 `pino-pretty` 让日志更易读，生产环境保持 JSON。

### 6.4 src/errors.ts

错误码要和 C++ 端保持一致。当前 proto 里 `error` 是 `int32`，这不是最理想的设计，但先保持兼容。

```ts
export const ErrorCode = {
  SUCCESS: 0,
  REDIS_ERROR: 1,
  EMAIL_ERROR: 2,
  INVALID_ARGUMENT: 3,
  RATE_LIMITED: 4,
  INTERNAL_ERROR: 5,
} as const;

export type ErrorCodeValue = (typeof ErrorCode)[keyof typeof ErrorCode];

export class AppError extends Error {
  constructor(
    public readonly code: ErrorCodeValue,
    message: string,
    public readonly retryable = false,
  ) {
    super(message);
    this.name = 'AppError';
  }
}
```

### 6.5 src/infra/redis.ts

```ts
import Redis from 'ioredis';
import { config } from '../config.js';
import { logger } from '../logger.js';

export const redis = new Redis({
  host: config.REDIS_HOST,
  port: config.REDIS_PORT,
  password: config.REDIS_PASSWORD || undefined,
  db: config.REDIS_DB,
  lazyConnect: false,
  enableOfflineQueue: false,
  maxRetriesPerRequest: 2,
  retryStrategy(times) {
    return Math.min(times * 200, 2000);
  },
});

redis.on('connect', () => logger.info('redis connected'));
redis.on('error', (err) => logger.error({ err }, 'redis error'));

export async function closeRedis(): Promise<void> {
  await redis.quit().catch(() => {
    redis.disconnect();
  });
}
```

关于 `127.0.0.1`：在 Windows 上，Node 17 以后 `localhost` 可能优先解析为 IPv6 的 `::1`，而你的 Redis 或 gRPC 服务只监听 IPv4，就会出现“明明服务在跑却连不上”的问题。跨服务连接建议明确写 `127.0.0.1`。

### 6.6 src/infra/mailer.ts

```ts
import nodemailer from 'nodemailer';
import { config } from '../config.js';
import { logger } from '../logger.js';

const transporter = nodemailer.createTransport({
  pool: true,
  maxConnections: 5,
  maxMessages: 100,
  host: config.SMTP_HOST,
  port: config.SMTP_PORT,
  secure: config.SMTP_SECURE,
  auth: {
    user: config.SMTP_USER,
    pass: config.SMTP_PASS,
  },
});

export async function verifyMailer(): Promise<void> {
  await transporter.verify();
  logger.info('smtp connection verified');
}

export async function sendVerifyCodeMail(
  to: string,
  code: string,
  ttlSeconds: number,
): Promise<void> {
  const minutes = Math.max(1, Math.floor(ttlSeconds / 60));

  await transporter.sendMail({
    from: config.MAIL_FROM,
    to,
    subject: '【聊天项目】邮箱验证码',
    text: `你的验证码是 ${code}，${minutes} 分钟内有效。若非本人操作请忽略。`,
  });

  logger.info({ to }, 'verification mail sent');
}

export function closeMailer(): void {
  transporter.close();
}
```

`transporter.verify()` 会在启动时验证 SMTP 账号。如果希望服务在邮件配置错误时快速失败，就保留它；如果希望服务先启动、稍后重试，可以捕获异常并记录日志。

### 6.7 src/services/verifyCodeService.ts

这是最核心的业务文件。它负责校验邮箱、限流、生成验证码、写 Redis、发邮件，以及失败回滚。

```ts
import { randomInt } from 'node:crypto';
import { config } from '../config.js';
import { AppError, ErrorCode } from '../errors.js';
import { redis } from '../infra/redis.js';
import { sendVerifyCodeMail } from '../infra/mailer.js';
import { logger } from '../logger.js';

const CODE_PREFIX = 'verify:code:';
const COOLDOWN_PREFIX = 'verify:cooldown:';
const EMAIL_PATTERN = /^[^\s@]+@[^\s@]+\.[^\s@]+$/;

export function normalizeEmail(email: string): string {
  return email.trim().toLowerCase();
}

export function isValidEmail(email: string): boolean {
  return email.length <= 254 && EMAIL_PATTERN.test(email);
}

export function generateCode(): string {
  return randomInt(100_000, 1_000_000).toString();
}

export async function requestVerifyCode(rawEmail: string): Promise<void> {
  const email = normalizeEmail(rawEmail);

  if (!isValidEmail(email)) {
    throw new AppError(ErrorCode.INVALID_ARGUMENT, 'invalid email');
  }

  const cooldownKey = COOLDOWN_PREFIX + email;
  const cooldownSet = await redis.set(
    cooldownKey,
    '1',
    'EX',
    config.CODE_RESEND_INTERVAL_SECONDS,
    'NX',
  );

  if (cooldownSet !== 'OK') {
    throw new AppError(ErrorCode.RATE_LIMITED, 'please wait before requesting another code');
  }

  const code = generateCode();
  const codeKey = CODE_PREFIX + email;

  try {
    await redis.set(codeKey, code, 'EX', config.CODE_TTL_SECONDS);
    await sendVerifyCodeMail(email, code, config.CODE_TTL_SECONDS);
  } catch (err) {
    await redis.del(codeKey, cooldownKey).catch(() => undefined);
    logger.error({ err, email }, 'failed to send verify code');
    throw new AppError(ErrorCode.EMAIL_ERROR, 'failed to send email', true);
  }
}

export async function checkVerifyCode(rawEmail: string, inputCode: string): Promise<boolean> {
  const email = normalizeEmail(rawEmail);
  const codeKey = CODE_PREFIX + email;
  const storedCode = await redis.get(codeKey);

  if (!storedCode) {
    return false;
  }

  if (storedCode !== inputCode) {
    return false;
  }

  await redis.del(codeKey);
  return true;
}
```

这里有两个关键设计：

- 用 `SET NX EX` 做冷却锁，保证同一邮箱在冷却期内只能成功请求一次。
- 邮件发送失败时删除验证码和冷却键，允许用户重试。

`checkVerifyCode` 用 `GET` 后 `DEL`，在单进程低并发下够用；高并发下应该用 Lua 脚本做原子比较并删除，避免验证码被重复使用。

```lua
if redis.call('GET', KEYS[1]) == ARGV[1] then
  return redis.call('DEL', KEYS[1])
else
  return 0
end
```

### 6.8 src/transport/grpc.ts

```ts
import path from 'node:path';
import * as grpc from '@grpc/grpc-js';
import * as protoLoader from '@grpc/proto-loader';
import { config } from '../config.js';
import { ErrorCode, AppError } from '../errors.js';
import { logger } from '../logger.js';
import { requestVerifyCode } from '../services/verifyCodeService.js';

interface GetVerifyReq {
  email: string;
}

interface GetVerifyRsp {
  error: number;
  email: string;
  code?: string;
}

const protoPath = path.resolve(process.cwd(), config.PROTO_PATH);

const packageDefinition = protoLoader.loadSync(protoPath, {
  keepCase: true,
  longs: String,
  enums: String,
  defaults: true,
  oneofs: true,
});

const proto = grpc.loadPackageDefinition(packageDefinition) as unknown as {
  message: {
    VerifyService: {
      service: grpc.ServiceDefinition;
    };
  };
};

async function getVerifyCode(
  call: grpc.ServerUnaryCall<GetVerifyReq, GetVerifyRsp>,
  callback: grpc.sendUnaryData<GetVerifyRsp>,
): Promise<void> {
  const email = call.request.email ?? '';

  try {
    await requestVerifyCode(email);
    callback(null, {
      error: ErrorCode.SUCCESS,
      email,
      code: '',
    });
  } catch (err) {
    const errorCode = err instanceof AppError ? err.code : ErrorCode.INTERNAL_ERROR;
    logger.error({ err, email }, 'GetVerifyCode failed');
    callback(null, {
      error: errorCode,
      email,
      code: '',
    });
  }
}

export function createGrpcServer(): grpc.Server {
  const server = new grpc.Server();
  server.addService(proto.message.VerifyService.service, {
    GetVerifyCode: getVerifyCode,
  });
  return server;
}

export function startGrpcServer(server: grpc.Server): Promise<number> {
  const address = `${config.GRPC_HOST}:${config.GRPC_PORT}`;

  return new Promise((resolve, reject) => {
    server.bindAsync(address, grpc.ServerCredentials.createInsecure(), (err, port) => {
      if (err) {
        reject(err);
        return;
      }
      logger.info({ address, port }, 'verify grpc server listening');
      resolve(port);
    });
  });
}
```

注意：`code` 字段永远置空。验证码属于敏感信息，只能发到邮箱，不能通过 gRPC 返回给 GateServer。后续如果确认 C++ 端不再需要这个字段，最好从 proto 中删除。

### 6.9 src/index.ts

```ts
import { config } from './config.js';
import { logger } from './logger.js';
import { closeRedis } from './infra/redis.js';
import { closeMailer, verifyMailer } from './infra/mailer.js';
import { createGrpcServer, startGrpcServer } from './transport/grpc.js';

async function main(): Promise<void> {
  logger.info({ env: config.NODE_ENV }, 'starting verify server');

  await verifyMailer();

  const server = createGrpcServer();
  await startGrpcServer(server);

  let shuttingDown = false;

  const shutdown = async (signal: string): Promise<void> => {
    if (shuttingDown) return;
    shuttingDown = true;

    logger.info({ signal }, 'shutting down');

    const forceTimer = setTimeout(() => {
      logger.error('graceful shutdown timed out, forcing exit');
      process.exit(1);
    }, 10_000);
    forceTimer.unref();

    await new Promise<void>((resolve) => server.tryShutdown(() => resolve()));
    await closeRedis();
    closeMailer();

    clearTimeout(forceTimer);
    logger.info('shutdown complete');
    process.exit(0);
  };

  process.on('SIGINT', () => void shutdown('SIGINT'));
  process.on('SIGTERM', () => void shutdown('SIGTERM'));

  process.on('unhandledRejection', (reason) => {
    logger.error({ reason }, 'unhandled promise rejection');
  });

  process.on('uncaughtException', (err) => {
    logger.fatal({ err }, 'uncaught exception');
    process.exit(1);
  });
}

main().catch((err) => {
  logger.fatal({ err }, 'failed to start verify server');
  process.exit(1);
});
```

这段代码解决了一个常见部署问题：Linux 上 `systemd`、Docker、Kubernetes 停止服务时发送的是 `SIGTERM`，如果没有优雅退出，正在发送的邮件和 Redis 写入可能被中断。

### 6.10 运行与直接验证

开发环境：

```powershell
cd server/VerifyServer
copy .env.example .env
npm install
npm run dev
```

生产构建：

```powershell
npm run build
npm start
```

没有客户端时，可以直接用 `grpcurl` 调 gRPC：

```bash
grpcurl -plaintext \
  -proto ../proto/message.proto \
  -d '{"email":"test@example.com"}' \
  127.0.0.1:50051 \
  message.VerifyService/GetVerifyCode
```

检查 Redis 里的验证码：

```bash
redis-cli -h 127.0.0.1 -p 6379 -a your_password GET verify:code:test@example.com
```

---

## 7. 与 C++ GateServer 的 gRPC 集成

### 7.1 proto 是唯一契约

当前 [message.proto](C:/Users/Mycode/C++_Full-Stack_Chat_Project/server/proto/message.proto:6) 定义：

```proto
service VerifyService {
    rpc GetVerifyCode (GetVerifyReq) returns (GetVerifyRsp) {}
}

message GetVerifyReq {
    string email = 1;
}

message GetVerifyRsp {
    int32 error = 1;
    string email = 2;
    string code = 3;
}
```

C++ 端通过 CMake 生成 `message.pb.*` 和 `message.grpc.pb.*`；Node 端通过 `@grpc/proto-loader` 动态加载同一份 proto。这个设计很好，因为协议只有一个来源，不会出现两端各写一份结构体的情况。

但要注意两点：

- 修改 proto 后必须重新生成 C++ 代码并重新编译 GateServer。
- 删除、重命名字段属于破坏性变更，需要两端同时升级。

### 7.2 error 字段的设计问题

用 `int32 error` 表示业务错误码可以工作，但工程上不够好。推荐未来改成 proto enum：

```proto
enum VerifyErrorCode {
    VERIFY_OK = 0;
    VERIFY_REDIS_ERROR = 1;
    VERIFY_EMAIL_ERROR = 2;
    VERIFY_INVALID_ARGUMENT = 3;
    VERIFY_RATE_LIMITED = 4;
    VERIFY_INTERNAL_ERROR = 5;
}
```

这样 C++、Node、客户端都能看到同一份错误码定义，避免现在这种“客户端和服务端各维护一份枚举”的散落状态。

### 7.3 给 gRPC 调用设置 deadline

当前 C++ 端同步调用没有设置超时，一旦 Node 服务卡住，GateServer 的工作线程会一直等待。正确做法是设置 deadline：

```cpp
ClientContext context;
context.set_deadline(std::chrono::system_clock::now() + std::chrono::milliseconds(3000));

Status status = stub->GetVerifyCode(&context, request, &reply);
```

一旦超时，`status.ok()` 为 false，C++ 端可以返回 `RPC_Failed`，并记录 `status.error_message()`。

### 7.4 配置不要硬编码

GateServer 的 `config.ini` 已经有：

```ini
[VerifyServer]
Host = 127.0.0.1
Port = 50051
```

C++ 端应该通过 `ConfigMgr` 读取这两项，而不是把地址写死在代码里。Node 端同样应该从 `.env` 读取监听地址，这样本地开发和容器部署都不需要改代码。

### 7.5 超时、重试与幂等

跨服务调用有三个基本问题：

1. **超时**：必须设置 deadline，不能无限等待。
2. **重试**：只对幂等操作重试。验证码发送不是严格幂等的，重复发送会让用户收到多封邮件。
3. **幂等**：用邮箱冷却锁实现“同一邮箱短时间内只成功发送一次”，这比在调用方盲目重试更有效。

如果未来要支持重试，可以在请求里加 `request_id`，由服务端记录短时间内的重复请求并返回同一结果。

### 7.6 观测性：日志里带什么

一次请求至少要能串起来：

```text
GateServer: request_id=..., email=...
VerifyServer: request_id=..., email=..., redis_ms=..., smtp_ms=..., result=...
```

`request_id` 可以由 GateServer 生成，通过 gRPC metadata 传给 Node 端。Node 端用 `AsyncLocalStorage` 保存它，日志自动带上。这样排查问题时，不会只看到两段互不相关的日志。

## 8. Redis 在验证码场景中的正确用法

### 8.1 键设计

推荐两类键：

```text
verify:code:<email>      -> 6 位验证码，TTL 600 秒
verify:cooldown:<email>  -> "1"，TTL 60 秒
```

还可以加一个每日配额：

```text
verify:daily:<email>     -> 计数，TTL 到当天 24 点
```

键名里包含邮箱时要注意：

- 邮箱先标准化（trim + lowercase），避免大小写导致重复键。
- 邮箱长度受 254 限制，键不会无限长。
- 生产环境如果担心隐私，可以对邮箱做 HMAC 后再作为键，而不是明文邮箱。

### 8.2 原子写入与冷却锁

错误写法：

```ts
if (await redis.exists(key)) return;
await redis.set(key, code);
await redis.expire(key, ttl);
```

并发下两个请求可能同时通过 `exists` 检查，然后都写入验证码，最后用户收到两封邮件。

正确写法：

```ts
const codeKey = `verify:code:${email}`;
const cooldownKey = `verify:cooldown:${email}`;

const acquired = await redis.set(cooldownKey, '1', 'EX', 60, 'NX');
if (acquired !== 'OK') {
  throw new AppError(ErrorCode.RATE_LIMITED, 'too many requests');
}

await redis.set(codeKey, code, 'EX', 600);
```

`SET key value EX seconds NX` 在 Redis 单线程模型下是原子的：要么设置成功，要么因为键已存在而失败。

### 8.3 一次性消费与 Lua 脚本

注册时需要做“比较验证码并删除”两步操作。如果先 `GET` 再 `DEL`，两个并发请求可能都读到同一个验证码，导致验证码被重复使用。

用 Lua 脚本保证原子性：

```lua
-- KEYS[1] = verify:code:<email>
-- ARGV[1] = 用户输入的验证码
if redis.call('GET', KEYS[1]) == ARGV[1] then
  return redis.call('DEL', KEYS[1])
else
  return 0
end
```

ioredis 调用：

```ts
const script = `
if redis.call('GET', KEYS[1]) == ARGV[1] then
  return redis.call('DEL', KEYS[1])
else
  return 0
end
`;

const deleted = Number(
  await redis.eval(script, 1, `verify:code:${email}`, inputCode),
);

if (deleted !== 1) {
  throw new AppError(ErrorCode.INVALID_ARGUMENT, 'invalid or expired code');
}
```

如果你的 Redis 版本在 6.2 以上，也可以直接用 `GETDEL`，但要注意它不是“比较后再删除”，需要额外确认取出的值是否匹配。

### 8.4 和 C++ RedisMgr 的关系

你的 C++ GateServer 已经封装了 `RedisMgr`，使用 redis++ 的连接池。Node 端和 C++ 端操作的是同一套 Redis 键，因此：

- 键前缀必须统一，例如都使用 `verify:code:`，不要一边 `code_` 一边 `verify:code:`。
- 验证码的写入方最好是唯一的一方。当前设计是 Node 写、C++ 读；不要让两端都写。
- 如果以后把校验逻辑也放到 Node，建议增加 `CheckVerifyCode` gRPC 方法，让 VerifyServer 成为验证码数据的唯一拥有者。

对比两种池化：

| 维度 | redis++ 连接池（C++） | ioredis 连接（Node） |
|---|---|---|
| 资源 | TCP 连接 | TCP 连接 |
| 并发模型 | 多线程 + 连接池 | 单连接命令队列 + 可选多连接 |
| 访问方式 | 借还连接 | 命令排队 / pipeline |
| 状态 | 连接状态、超时 | 自动重连、命令队列 |
| 关键点 | 池大小与等待超时 | 不要在主线程做同步操作 |

---

## 9. 工程化落地

### 9.1 配置与密钥

原则：

- 代码里不出现真实密码、token、授权码。
- `.env` 不提交，`.env.example` 提交。
- 生产环境用容器编排的 Secret、Vault、云厂商密钥管理，而不是把 `.env` 放在镜像里。
- 配置必须在启动时校验，缺少关键项直接退出，不要带着空配置运行。

在现有项目里至少要做：

```gitignore
# server/VerifyServer/.gitignore
node_modules/
dist/
.env
config.json
*.log
```

### 9.2 结构化日志与请求 ID

日志统一用 JSON 格式输出，方便 Linux 上被 Filebeat、Fluent Bit、Loki 采集。

用 `AsyncLocalStorage` 给一次请求附带 `requestId`：

```ts
import { AsyncLocalStorage } from 'node:async_hooks';

export interface RequestContext {
  requestId: string;
  email?: string;
}

export const requestContext = new AsyncLocalStorage<RequestContext>();

export function currentContext(): RequestContext | undefined {
  return requestContext.getStore();
}
```

在 gRPC handler 里包一层：

```ts
import { randomUUID } from 'node:crypto';
import { requestContext } from '../logger.js';

async function withContext<T>(fn: () => Promise<T>): Promise<T> {
  const requestId = randomUUID();
  return requestContext.run({ requestId }, fn);
}
```

日志里输出并脱敏：

```ts
logger.info(
  {
    requestId: currentContext()?.requestId,
    email: maskEmail(email),
  },
  'verify code requested',
);
```

不要直接打印完整邮箱和验证码；邮箱可以只保留前两位和域名，验证码永远不要打日志。

### 9.3 错误处理的分层

把错误分成三类：

| 类型 | 例子 | 处理方式 |
|---|---|---|
| 用户错误 | 邮箱格式错误、验证码错误 | 返回明确业务错误码 |
| 业务限制 | 冷却中、超过每日上限 | 返回限流错误码 |
| 系统错误 | Redis 连接失败、SMTP 失败 | 记录日志、返回内部错误、触发告警 |

gRPC handler 里不要抛异常给框架，而是捕获后转换成响应：

```ts
try {
  await requestVerifyCode(email);
  callback(null, { error: ErrorCode.SUCCESS, email, code: '' });
} catch (err) {
  const code = err instanceof AppError ? err.code : ErrorCode.INTERNAL_ERROR;
  logger.error({ err, email }, 'GetVerifyCode failed');
  callback(null, { error: code, email, code: '' });
}
```

### 9.4 测试

单元测试覆盖纯逻辑，集成测试覆盖 Redis 和 SMTP。

Vitest 示例：

```ts
import { describe, expect, it } from 'vitest';
import { generateCode, isValidEmail, normalizeEmail } from './verifyCodeService.js';

describe('verifyCodeService', () => {
  it('normalizes email', () => {
    expect(normalizeEmail('  Test@Example.COM ')).toBe('test@example.com');
  });

  it('validates email', () => {
    expect(isValidEmail('test@example.com')).toBe(true);
    expect(isValidEmail('not-an-email')).toBe(false);
  });

  it('generates 6 digit code', () => {
    const code = generateCode();
    expect(code).toMatch(/^\d{6}$/);
  });
});
```

集成测试建议：

- 用 Testcontainers 或本地 Docker Compose 启动 Redis。
- SMTP 用 MailHog / Mailpit 代替真实邮箱。
- 不要在生产 SMTP 账号上跑自动化测试。

### 9.5 代码质量工具

最低限度配置：

```json
{
  "scripts": {
    "lint": "eslint .",
    "format": "prettier --write .",
    "typecheck": "tsc --noEmit",
    "test": "vitest run"
  }
}
```

CI 里按顺序执行：

```bash
npm ci
npm run typecheck
npm run lint
npm run test
npm run build
```

---

## 10. 部署：Windows 开发，Linux 上线

### 10.1 本地开发

Windows 上推荐：

1. gRPC、Redis、服务代码都可以在 Windows 本机跑。
2. Redis 建议用 Docker Desktop 启动，避免在 Windows 上直接装 Redis。
3. SMTP 使用真实邮箱授权码联调，但不要频繁触发。

```powershell
docker run --name chat-redis -p 6379:6379 -d redis:7-alpine redis-server --requirepass 123456

cd server/VerifyServer
npm run dev
```

### 10.2 Dockerfile

```dockerfile
FROM node:22-alpine AS deps
WORKDIR /app
COPY package*.json ./
RUN npm ci

FROM deps AS build
COPY tsconfig.json ./
COPY src ./src
RUN npm run build

FROM node:22-alpine AS runtime
ENV NODE_ENV=production
WORKDIR /app

COPY package*.json ./
RUN npm ci --omit=dev && npm cache clean --force

COPY --from=build /app/dist ./dist
COPY proto ./proto

USER node
EXPOSE 50051

CMD ["node", "--enable-source-maps", "dist/index.js"]
```

`.dockerignore`：

```text
node_modules
dist
.env
*.log
.git
```

容器内 `PROTO_PATH` 建议设置为 `./proto/message.proto`，因为工作目录是 `/app`。

### 10.3 docker-compose

```yaml
services:
  redis:
    image: redis:7-alpine
    command: ["redis-server", "--requirepass", "${REDIS_PASSWORD:-123456}"]
    ports:
      - "6379:6379"

  verify-server:
    build: .
    env_file:
      - .env
    environment:
      REDIS_HOST: redis
      REDIS_PORT: 6379
      PROTO_PATH: ./proto/message.proto
    depends_on:
      - redis
    ports:
      - "50051:50051"
    restart: unless-stopped
```

### 10.4 Linux 进程管理

如果不用 Docker，可以用 systemd：

```ini
[Unit]
Description=Chat Verify Server
After=network.target redis-server.service

[Service]
Type=simple
User=nodeapp
WorkingDirectory=/opt/verify-server
EnvironmentFile=/opt/verify-server/.env
ExecStart=/usr/bin/node --enable-source-maps dist/index.js
Restart=always
RestartSec=3
LimitNOFILE=65535

[Install]
WantedBy=multi-user.target
```

也可以使用 PM2：

```js
// ecosystem.config.cjs
module.exports = {
  apps: [
    {
      name: 'verify-server',
      script: 'dist/index.js',
      instances: 1,
      exec_mode: 'fork',
      max_memory_restart: '300M',
      env: {
        NODE_ENV: 'production',
      },
    },
  ],
};
```

PM2 用 cluster 模式扩展 gRPC 服务前，先确认端口共享行为；更稳妥的方案是一个容器/进程监听一个端口，通过多个副本和客户端负载均衡扩展。

### 10.5 CI 示例

```yaml
name: ci

on:
  push:
  pull_request:

jobs:
  verify-server:
    runs-on: ubuntu-latest
    defaults:
      run:
        working-directory: server/VerifyServer

    steps:
      - uses: actions/checkout@v4

      - uses: actions/setup-node@v4
        with:
          node-version: 22
          cache: npm
          cache-dependency-path: server/VerifyServer/package-lock.json

      - run: npm ci
      - run: npm run typecheck
      - run: npm run test
      - run: npm run build
```

### 10.6 上线检查清单

- [ ] `.env` 未提交，密钥已轮换。
- [ ] `NODE_ENV=production`，`LOG_LEVEL=info`。
- [ ] Redis 地址使用内网地址，生产环境启用密码或 ACL。
- [ ] SMTP 授权码使用专用账号，不和个人主邮箱共用。
- [ ] gRPC 端口只对 GateServer 所在网络开放，不对公网暴露。
- [ ] 配置了 `SIGTERM` 优雅退出。
- [ ] 配置了容器健康检查或 systemd 自动重启。
- [ ] 日志能被采集，且不包含验证码和完整邮箱。
- [ ] 已做邮件发送限流和每日上限。

---

## 11. 安全、性能与常见坑

### 11.1 邮箱验证服务的特有安全风险

邮箱验证码接口天然容易被滥用，攻击者可以：

- 用脚本批量提交不同邮箱，把你的 SMTP 账号打成垃圾邮件源。
- 对同一个邮箱疯狂请求，骚扰用户。
- 利用“邮箱是否存在”的响应差异枚举用户。

防护措施：

1. 同一邮箱冷却 60 秒。
2. 同一邮箱每日上限，例如 10 次。
3. 同一 IP 限流，例如每分钟 5 次，建议在 GateServer 或网关层做。
4. 对同一 IP 的异常请求做验证码或拒绝。
5. 无论邮箱是否存在，都返回相似的成功提示，避免枚举。
6. 验证码使用 6 位随机数，错误次数限制在 5 次以内。

### 11.2 密钥与依赖安全

```bash
npm audit
npm audit fix
```

注意：

- `package-lock.json` 提交，但不要提交 `.env`。
- 依赖升级要经过测试，不要在线上直接 `npm update`。
- 避免使用长期不维护的包；Node 生态攻击面比 C++ 依赖更动态。
- 生产镜像使用非 root 用户运行，Dockerfile 里已加 `USER node`。

### 11.3 性能与并发

VerifyServer 的瓶颈通常不在 CPU，而在：

- Redis 往返延迟。
- SMTP 发送延迟。
- 单进程事件循环能承载的并发连接数。

优化顺序：

1. 使用 `SET ... EX`、`GETDEL`、Lua 等原子命令减少往返。
2. SMTP 开启连接池，避免每次发信重新握手。
3. 控制并发发送量，避免一次性打开过多 SMTP 连接。
4. 用 `Promise.allSettled` 批量处理时设置并发上限，不要无限并发。
5. 监控事件循环延迟，超过几十毫秒就要排查阻塞代码。

估算并发可以用 Little's Law：

```text
并发量 L ≈ 到达速率 λ × 平均处理时间 W
```

例如目标 200 QPS、平均处理 30 ms，则需要约 6 个在途请求，再结合 Redis 和 SMTP 的承载能力留出余量。

### 11.4 事件循环延迟监控

```ts
import { monitorEventLoopDelay } from 'node:perf_hooks';

const h = monitorEventLoopDelay({ resolution: 20 });
h.enable();

setInterval(() => {
  const p99 = h.percentile(99) / 1e6;
  if (p99 > 50) {
    logger.warn({ p99Ms: p99 }, 'event loop lag is high');
  }
}, 10_000);
```

### 11.5 C++ 开发者最容易踩的 Node 坑

| 坑 | 说明 | 规避方式 |
|---|---|---|
| 以为 `async` 会开线程 | `async` 只是语法糖，仍运行在主线程 | 理解事件循环 |
| 忘记 `await` | Promise rejection 逃逸，异常处理失效 | 开启 ESLint 规则 |
| `forEach` 里用 `async` | 不会等待，结果不可控 | 用 `for...of` 或 `Promise.all` |
| 阻塞事件循环 | CPU 密集代码让整个服务卡住 | Worker / 拆分任务 |
| `localhost` 解析成 IPv6 | 连不上只监听 IPv4 的服务 | 明确用 `127.0.0.1` |
| 路径分隔符 | Windows 用 `\`，Linux 用 `/` | 统一 `node:path` |
| 文件名大小写 | Linux 区分大小写，Windows 不区分 | 导入路径严格匹配 |
| 环境变量未校验 | 启动后才发现缺少配置 | zod 启动校验 |
| 进程不退出 | Redis、gRPC、定时器未关闭 | 优雅退出 |
| 内存泄漏 | 全局缓存、事件监听、定时器累积 | 定期压测和堆快照 |
| 同步 API 滥用 | `readFileSync`、`pbkdf2Sync` 阻塞 | 使用异步版本 |
| `npm install` 与 `npm ci` 混用 | 依赖树漂移 | 本地 install，CI 用 ci |
| 密钥进仓库 | 授权码、密码泄露 | `.env` + secret 管理 |

## 12. 从 VerifyServer 扩展到完整聊天后端

### 12.1 服务拆分建议

当前项目已经有一个清晰的方向：

```text
Qt 客户端
   │
   ▼
GateServer（HTTP 网关 / 鉴权 / 路由）
   ├── VerifyServer（验证码 / 邮件）
   ├── StatusServer（登录状态 / 分配 ChatServer）
   └── ChatServer（WebSocket / 消息转发）
```

建议各服务的职责边界：

| 服务 | 职责 | 不建议做的事 |
|---|---|---|
| GateServer | HTTP 接入、参数校验、鉴权、路由、聚合 | 不直接发邮件，不直接处理长连接消息 |
| VerifyServer | 验证码生成、存 Redis、发邮件 | 不处理用户注册和登录 |
| StatusServer | 登录验证、分配 ChatServer、生成 token | 不保存聊天消息 |
| ChatServer | WebSocket 长连接、消息投递、在线状态 | 不直接读写用户密码 |
| 数据库层 | 用户、好友、消息持久化 | 不要用 Redis 代替所有持久化数据 |

### 12.2 通信方式怎么选

| 场景 | 推荐方式 | 原因 |
|---|---|---|
| 客户端 ↔ GateServer | HTTP/JSON | 简单、易调试、Qt 端已有 HttpMgr |
| 服务 ↔ 服务 | gRPC | 强类型、性能好、适合内网 |
| 客户端 ↔ ChatServer | WebSocket | 双向、长连接、低延迟 |
| 后台任务 | 消息队列 / 定时任务 | 解耦、可重试 |

不要为了“微服务”而微服务。当前阶段最重要的是把 VerifyServer 做扎实，再逐步引入 StatusServer 和 ChatServer。

### 12.3 数据归属

每个服务应该拥有自己的数据边界：

- 用户资料、好友关系、聊天记录：MySQL。
- 验证码、登录 token、在线状态、热点计数：Redis。
- 邮件发送记录：可以先用日志，后续再落库做审计。
- 不要把验证码写到 MySQL，也不要把用户密码放到 Redis。

跨服务共享数据时，优先通过接口而不是直接共享数据库表。共享 Redis 键是一种折中方案，必须把键前缀和格式写进文档，否则两端很容易漂移。

### 12.4 鉴权与 Token

登录成功后，StatusServer 通常返回一个 token，客户端后续请求带上它。两种常见方案：

- **JWT**：服务端无状态，适合水平扩展；缺点是吊销困难，必须处理过期、刷新和密钥轮换。
- **Redis Session**：服务端可主动失效，适合需要强制下线的场景；缺点是每个请求要查 Redis。

聊天项目里有“踢人”和“在线状态”需求，Redis Session 或 JWT + Redis 黑名单的组合更实用。

### 12.5 消息与长连接

ChatServer 用 WebSocket 维护长连接时，要处理：

- 心跳检测与超时断开。
- 断线重连与消息补偿。
- 消息去重和顺序。
- 单用户多端登录。
- 在线状态与路由表。

这些能力可以先用单机版本实现，再考虑用 Redis Pub/Sub、消息队列或一致性哈希做跨节点路由。

---

## 13. 学习路线与练习

### 第 1 周：Node.js 与 TypeScript 基础

目标：能读写异步代码，理解事件循环。

- 完成 `Promise`、`async/await`、`try/catch` 练习。
- 写一个读取文件、调用 HTTP 接口、处理并发的命令行程序。
- 把 `node.js相关补充.md` 中的例子全部手敲一遍。
- 用 `setTimeout`、`setImmediate`、`Promise` 验证执行顺序。

### 第 2 周：重写 VerifyServer

目标：用 TypeScript 完成一份可运行的验证码服务。

- 按第 6 章拆分目录。
- 接入 Redis，实现冷却锁和 TTL。
- 接入 SMTP，实现邮件发送。
- 用 `grpcurl` 直接调用 gRPC 接口。
- 修复当前 JS 版本的空指针、依赖名和配置路径问题。

### 第 3 周：工程化

目标：让服务可以被测试、被观测、被部署。

- 增加 zod 配置校验。
- 增加 Pino 结构化日志和 requestId。
- 用 Vitest 写单元测试。
- 用 Docker Compose 启动 Redis 和服务。
- 配置 GitHub Actions。

### 第 4 周：性能与扩展

目标：能解释服务的瓶颈在哪里。

- 用 autocannon 或 k6 压测 GateServer 的 `/get_verifycode`。
- 观察 Redis 往返、SMTP 延迟和事件循环延迟。
- 给 C++ gRPC 调用加 deadline。
- 设计 StatusServer 的接口和 Redis 键。

### 推荐练习清单

1. 把验证码从 4 位 UUID 改成 6 位 `crypto.randomInt`。
2. 给同一邮箱加 60 秒冷却。
3. 给同一邮箱加每日 10 次上限。
4. 用 Lua 脚本实现验证码的一次性消费。
5. 给日志加 requestId。
6. 写一个 Mock SMTP 测试，验证邮件失败时会回滚验证码。
7. 用 Docker 在 Linux 上跑通整个 VerifyServer。
8. 把 C++ 端 gRPC 调用从无超时改成 3 秒 deadline。

---

## 14. 速查表

### 14.1 常用命令

```bash
# 初始化与依赖
npm init -y
npm install <pkg>
npm install -D <pkg>
npm ci

# 开发与构建
npm run dev
npm run build
npm start
npm run typecheck
npm run test

# gRPC 调试
grpcurl -plaintext -proto ../proto/message.proto \
  -d '{"email":"test@example.com"}' \
  127.0.0.1:50051 \
  message.VerifyService/GetVerifyCode

# Redis 调试
redis-cli -h 127.0.0.1 -p 6379 -a password GET verify:code:test@example.com
redis-cli -h 127.0.0.1 -p 6379 -a password TTL verify:code:test@example.com

# Windows 查看端口
netstat -ano | findstr :50051

# Linux 查看端口
ss -lntp | grep 50051
```

### 14.2 Node 与 C++ 并发概念对照

| 概念 | C++ / Asio | Node.js |
|---|---|---|
| 事件分发 | `io_context` | 事件循环 |
| 异步回调 | completion handler | callback / Promise |
| 协程 | C++20 coroutine | `async/await` |
| 线程池 | Asio 线程池 / 自建池 | libuv 线程池 / Worker |
| 多核 | 多线程 / 多进程 | cluster / 多进程 / 容器 |
| 连接复用 | 连接池 | 连接池 / 命令队列 |
| 背压 | 信号量 / 队列上限 | stream backpressure / 限流 |
| 生命周期 | RAII | GC + 显式 close |

### 14.3 一句话回答常见面试问题

**Node.js 是单线程吗？**

> JavaScript 执行是单线程的，但运行时包含 libuv 线程池；I/O 主要由操作系统和 libuv 异步处理。

**为什么 Node.js 不适合 CPU 密集任务？**

> CPU 密集代码会占住唯一的主线程，事件循环无法调度其他请求；应使用 Worker Threads 或拆分进程。

**Promise 和线程有什么区别？**

> Promise 是异步结果的状态抽象，不是并行执行单元；它解决的是回调地狱和错误传播，不解决 CPU 并行。

**为什么 gRPC 服务要设置 deadline？**

> 防止下游卡死导致调用方线程、连接池配额被长期占用，造成级联故障。

**为什么池化能提升性能？**

> 它摊销资源创建成本、限制并发、复用连接；但池不是越大越好，过大反而会增加下游压力和排队延迟。

---

## 附录 A：当前项目可直接执行的修复清单

### A.1 先修复现有 JS 版本（最小改动）

如果你暂时不想整体迁移 TypeScript，可以先按下面顺序修复现有版本：

1. **统一 Redis 客户端**

```powershell
cd server/VerifyServer
npm uninstall redis
npm install ioredis
```

2. **恢复 `redis_module` 引用**

```js
const redis_module = require('./redis')
```

3. **修复空值判断顺序**

```js
let query_res = await redis_module.GetRedis(const_module.code_prefix + call.request.email);
let uniqueId;

if (query_res == null) {
    uniqueId = generateCode();
    const ok = await redis_module.SetRedisExpire(
        const_module.code_prefix + call.request.email,
        uniqueId,
        600,
    );
    if (!ok) {
        // 返回 Redis 错误
        return;
    }
} else {
    uniqueId = query_res;
}
```

4. **把 UUID 截取替换为安全随机数**

```js
const { randomInt } = require('node:crypto');

function generateCode() {
    return randomInt(100000, 1000000).toString();
}
```

5. **让 `SetRedisExpire` 使用原子命令**

```js
async function SetRedisExpire(key, value, exptime) {
    try {
        await RedisCli.set(key, value, 'EX', exptime);
        return true;
    } catch (error) {
        console.log('SetRedisExpire error is', error);
        return false;
    }
}
```

6. **统一发件人地址**

```js
from: config_module.email_user,
```

7. **把 `config.json` 加入 `.gitignore` 并轮换密钥**

```gitignore
server/VerifyServer/config.json
```

8. **给 Redis 冷却键加 `SET NX EX`**，限制同一邮箱的发信频率。

做完这 8 步，现有 JS 版本至少能稳定跑通。之后建议按第 6 章逐步迁移到 TypeScript。

### A.2 C++ 端配套修改

1. 给 [VerifyGrpcClient.cpp](C:/Users/Mycode/C++_Full-Stack_Chat_Project/server/GateServer/VerifyGrpcClient.cpp:10) 的 gRPC 调用加 deadline。
2. 从 `ConfigMgr` 读取 VerifyServer 的 Host 和 Port，不要硬编码。
3. 在失败分支打印 `status.error_code()` 和 `status.error_message()`。
4. `GetConnection()` 返回空指针时要提前返回错误，避免解引用。
5. 根据 `AsioIOServicePool` 的实际线程数调整 RPC 池大小。

### A.3 上线前的最后检查

```text
[ ] Node 服务能通过 grpcurl 独立调用
[ ] Redis 中能看到带 TTL 的验证码键
[ ] C++ GateServer 能收到 error=0
[ ] 客户端显示“验证码已发送”
[ ] 日志里没有验证码和完整邮箱
[ ] 配置目录不存在明文生产密钥
[ ] Linux 上 SIGTERM 能优雅退出
[ ] Docker 镜像使用非 root 用户
```

## 附录 B：推荐的延伸阅读

- [Node.js 官方文档：事件循环](https://nodejs.org/en/learn/asynchronous-work/event-loop-timers-and-nexttick)
- [Node.js 官方文档：TypeScript](https://nodejs.org/en/learn/typescript/introduction)
- [gRPC Node 官方教程](https://grpc.io/docs/languages/node/basics/)
- [ioredis 文档](https://github.com/redis/ioredis)
- [Nodemailer 文档](https://nodemailer.com/about/)
- [Pino 文档](https://getpino.io/)
- [Zod 文档](https://zod.dev/)
- [Vitest 文档](https://vitest.dev/)

---

## 结语

对 C++ 开发者来说，Node.js 最大的变化不是语法，而是运行时模型和资源管理方式：

- 用事件循环和 Promise 取代线程 + 阻塞等待。
- 用连接池和背压取代无限制地创建资源。
- 用 TypeScript + 运行时校验弥补动态类型的风险。
- 用 Docker、systemd、结构化日志和测试，把脚本级代码提升为可运维服务。

把当前 VerifyServer 按本指南重写一遍，再跑通 Redis、SMTP、gRPC 和 Linux 部署，你对 Node.js 后端的理解就会从“会写 JS”进入“能交付服务”的阶段。
