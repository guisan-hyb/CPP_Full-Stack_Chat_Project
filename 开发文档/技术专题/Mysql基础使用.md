# Mysql基础使用

## 其一：

没问题，我们分两步来解决这个问题：先帮你在 Linux 虚拟机上装好 Docker，再教你怎么在 Windows 终端里玩转本地的 MySQL。

---

### 第一部分：在 Linux 虚拟机上安装 Docker

假设你的虚拟机是 Ubuntu 系统（这是最常用的），官方推荐使用 apt 仓库来安装。请按顺序在虚拟机终端执行以下命令：

**1. 更新包管理器并安装基础依赖**
```bash
sudo apt update
sudo apt install ca-certificates curl gnupg lsb-release -y
```

**2. 添加 Docker 官方的 GPG 密钥和仓库**
```bash
# 添加密钥
sudo mkdir -p /etc/apt/keyrings
curl -fsSL https://download.docker.com/linux/ubuntu/gpg | sudo gpg --dearmor -o /etc/apt/keyrings/docker.gpg

# 添加仓库
echo "deb [arch=$(dpkg --print-architecture) signed-by=/etc/apt/keyrings/docker.gpg] https://download.docker.com/linux/ubuntu $(lsb_release -cs) stable" | sudo tee /etc/apt/sources.list.d/docker.list > /dev/null
```
*(注：如果你的虚拟机网络访问官方源较慢，可以考虑使用国内镜像源，但一般情况下上述官方源在虚拟机里都能正常下载)*

**3. 安装 Docker 引擎**
```bash
sudo apt update
sudo apt install docker-ce docker-ce-cli containerd.io docker-compose-plugin -y
```

**4. 验证安装并配置权限（非常重要）**
安装完成后，输入以下命令查看版本：
```bash
docker -v
```
默认情况下，每次运行 docker 命令都需要加 `sudo`，非常麻烦。把当前用户加入 docker 组即可免 sudo：
```bash
sudo usermod -aG docker $USER
```
**⚠️ 注意：执行完上面这条命令后，必须注销并重新登录虚拟机（或者直接重启虚拟机），权限才会生效。**

重新登录后，运行自带的测试镜像：
```bash
docker run hello-world
```
如果终端打印出 "Hello from Docker!"，说明你的 Docker 环境彻底装好了！接下来你就可以回到上一条回答的“第一步”，用那行 `docker run` 命令去部署 MySQL 了。



---

### 补充：使用国内镜像源

这**不是**因为你没有创建镜像，而是因为**网络连接被拒绝**。

报错信息 `dial tcp...:443: connect: connection refused` 意味着你的 Linux 虚拟机无法连接到 Docker 的官方镜像仓库。这在国内网络环境下非常常见，Docker Hub 的官方服务器在国外，经常会连接超时或被拒绝。

### 解决方法：配置 Docker 国内镜像加速器

你需要告诉 Docker，以后不要去官方地址拉镜像了，去国内的镜像站拉。按以下步骤操作：

**1. 修改 Docker 配置文件**
在虚拟机终端执行以下命令，创建或修改 `/etc/docker/daemon.json` 文件：
```bash
sudo mkdir -p /etc/docker
sudo tee /etc/docker/daemon.json <<-'EOF'
{
  "registry-mirrors": [
    "https://docker.m.daocloud.io",
    "https://dockerproxy.com",
    "https://docker.mirrors.ustc.edu.cn",
    "https://hub-mirror.c.163.com"
  ]
}
EOF
```

**2. 重新加载配置并重启 Docker 服务**
```bash
sudo systemctl daemon-reload
sudo systemctl restart docker
```

**3. 再次尝试拉取测试镜像**
注意：官方测试镜像的名字是 `hello-world`（中间是短横线，不是下划线）。
```bash
docker run hello-world
```

如果终端打印出如下一段英文：
> Hello from Docker!
> This message shows that your installation appears to be working correctly.

说明你的 Docker 环境已经彻底打通，网络问题解决了！接下来你就可以用 `docker run` 顺畅地去部署 MySQL 和 Redis 了。



---

### 第二部分：在 Windows 终端使用本地 MySQL

你之前在 Windows 上安装了 MySQL，它通常自带了一个命令行工具。要在 Windows 终端（PowerShell 或 CMD）里使用它，核心是**把 MySQL 的 bin 目录加到系统环境变量里**。

#### 1. 找到你 Windows 上 MySQL 的安装路径
通常默认安装路径类似：
`C:\Program Files\MySQL\MySQL Server 8.0\bin`
你需要进入这个目录确认一下，里面是否有 `mysql.exe` 这个文件。

