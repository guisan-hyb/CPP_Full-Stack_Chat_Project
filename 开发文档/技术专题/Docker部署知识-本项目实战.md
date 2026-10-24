# Docker 部署知识：本项目实战

## 0. 文档定位

本文只整理 v1 项目迁移到 Linux Docker 时真正会用到的知识，不试图替代 Docker 官方手册。

阅读目标：

- 能理解镜像、容器、网络和卷。
- 能读懂本项目 C++ / Node.js Dockerfile。
- 能解释 Compose 中每个关键字段的作用。
- 能定位本项目常见的容器网络、配置、端口、动态库和数据持久化问题。

## 1. Docker 是什么

### 1.1 镜像与容器

| 概念 | 含义 | 本项目例子 |
| --- | --- | --- |
| 镜像 Image | 只读运行模板 | `chat-cpp-services:v1` |
| 容器 Container | 镜像启动后的进程实例 | `gate`、`chat1` |
| 卷 Volume | 独立于容器生命周期的持久数据 | `mysql_data` |
| 网络 Network | 容器间和宿主机的通信路径 | `chat_backend` |
| Registry | 镜像仓库 | 私有 Registry 或镜像服务 |

Linux 容器共享宿主机内核，不是完整的虚拟机。Docker 隔离的是进程、文件系统、网络和资源视图。

### 1.2 为什么镜像要重新构建

Docker 镜像中打包的是 Linux 二进制和依赖库。

Windows 下生成的：

```text
GateServer.exe
*.dll
MSVC 运行库
```

不能直接在 Linux 容器中运行。

本项目必须：

```text
复制源代码到 Linux Builder
  → 安装 Linux vcpkg 依赖
  → CMake 编译
  → 生成 Linux 可执行文件
  → 放进 Linux Runtime 镜像
```

## 2. 构建上下文与 .dockerignore

执行：

```bash
docker build -f Dockerfile.cpp -t chat-cpp-services:v1 .
```

末尾的 `.` 是构建上下文，不只是一个 Dockerfile 路径。Docker 会把上下文中允许的文件发送给 Builder。

本项目必须排除：

```text
build/
out/
.git/
client/
server/VerifyServer/node_modules/
server/*/config.ini
server/VerifyServer/config.json
*.log
*.exe
*.dll
```

`client/` 是 Qt 客户端，不进入服务端镜像。

`.dockerignore` 的作用：

- 减少发送到 Builder 的数据。
- 避免缓存被无关文件频繁破坏。
- 防止真实配置和凭据意外进入镜像层。
- 避免把 Windows 构建产物带入镜像。

## 3. Dockerfile 分层与缓存

### 3.1 每条指令形成一层

典型 Dockerfile：

```dockerfile
FROM ubuntu:24.04 AS builder
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake ninja-build git curl zip unzip tar
COPY vcpkg.json vcpkg-configuration.json ./
RUN /opt/vcpkg/vcpkg install --triplet x64-linux-release
COPY . .
RUN cmake --preset linux-vcpkg && \
    cmake --build --preset linux-release -j 2
```

如果把 `COPY . .` 放在安装 vcpkg 依赖之前，任何源码改动都会破坏依赖缓存。

推荐顺序：

```text
安装系统工具
  → 复制依赖清单
  → 安装依赖
  → 复制源代码
  → 编译
```

### 3.2 多阶段构建

```dockerfile
FROM ubuntu:24.04 AS builder
# 编译工具和源代码

FROM ubuntu:24.04 AS runtime
# 仅复制运行所需文件
```

Builder 阶段可以包含：

- 编译器
- CMake
- Ninja
- 头文件
- vcpkg 下载缓存
- 构建中间文件

Runtime 阶段应尽量只包含：

- 可执行文件
- 动态库
- CA 证书
- 时区数据
- 最小运行配置

### 3.3 vcpkg 动态库问题

如果在 Builder 中构建成功，但容器启动时报：

```text
error while loading shared libraries
```

说明 Runtime 阶段缺少 vcpkg 动态库。

处理思路：

1. 在 Builder 中执行 `ldd` 检查每个可执行文件。
2. Runtime 使用与 Builder 兼容的 Ubuntu 基础镜像。
3. 复制所需共享库并执行 `ldconfig`。
4. 或者明确采用静态链接，但要先确认每个依赖都支持。

不要只复制二进制文件后就假设运行环境完整。

## 4. C++ 多服务镜像设计

