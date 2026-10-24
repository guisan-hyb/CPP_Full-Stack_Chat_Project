# Docker入门手册

按默认路线开始。下面是一个从 0 到 1 的 Docker 完整入门路径，每个阶段都包含**核心概念、最小可操作命令**和**为什么需要它**。

---

## 1. Docker 是什么：解决“在我机器上能跑”的问题

Docker 是一个开源的容器化平台。它的核心思想只有一句话：**一次构建，随处运行（Build Once, Run Anywhere）**。

传统部署的典型痛点是环境不一致：开发机上是 Python 3.11，服务器上是 3.8；本地有某个系统库，线上没有。Docker 的解法是把**应用代码 + 运行时 + 系统库 + 配置**全部打包进一个标准化的“盒子”里，这个盒子在任何安装了 Docker 的机器上都能以完全相同的方式运行。它比虚拟机轻量得多，因为容器共享宿主机内核，不需要模拟整个操作系统。

Docker 的架构可以简化为三个角色：**Docker Client**（你敲的 `docker` 命令）→ **Docker Daemon**（后台服务进程，真正干活）→ **Registry**（镜像仓库，如 Docker Hub）。

---

## 2. 镜像与容器：Docker 世界的“类”与“对象”

这是 Docker 最核心的一对概念，理解它们，后面的所有操作都会顺理成章。

**镜像**是**只读模板**，包含运行应用所需的一切：代码、运行时、库、环境变量、配置文件。可以类比为 Java 的 **Class**，或者一份**菜谱**——它本身不能“跑”，但定义了怎么“跑”。

**容器**是**镜像的运行实例**。镜像启动后就是一个容器，它可以被启动、停止、删除，拥有独立的文件系统、网络和进程空间。类比为 Java 的 **Object**，或者按菜谱做出来的**菜**。

一个镜像可以启动无数个相互隔离的容器。容器删除后，镜像仍然保留，可以再次创建新的容器。

此外还有两个配套概念：**Dockerfile** 是“构建镜像的剧本”，**仓库**是存放和分发镜像的服务（公共的如 Docker Hub，私有的如 Harbor、AWS ECR）。

---

## 3. 安装与第一个容器

### 安装

- **Windows / macOS**：下载并安装 **Docker Desktop**。Windows 必须启用 **WSL2**，Docker 的 Linux 容器才能运行。
- **Linux（Ubuntu Server 等）**：安装 **Docker CE**（社区版），通过官方仓库安装即可。

安装后验证：`docker ps` 或 `docker run hello-world`。如果提示权限不足，把当前用户加入 `docker` 组：`sudo usermod -aG docker $USER && newgrp docker`。

### 第一个容器：hello-world

```bash
docker run hello-world
```

这条命令做了三件事：从 Docker Hub **拉取** `hello-world` 镜像 → 基于该镜像**创建并启动**一个容器 → 容器执行打印任务后**退出**。你看到的那段文字，就是容器内进程的输出。

### 第二个容器：一个可访问的 Web 服务

```bash
docker run -d --name my-nginx -p 8080:80 nginx
```

打开浏览器访问 `http://localhost:8080`，你会看到 Nginx 的欢迎页。这条命令的关键参数：

| 参数         | 含义                                                     |
| ------------ | -------------------------------------------------------- |
| `-d`         | 后台运行（detach），不阻塞终端                           |
| `--name`     | 给容器起一个名字，方便后续管理                           |
| `-p 8080:80` | **端口映射**：把宿主机的 8080 端口映射到容器内的 80 端口 |

**容器是“用完即弃”的，但镜像不是。** 停止并删除容器 `docker rm -f my-nginx` 后，`nginx` 镜像仍然在本地，随时可以再启动一个新容器。

---

## 4. Dockerfile：把“怎么装环境”写成代码

手动 `docker run` 只能使用别人做好的镜像。要打包**自己的应用**，就需要编写 **Dockerfile**。

Dockerfile 是一个纯文本文件，按顺序执行指令来构建镜像。**最常用的五个指令**构成了 90% 场景的最小集合：

| 指令       | 作用                   | 关键点                                  |
| ---------- | ---------------------- | --------------------------------------- |
| **FROM**   | 指定基础镜像           | 必须放在第一行（ARG 除外）              |
| **RUN**    | 在构建时执行命令       | 安装依赖、创建目录等                    |
| **COPY**   | 把宿主机文件复制进镜像 | 比 `ADD` 更透明，推荐优先使用           |
| **CMD**    | 容器启动时的默认命令   | 一个 Dockerfile 只有最后一个 `CMD` 生效 |
| **EXPOSE** | 声明容器监听的端口     | **只做声明**，实际映射仍靠 `-p`         |

一个典型的 Node.js 应用 Dockerfile 骨架：

```dockerfile
FROM node:20-alpine
WORKDIR /app
COPY package*.json ./
RUN npm install
COPY . .
EXPOSE 3000
CMD ["node", "index.js"]
```

**两个生产级最佳实践，从第一个 Dockerfile 就应养成**：
1. **必须配 `.dockerignore`**：把 `.git`、`node_modules`、日志、本地配置排除掉，减小镜像体积，避免敏感信息泄露。
2. **合并 RUN 命令**：`RUN apt update && apt install -y xxx && rm -rf /var/lib/apt/lists/*` 写在一条 `RUN` 里，减少镜像层数，并清理缓存。

构建并运行：

```bash
docker build -t myapp:v1 .
docker run -d -p 3000:3000 --name myapp myapp:v1
```

---

## 5. 数据卷与网络：让容器“可持久”且“可对话”

容器本身是**无状态**的：删除容器，容器内写入的数据也会消失。**数据卷（Volume）** 就是解决持久化问题的机制。