#### 2. 配置环境变量（一劳永逸）
1. 按 `Win + S` 搜索 **“环境变量”**，点击 **“编辑系统环境变量”**。
2. 在弹出的窗口点击右下角的 **“环境变量”**。
3. 在下方 **“系统变量”** 列表中，找到 `Path`，双击它。
4. 点击 **“新建”**，把你刚才找到的 bin 目录路径粘贴进去（例如 `C:\Program Files\MySQL\MySQL Server 8.0\bin`）。
5. 一路点击“确定”保存。

#### 3. 在终端里连接 MySQL
1. **以管理员身份运行 PowerShell**（或者普通的 CMD）。
2. **启动 MySQL 服务**（如果它没有开机自启）：
   ```powershell
   net start mysql80
   ```
   *(注：`mysql80` 是默认的服务名，如果你的版本不同，可能是 `mysql` 等)*
3. **登录 MySQL**：
   ```powershell
   mysql -u root -p
   ```
4. 终端会提示你输入密码。输入你当初安装时设置的 root 密码（输入时屏幕不会显示字符，盲打即可，回车确认）。
5. 看到提示符变成 `mysql>`，说明你成功进入了 Windows 本地的 MySQL！

#### 4. 在终端里执行基础 SQL 测试
```sql
-- 查看当前有哪些数据库
SHOW DATABASES;

-- 创建一个测试数据库
CREATE DATABASE test_db;

-- 切换到这个数据库
USE test_db;

-- 退出
exit;
```

### 总结
*   **Linux 虚拟机**：装好 Docker 后，用 `docker run` 一键部署生产级 MySQL，供你的 C++ 项目远程连接。
*   **Windows 本地**：配置好环境变量后，直接在 PowerShell 里 `mysql -u root -p` 登录，适合临时写写 SQL、做做语法练习，或者用 Navicat 连接 `127.0.0.1` 看本地的数据。



## 其二：

这是一个非常完美的开发环境规划！**“Windows 上用 Navicat 作为图形化客户端，Linux 虚拟机上用 Docker 运行 MySQL 服务端”**，这完全是现代后端开发的标准姿势。

这样做的好处是：你的数据库环境干净隔离，随时可以销毁重建，而你的 Windows 上依然保留着友好的可视化界面来建表、看数据。

下面是保姆级的实操步骤，带你一步步打通这个环境：

---

### 第一步：在 Linux 虚拟机上用 Docker 部署 MySQL

登录你的 Linux 虚拟机终端，执行以下一条 Docker 命令，直接拉取并启动 MySQL 8.0：

```bash
docker run -d \
  --name chat-mysql \
  -p 3306:3306 \
  -e MYSQL_ROOT_PASSWORD=123456 \
  -v /mydata/mysql/data:/var/lib/mysql \
  mysql:8.0 \
  --character-set-server=utf8mb4 \
  --collation-server=utf8mb4_unicode_ci
```

**参数解释（很重要，知其然知其所以然）：**
* `-d`：后台运行。
* `--name chat-mysql`：给容器起个名字叫 `chat-mysql`。
* `-p 3306:3306`：端口映射。把虚拟机的 3306 端口映射到容器内的 3306 端口（这样你的 Windows 才能连进来）。
* `-e MYSQL_ROOT_PASSWORD=123456`：设置 root 账号的密码为 `123456`（开发环境随便设，别太复杂）。
* `-v /mydata/mysql/data:/var/lib/mysql`：**数据卷挂载（核心！）**。把容器内的数据目录映射到虚拟机的 `/mydata/mysql/data`。这样即使容器被删了，你的数据库文件还在，不会丢数据。
* `--character-set-server=utf8mb4...`：强制数据库使用 `utf8mb4` 编码。**做全栈聊天项目必须用这个**，因为它支持存储 Emoji 表情符号，普通的 utf8 存表情会报错。

执行完后，输入 `docker ps`，如果看到 `chat-mysql` 状态是 `Up`，说明数据库已经跑起来了。

---

### 第二步：配置 MySQL 允许远程连接

Docker 启动的 MySQL 默认是允许 root 远程连接的，但为了保险起见，我们进容器里确认一下，并开放远程权限。

1. 进入 MySQL 容器内部：
   ```bash
   docker exec -it chat-mysql mysql -uroot -p123456
   ```