本项目有四个 C++ 可执行文件：

```text
GateServer
StatusServer
ChatServer
ChatServer2
```

它们依赖高度重叠，推荐共用一个镜像。

Dockerfile 不必为每个服务生成独立镜像，Compose 可以设置不同入口：

```yaml
services:
  gate:
    image: chat-cpp-services:v1
    command: ["/app/bin/GateServer"]

  status:
    image: chat-cpp-services:v1
    command: ["/app/bin/StatusServer"]
```

共享镜像的优点：

- 只构建一次 C++ 依赖链。
- 四个服务使用完全一致的运行库版本。
- 镜像构建和回滚更简单。

缺点：

- 单个镜像同时包含四个可执行文件。
- 一个服务有安全漏洞时，通常需要统一升级镜像。

对当前 v1 单机测试环境，这是合适取舍。

## 5. Node.js 镜像设计

VerifyServer 使用 Node.js，推荐：

```dockerfile
FROM node:22-bookworm-slim
WORKDIR /app
COPY server/VerifyServer/package*.json ./
RUN npm ci
COPY server/VerifyServer/ ./
CMD ["node", "server.js"]
```

关键点：

- 使用 `npm ci` 按 lockfile 安装。
- 先复制 `package.json` 和 `package-lock.json`，利用 Docker 缓存。
- 不复制宿主机的 `node_modules`。
- 不在镜像中保存真实 SMTP 授权码。

如果最终镜像没有 shell，`docker exec -it ... sh` 可能无法使用。测试阶段可以保留 shell，正式运行阶段再收紧。

## 6. Compose 的核心字段

### 6.1 services

`services` 定义需要运行哪些容器。

本项目：

```text
mysql
redis
verify
status
chat1
chat2
gate
```

### 6.2 image 与 build

```yaml
services:
  gate:
    build:
      context: .
      dockerfile: Dockerfile.cpp
    image: chat-cpp-services:v1
```

- `build` 表示从源码构建。
- `image` 表示给结果打标签。
- 如果镜像已经存在，也可以只写 `image` 从 Registry 拉取。

### 6.3 ports 与 expose

```yaml
services:
  gate:
    ports:
      - "8080:8080"

  verify:
    expose:
      - "50051"
```

区别：

| 写法 | 宿主机能否访问 | 容器网络内能否访问 |
| --- | --- | --- |
| `ports` | 能 | 能 |
| `expose` | 默认不能 | 能 |

`ports: "8080:8080"` 的格式是：

```text
宿主机端口:容器端口
```

本项目只应发布 `8080`、`8990`、`8991`。

### 6.4 depends_on 与 healthcheck

`depends_on` 只保证启动先后关系；如果 MySQL 容器已经启动但尚未可接受连接，应用仍可能失败。

因此应使用健康检查：

```yaml
depends_on:
  mysql:
    condition: service_healthy
```

健康检查检查的是“服务是否可以工作”，不是“容器进程是否存在”。

### 6.5 restart

```yaml
restart: unless-stopped
```

适用于：

- Docker 服务异常退出
- 宿主机重启
- ChatServer 意外崩溃

不要把容器启动失败和业务错误混为一谈。重启只能恢复进程，不能修复错误配置。

## 7. Docker 网络

### 7.1 Compose 服务名就是 DNS 名

在同一个 Compose 网络中：

```text
Gate 连接 mysql:33060
Gate 连接 redis:6379
Gate 连接 verify:50051
Gate 连接 status:50052
Chat1 连接 chat2:50056
```

服务名由 Docker 内部 DNS 解析，不需要在配置中写容器 IP。

容器 IP 可能变化，因此不要硬编码 `172.x.x.x`。

### 7.2 容器里的 127.0.0.1

在 `gate` 容器中：

```text
127.0.0.1 = gate 容器自己
```

它不是 MySQL，不是 Redis，也不是 Linux 宿主机。

错误配置：

```ini
[Mysql]
Host = 127.0.0.1
```

如果 MySQL 运行在另一个容器中，应改成：

```ini
Host = mysql
```

例外：同一个容器内运行的多个进程可以访问自己的回环地址。当前架构不采用这种运行方式。

### 7.3 监听地址和连接地址

`0.0.0.0` 表示监听所有网络接口，不是客户端应该连接的地址。

本项目区分：

```text
ChatServer 监听：0.0.0.0:8990
Status 返回给客户端：LINUX_HOST_IP:8990
Chat 间 gRPC：chat2:50056
```