### 数据卷的两种挂载方式

| 方式                       | 命令示例                 | 适用场景                                                   |
| -------------------------- | ------------------------ | ---------------------------------------------------------- |
| **命名卷（Volume）**       | `-v mydata:/app/data`    | 数据库数据、应用上传文件等生产数据，由 Docker 管理存储位置 |
| **绑定挂载（Bind Mount）** | `-v $(pwd)/src:/app/src` | 开发时把本地代码挂进容器，实现热更新                       |

常用管理命令：`docker volume ls` / `docker volume create <名>` / `docker volume inspect <名>`。

### 网络：容器之间如何互相访问

默认情况下，每个容器都在自己的网络命名空间里，**不能**通过 `localhost` 访问彼此。Docker 提供 **自定义桥接网络** 来解决这个问题：

```bash
docker network create mynet
docker run -d --name db --network mynet postgres
docker run -d --name app --network mynet myapp
```

在 `mynet` 网络中，`app` 容器可以直接用 **服务名 `db`** 作为主机名访问 PostgreSQL，Docker 内置的 DNS 会完成解析。这就是“服务发现”的最简形态。

---

## 6. Docker Compose：一条命令拉起整个应用栈

真实应用很少只有一个容器。前端、后端、数据库、缓存往往需要一起运行。如果逐个 `docker run`，你会很快被端口、网络、依赖顺序和环境变量淹没。

**Docker Compose** 用一个 `compose.yaml` 文件声明所有服务，然后一条命令完成构建、启动、组网：

```bash
docker compose up -d --build
```

一个典型的三层应用 `compose.yaml` 结构：

```yaml
services:
  frontend:
    build: ./frontend
    ports:
      - "80:80"
    depends_on:
      - backend

  backend:
    build: ./backend
    environment:
      DB_HOST: db
    depends_on:
      - db

  db:
    image: postgres:16
    volumes:
      - pgdata:/var/lib/postgresql/data
    environment:
      POSTGRES_PASSWORD: example

volumes:
  pgdata:
```

Compose 会自动：为项目创建**独立网络**，服务名即主机名；按 `depends_on` 处理启动顺序；管理命名卷的生命周期。

**注意版本**：Compose V1（命令为 `docker-compose`，带连字符）已于 2023 年停止维护，**务必使用 V2**（命令为 `docker compose`，空格分隔），它已经集成在 Docker Desktop 和现代 Docker CE 中。

---

## 7. 实战：部署一个全栈项目

以一个典型的 **React（前端）+ Flask（后端）+ PostgreSQL（数据库）** 为例，完整流程如下：

**项目结构：**
```
fullstack-app/
├── backend/
│   ├── Dockerfile
│   ├── requirements.txt
│   └── app.py
├── frontend/
│   ├── Dockerfile
│   └── ... (React 源码)
├── compose.yaml
└── .env
```

**关键步骤：**

1. **为每个服务写 Dockerfile**：前后端各自 `FROM` 合适的基础镜像，`COPY` 代码，`RUN` 安装依赖，`CMD` 启动。
2. **编写 `compose.yaml`**：定义 `frontend`、`backend`、`db` 三个服务。后端通过环境变量 `DB_HOST=db` 连接数据库；`db` 挂载命名卷持久化数据；前端端口映射到宿主机 `80`。
3. **启动**：`docker compose up -d --build`。Compose 会按顺序构建镜像、创建网络、启动数据库 → 后端 → 前端。
4. **验证**：`docker compose ps` 查看各服务状态；访问 `http://localhost` 看到前端页面，提交一条数据，确认前后端与数据库链路打通。
5. **清理**：`docker compose down`（加 `-v` 会连同数据卷一起删除，**生产环境慎用**）。

对于前后端项目，一个常见的生产级优化是在前端容器中使用 **Nginx** 托管静态文件并反向代理 API，而不是直接用 Node 进程跑前端开发服务器。

---

## 8. 常见问题与进阶方向

### 常见坑

- **容器一启动就退出**：`docker run` 运行后立即退出，通常是 `CMD` 执行的进程不是前台进程。容器需要有一个**前台进程**才能保持运行；后台进程（如 `service xxx start`）会导致容器以为任务结束而退出。用 `docker logs <容器>` 查看退出前的输出定位。
- **端口被占用**：`-p 8080:80` 中 8080 已被占用，换宿主机端口即可。
- **数据丢了**：忘记挂载数据卷，容器删除后容器内数据随之消失。数据库等有状态服务**必须**使用命名卷。
- **镜像越来越大**：`COPY . .` 把无关文件打进去，或缺少 `.dockerignore`。遵循“**先 COPY 依赖清单，再 RUN 安装，最后 COPY 代码**”的顺序，利用 Docker 层缓存，可以显著加速构建并减小体积。

### 进阶方向

| 方向                                | 一句话说明                                                   |
| ----------------------------------- | ------------------------------------------------------------ |
| **多阶段构建（Multi-stage Build）** | 构建阶段和运行阶段分离，最终镜像只保留运行时产物，体积可缩小一个数量级 |
| **Docker Compose → Kubernetes**     | Compose 适合单机和小规模部署；多节点、高可用、滚动更新等场景需要 Kubernetes |
| **镜像仓库与 CI/CD**                | 把构建好的镜像推送到私有 Harbor 或云 Registry，在 CI 流水线中自动化构建和部署 |
| **安全实践**                        | 非 root 用户运行（`USER` 指令）、扫描镜像漏洞、使用精简基础镜像（Alpine / Distroless） |

---

以上是默认路线的完整框架。如果你在任意一步（比如写 Dockerfile 构建失败、Compose 服务连不通）遇到具体问题，可以随时停下来，把报错或配置贴出来，我们针对性地排查。