2. 在 MySQL 命令行里执行授权语句（注意每句话结尾要有分号 `;`）：
   ```sql
   -- 创建一个允许从任意IP(%)登录的root用户
   CREATE USER 'root'@'%' IDENTIFIED WITH mysql_native_password BY '123456';
   
   -- 赋予全部权限
   GRANT ALL PRIVILEGES ON *.* TO 'root'@'%';
   
   -- 刷新权限
   FLUSH PRIVILEGES;
   
   -- 退出
   exit;
   ```

3. 如果你的虚拟机开启了防火墙，记得放行 3306 端口：
   ```bash
   sudo ufw allow 3306
   ```

---

### 第三步：用 Windows 的 Navicat 连接虚拟机里的 MySQL

现在回到你的 Windows 宿主机。

1. 获取你 Linux 虚拟机的 IP 地址（比如 `192.168.64.128`）。
2. 打开 Navicat，点击左上角 **“连接” -> “MySQL”**。
3. 按以下信息填写：
   * **连接名**：随便起，比如 `Linux-Chat-MySQL`
   * **主机**：填你虚拟机的 IP（如 `192.168.64.128`）
   * **端口**：`3306`
   * **用户名**：`root`
   * **密码**：`123456`
4. 点击左下角 **“测试连接”**。如果弹出“连接成功”，说明大功告成！

以后你就可以在 Navicat 里双击这个连接，直接在里面新建数据库（比如叫 `chat_db`），建表（比如 `users` 表），敲 SQL 语句了。

---

### 第四步：C++ 项目引入 MySQL 库 (vcpkg + CMake)

你的 C++ GateServer 要连 MySQL，需要引入 C++ 的 MySQL 驱动库。由于你用的是 vcpkg，这一步极其简单。

**1. 用 vcpkg 安装 `libmysql`**
打开终端，进入 vcpkg 目录：
```bash
.\vcpkg install libmysql:x64-windows
```
*(注：恋恋风辰的教程可能用的是 `mysql-connector-cpp` 或其他的库，如果你跟着视频敲，视频用什么你就装什么。但 `libmysql` 是最基础最常用的 C 语言接口库，大部分 C++ 封装都基于它)*

**2. 修改 `CMakeLists.txt`**
在你的 `server/CMakeLists.txt` 中加入寻找 MySQL 的代码：
```cmake
# 寻找 MySQL
find_package(unofficial-libmysql CONFIG REQUIRED)
```
然后在 `server/GateServer/CMakeLists.txt` 中链接它：
```cmake
target_link_libraries(GateServer PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    Boost::boost
    redis++::redis_plus_plus
    chat_proto
    unofficial::libmysql::libmysql   # 链接 MySQL
)
```

**3. C++ 代码测试连接**
```cpp
#include <mysql.h>
#include <iostream>

int main() {
    MYSQL* conn = mysql_init(nullptr);
    
    // 连接数据库 (IP, 用户名, 密码, 数据库名, 端口)
    conn = mysql_real_connect(conn, "192.168.64.128", "root", "123456", "chat_db", 3306, nullptr, 0);
    
    if (conn) {
        std::cout << "MySQL 连接成功！" << std::endl;
        
        // 执行一条查询测试
        mysql_query(conn, "SELECT * FROM users;");
        MYSQL_RES* res = mysql_store_result(conn);
        // ... 处理结果
        
        mysql_close(conn);
    } else {
        std::cerr << "MySQL 连接失败: " << mysql_error(conn) << std::endl;
    }
    return 0;
}
```

### 总结避坑指南
1. **Windows 本地 MySQL 的干扰**：你之前在 Windows 上装了 MySQL。如果你的 Windows MySQL 服务也是开机自启的，它占用了 Windows 的 3306 端口。这不影响你用 Navicat 连虚拟机，但建议把 Windows 本地的 MySQL 服务设为“手动启动”，以免以后造成端口或配置冲突。
2. **编码问题**：一定要保证你的数据库是 `utf8mb4`，并且你的 C++ 代码连接后也要执行 `mysql_query(conn, "set names utf8mb4");`，否则存中文到数据库里会变成一堆问号 `???`。



---

---

### 其三：Docker MySQL 连接排错复盘手册

太棒了！恭喜你成功打通了 Windows 宿主机到 Linux 虚拟机 Docker 容器的数据库连接。这是 Docker 部署中最容易卡壳的一步，你跨过去了。

为了方便你日后复习查阅，我将本次“Navicat 连接 Docker MySQL 报 1045 错误”的排查过程提炼成了一份**排错复盘手册**：

---

### 📌 Docker MySQL 连接排错复盘手册