把 `0.0.0.0` 返回给客户端通常是错误配置。

### 7.4 宿主机 IP

Windows 客户端连接的是：

```text
Linux 宿主机 IP + Compose 发布的端口
```

不是：

```text
容器 IP + 容器内部端口
```

当前 VMware NAT VM 中，宿主机侧一般使用 VM 的 NAT IP。若其他物理设备也要访问，需要检查 VM 网络模式、端口转发和宿主机防火墙。

## 8. 环境变量与配置

### 8.1 Compose 变量展开

`deploy/.env` 可以被 Compose 用于变量替换：

```yaml
environment:
  MYSQL_PASSWORD: ${MYSQL_APP_PASSWORD}
```

但 `.env` 中的变量不会自动成为所有容器的环境变量。

### 8.2 env_file

如果要把变量注入容器：

```yaml
env_file:
  - deploy/.env
```

需要区分文件挂载、Compose 替换和容器环境变量三件事。

### 8.3 本项目配置读取方式

服务端 `ConfigMgr` 从当前工作目录读取 `config.ini`：

```text
进程当前目录/config.ini
```

因此容器必须：

1. 设置正确的 `WORKDIR`。
2. 把对应配置文件放到该目录。
3. 确保不同 ChatServer 容器使用不同配置。

VerifyServer 的 `config.js` 通过 `__dirname` 定位 `config.json`，因此该文件必须位于 Node 应用目录。

### 8.4 配置模板

建议把配置分成：

```text
版本控制中的模板
  不含真实密码

服务器上的真实配置
  由环境变量生成或手工填写
```

不能把真实数据库密码、Redis 密码、SMTP 授权码提交到 Git。

## 9. 数据卷与持久化

### 9.1 容器可写层不是数据库存储

删除并重新创建 MySQL 容器时，容器可写层可能丢失。

数据库必须挂载卷：

```yaml
volumes:
  - mysql_data:/var/lib/mysql
```

`mysql_data` 的数据独立于 MySQL 容器。

### 9.2 MySQL 初始化脚本只执行一次

官方 MySQL 镜像只在数据目录为空时执行 `/docker-entrypoint-initdb.d`。

因此：

- 首次初始化会执行 SQL。
- 之后的容器重启不会再次执行。
- 修改表结构需要 migration。
- 删除数据卷会毁掉现有数据库。

### 9.3 Redis 持久化

Redis 保存 Token、验证码、节点归属和在线计数。

MySQL 是账号和好友关系的权威来源，但 Redis 仍是运行状态的重要部分。

建议挂载：

```text
redis_data → /data
```

并启用合适的数据持久化策略。

### 9.4 常用卷命令

```bash
docker volume ls
docker volume inspect <volume>
docker compose down
```

不要在日常重启中使用：

```bash
docker compose down -v
```

它会删除命名卷。

## 10. 日志与排障

### 10.1 查看状态

```bash
docker compose ps
```

重点看：

- State
- Health
- ExitCode
- Published Ports

### 10.2 查看日志

```bash
docker compose logs --tail=200 verify
docker compose logs -f --tail=200 gate
docker compose logs -f --tail=200 chat1 chat2
```

容器中的应用标准输出和标准错误通常由 Docker 收集。

### 10.3 进入容器

```bash
docker compose exec gate sh
```

进入后检查：

```bash
pwd
ls -l
getent hosts mysql redis verify status
```

如果镜像使用精简运行时，可能只有 `sh`，没有 Bash。

### 10.4 检查动态库

```bash
ldd /app/bin/GateServer
```

常见问题：

```text
not found
```

表示运行镜像缺少动态库。

### 10.5 检查端口

宿主机：

```bash
ss -lntp
```

容器内：

```bash
cat /proc/net/tcp
nc -vz 127.0.0.1 8080
```

是否安装 `nc` 取决于基础镜像。

### 10.6 检查 Compose 最终配置

```bash
docker compose config
```

该命令能显示变量展开后的配置，特别适合排查：

- 端口写错
- 卷路径错误
- 环境变量为空
- 服务名不一致

## 11. 本项目常见错误

### 11.1 MySQL 只有 3306，没有 33060

症状：

```text
C++ MySQL X DevAPI 连接失败
```

检查：

```text
MySQL 镜像版本是否为 8.x
X Plugin 是否启用
配置的 XPort 是否与实际端口一致
容器内是否监听 33060
```

### 11.2 服务间使用 127.0.0.1

症状：

```text
Gate 启动成功，但连接 MySQL/Redis/Verify/Status 失败
```

修复：

```text
容器内改用 Compose 服务名
```

### 11.3 客户端收到 chat1 或 127.0.0.1

症状：

```text
Windows 端 TCP 连接失败或连接到错误主机
```

修复：

```text
StatusServer 返回 Linux 宿主机 IP 或内网 DNS 名
```

### 11.4 容器找不到 config.ini

症状：

```text
配置为空、端口为 0、启动立即退出
```

检查：

```bash
docker compose exec gate sh
pwd
ls -l config.ini
```

配置必须位于进程当前工作目录。

### 11.5 动态库缺失

症状：

```text
error while loading shared libraries
```

修复：

```text
复制 vcpkg 共享库
或统一基础镜像版本
或检查 ldd
```

### 11.6 gRPC UNAVAILABLE

检查顺序：

1. 目标服务容器是否运行。
2. 目标服务是否监听正确端口。
3. 配置是否使用服务名。
4. 目标服务地址是否写成 `0.0.0.0`。
5. gRPC 服务名和端口是否对应。

### 11.7 MySQL Access denied

可能原因：

- 配置密码和 `.env` 不一致。
- 旧数据卷保留了旧密码。
- `chat_app` 用户未创建。
- 授权 Host 范围不正确。
- 使用了 `3306`，但代码实际连接 `33060`。

不要直接删除数据卷解决所有 Access denied。先确认原因并备份。

### 11.8 容器启动后立即退出

检查：

```bash
docker compose ps -a
docker compose logs --tail=200 <service>
docker inspect <container>
```

重点看 ExitCode 和最后一条错误。

## 12. 常用命令速查

### 12.1 构建与启动

```bash
docker compose config
docker compose build --pull
docker compose up -d
docker compose up -d mysql redis
docker compose up -d verify status chat1 chat2 gate
```

### 12.2 状态与日志

```bash
docker compose ps
docker compose logs -f --tail=200 gate
docker compose stats
```

### 12.3 重启与停止

```bash
docker compose restart gate
docker compose stop
docker compose down
```

### 12.4 清理

```bash
docker image ls
docker volume ls
docker network ls
docker system df
```

`docker system prune` 会删除未使用资源，执行前必须确认影响范围。

### 12.5 进入容器

```bash
docker compose exec gate sh
docker compose exec mysql bash
docker compose exec redis sh
```

## 13. 构建上下文和敏感文件

建议的 `.dockerignore` 基础内容：

```text
.git/
.vs/
build/
out/
client/
server/VerifyServer/node_modules/
server/*/config.ini
server/VerifyServer/config.json
deploy/.env
*.log
*.exe
*.dll
*.pdb
```

如果真实配置需要在运行时挂载，Dockerfile 只能复制模板，不能把真实密码烧进镜像层。

## 14. 安全边界

内部测试阶段的最低要求：

- 不把 MySQL 和 Redis 映射到宿主机公网接口。
- 不把真实配置提交 Git。
- 不把当前用户随意加入高权限 Docker 组。
- 不在容器内使用 root 运行不必要的服务。
- 清楚 `docker` 命令通常等价于较高宿主机权限。
- 知道当前 HTTP 和聊天 TCP 都是明文。

本次目标是 Linux 内网迁移，不代表已经满足公网安全要求。

## 15. 实施检查表

- [ ] `vcpkg.json` 固定依赖版本或 baseline。
- [ ] `.dockerignore` 排除构建产物、客户端和真实配置。
- [ ] C++ Builder 使用 Linux 编译。
- [ ] Runtime 镜像通过 `ldd` 验证动态库。
- [ ] Node.js 镜像使用 `npm ci`。
- [ ] Compose 服务名与配置一致。
- [ ] 只有 8080、8990、8991 发布到宿主机。
- [ ] MySQL 和 Redis 使用命名卷。
- [ ] MySQL 初始化脚本只在空数据目录执行。
- [ ] 所有服务支持按顺序启动。
- [ ] 每个容器有正确的 `WORKDIR` 和配置。
- [ ] 能通过 `docker compose config` 检查最终配置。
- [ ] 能通过 `docker compose logs` 定位启动错误。
- [ ] 明确 `docker compose down` 与 `down -v` 的区别。
- [ ] 迁移验收通过后再讨论公网运营。