#### 1. 核心结论：1045 错误代表什么？
* **报错信息**：`1045 - Access denied for user 'root'@'...' (using password: YES)`
* **真实含义**：网络是完全通的！Navicat 成功跨越了 Windows -> 虚拟机 -> Docker 容器，连到了 MySQL。**只是 MySQL 认为你的密码不对，或者权限不对，拒绝了你的登录。**

#### 2. 罪魁祸首：Docker 数据卷持久化陷阱
* **现象**：明明在 `docker run` 时通过 `-e MYSQL_ROOT_PASSWORD=123456` 设置了新密码，但就是登不进去。
* **原因**：你使用了 `-v /mydata/mysql/data:/var/lib/mysql` 挂载数据卷。当第一次启动容器时，MySQL 把数据写进了这个目录。当你后来修改密码重新 `docker run` 时，**MySQL 发现挂载目录里已经有旧的用户权限数据了，就会忽略你新传入的环境变量密码**。
* **教训**：对于数据库这类有状态服务，挂载的数据卷是“老大”，环境变量只在第一次初始化时生效。

#### 3. 标准解决流程（重置大法）
当确认密码输没错，但还是报 1045 时，执行以下“清空重置”三连操作：
```bash
# 1. 停止并删除出问题的容器 (图纸还在，只是拆了房子)
docker stop chat-mysql
docker rm chat-mysql

# 2. 彻底删除挂载出来的旧数据文件 (清空旧档案)
sudo rm -rf /mydata/mysql/data

# 3. 重新执行 docker run 命令 (全新初始化)
docker run -d \
  --name chat-mysql \
  -p 3306:3306 \
  -e MYSQL_ROOT_PASSWORD=123456 \
  -v /mydata/mysql/data:/var/lib/mysql \
  mysql:8.0 \
  --character-set-server=utf8mb4 \
  --collation-server=utf8mb4_unicode_ci
```
*注：等待 10 秒让 MySQL 完成初始化，再用 Navicat 测试即可成功。*

#### 4. 备用方案：MySQL 8.0 认证插件不兼容
如果重置数据后依然报 1045，可能是 MySQL 8.0 默认的 `caching_sha2_password` 加密方式老版本 Navicat 不认。
**解决办法**：进容器改回老插件。
```bash
# 进入容器内部
docker exec -it chat-mysql mysql -uroot -p123456

# 执行 SQL 修改加密方式
ALTER USER 'root'@'%' IDENTIFIED WITH mysql_native_password BY '123456';
FLUSH PRIVILEGES;
exit;
```

---

### 💡 架构连通性验证总结

通过这次排错，你实际上验证了你的**跨端网络架构**是完全合格的：

1. **虚拟机网络配置合格**：`ip addr` 查到的 `192.168.182.129` 地址有效，且虚拟机防火墙放行了 3306 端口（`ufw allow 3306`）。
2. **Docker 端口映射合格**：`-p 3306:3306` 成功将宿主机的端口流量转发给了容器。
3. **Windows 客户端配置合格**：Navicat 填写的 IP、端口、账号密码格式正确。

以后如果遇到类似问题（比如 Redis 连不上、gRPC 连不上），都可以套用这个思路：**先看网络通不通（ping / telnet），再看端口映射对不对（docker ps），最后看应用层认证对不对（密码/权限）**。





## 其三：改用mysql-connector-cpp

好的！换成 `mysql-connector-cpp` 是一个非常明智的选择，它能让你用上面向对象和智能指针的方式来操作数据库，代码会优雅很多。

由于你使用了 vcpkg，卸载和安装都极其简单。以下是完整的切换流程：

### 第一步：卸载旧的 `libmysql`

打开你的 PowerShell，进入 vcpkg 目录，执行卸载命令：
```bash
.\vcpkg remove libmysql:x64-windows
```
*(vcpkg 会自动把它从你的系统中移除，并清理相关的底层依赖)*

### 第二步：安装 `mysql-connector-cpp`

继续在 vcpkg 目录下执行安装命令：
```bash
.\vcpkg install mysql-connector-cpp:x64-windows
```
*(注：这个库比较大，可能需要编译十几分钟，请耐心等待)*

### 第三步：修改 CMakeLists.txt

安装完成后，我们需要把 CMake 里的寻找和链接目标改掉。

**1. 修改 `server/CMakeLists.txt`**
把之前寻找 `libmysql` 的那行删掉，换成：
```cmake
# 移除旧的 find_package(unofficial-libmysql CONFIG REQUIRED)
find_package(mysql-connector-cpp CONFIG REQUIRED)
```

**2. 修改 `server/GateServer/CMakeLists.txt`**
把链接目标改掉：
```cmake
target_link_libraries(GateServer PRIVATE
    gRPC::grpc++
    protobuf::libprotobuf
    Boost::boost
    redis++::redis_plus_plus
    chat_proto
    # 移除旧的 unofficial::libmysql::mysqlclient
    mysql-connector-cpp::mysql-connector-cpp   # 替换为新目标
)
```
*(⚠️ 注意：vcpkg 有时对包名的命名很随意，如果在 CMake 里还是报找不到目标，请看一下安装完成后 vcpkg 在终端输出的 `find_package()` 和 `target_link_libraries` 提示，以那个提示为准)*

### 第四步：极其重要的一步 —— 清理缓存！

因为你在 CMake 里大改了库的依赖，**必须清理缓存**，否则旧的 `libmysql` 残留配置会导致编译报错。
1. 关闭编辑器。
2. 删除项目根目录下的 `build` 文件夹。
3. 删除 `.vs` 文件夹（如果有的话）。
4. 重新打开项目，让 CMake 重新配置。

---

### 第五步：现代 C++ 代码测试体验

一切配置就绪后，你可以用下面这段现代 C++ 风格的代码来测试连接。你会发现，相比之前满屏幕的 `MYSQL*` 指针，这简直像是在写 Java 或 C# 一样清爽：

```cpp
#include <jdbc/mysql_driver.h>
#include <jdbc/mysql_connection.h>
#include <jdbc/cppconn/statement.h>
#include <jdbc/cppconn/resultset.h>
#include <jdbc/cppconn/exception.h>
#include <memory>
#include <iostream>

void testMySQL() {
    try {
        // 1. 获取驱动实例 (不需要手动 delete)
        sql::mysql::MySQL_Driver* driver = sql::mysql::get_mysql_driver_instance();
        
        // 2. 建立连接 (使用智能指针自动管理内存，用完自动析构)
        // 格式: tcp://IP地址:端口
        std::unique_ptr<sql::Connection> conn(
            driver->connect("tcp://192.168.182.129:3306", "root", "123456")
        );
        
        // 3. 选择要使用的数据库 (你在 Navicat 里建好的那个库)
        conn->setSchema("chat_db");
        
        // 4. 创建 Statement 对象
        std::unique_ptr<sql::Statement> stmt(conn->createStatement());
        
        // 5. 执行查询
        std::unique_ptr<sql::ResultSet> res(
            stmt->executeQuery("SELECT * FROM users LIMIT 10;")
        );
        
        // 6. 遍历结果集
        while (res->next()) {
            // 通过列名获取数据，比原来的按下标取安全多了！
            std::cout << "ID: " << res->getInt("id") 
                      << ", Email: " << res->getString("email") 
                      << std::endl;
        }
        
        std::cout << "MySQL Connector C++ 测试成功！" << std::endl;
        
    } catch (const sql::SQLException& e) {
        // 现代库自带异常捕获，不用担心程序崩溃
        std::cerr << "MySQL Error: " << e.what() << std::endl;
        std::cerr << "Error Code: " << e.getErrorCode() << std::endl;
    }
}
```

### 核心优势总结：
1. **智能指针管理 (`std::unique_ptr`)**：连接、语句、结果集用完自动释放，**永远不会内存泄漏**。
2. **异常机制 (`try-catch`)**：网络断了、密码错了、SQL写错了，都会抛出 `sql::SQLException`，你可以统一捕获处理，不会像 C API 那样直接段错误。
3. **按列名取值 (`res->getString("email")`)**：不用再记第几列是什么字段，代码可读性拉满。

赶紧把这段代码塞到你的 `GateServer.cpp` 里跑一下，如果成功打印出数据，说明你的现代 C++ 数据库层已经完美搭建完成了！



## 注意：*(⚠️ 注意：vcpkg 有时对包名的命名很随意，如果在 CMake 里还是报找不到目标，请看一下安装完成后 vcpkg 在终端输出的 `find_package()` 和 `target_link_libraries` 提示，以那个提示为准)*

例如，我的输出如下：
```bash
 # this is heuristically generated, and may not be correct
  find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)
  target_link_libraries(main PRIVATE unofficial::mysql-connector-cpp::connector)# 写到cmake的时候把 main PRIVATE 去掉，否则会报错 -> " LNK1104: 无法打开文件 "main.lib" "
```

