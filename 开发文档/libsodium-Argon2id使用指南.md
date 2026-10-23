# libsodium Argon2id 使用指南

## 1. 本文目标

本文说明如何在当前 C++ 服务端项目中使用 libsodium 的 Argon2id 能力完成密码哈希与校验。

阅读完本文应能独立完成：

1. 使用 vcpkg 安装 libsodium。
2. 使用 CMake 找到并链接 libsodium。
3. 调用 `crypto_pwhash_str` 生成密码哈希。
4. 调用 `crypto_pwhash_str_verify` 校验密码。
5. 正确保存 Argon2id 字符串。
6. 处理 Windows Debug/Release DLL。
7. 避免把密码哈希当成网络传输加密。

当前项目中的实际实现位于：

```text
server/GateServer/PasswordHasher.h
server/GateServer/PasswordHasher.cpp
```

## 2. 基础概念

### 2.1 密码哈希不是加密

密码哈希是单向的：

```text
password → Argon2id → encoded hash
```

服务端不能从 Argon2id 字符串恢复原始密码，只能验证输入密码是否与已有哈希匹配。

### 2.2 为什么使用 Argon2id

Argon2id 同时使用计算和内存资源，适合抵御针对密码哈希的批量猜解。它比 SHA-256 之类快速哈希更适合保存用户密码。

### 2.3 为什么不自己生成盐

libsodium 的 `crypto_pwhash_str` 会自动：

- 生成随机盐。
- 应用安全参数。
- 将算法、版本、参数、盐和哈希编码为一个字符串。

因此业务代码不需要单独管理盐、迭代次数和参数字段。

### 2.4 密码哈希不保护网络传输

当前桌面客户端仍然通过 HTTP 向 GateServer 发送密码。Argon2id 只解决“数据库中的密码如何保存”。

HTTPS、TCP 加密和内部 gRPC TLS 属于另外的安全边界，不能因为使用了 Argon2id 就认为网络传输已经安全。

## 3. vcpkg 安装

当前项目使用 x64-windows triplet：

```powershell
C:\vcpkg\vcpkg.exe install libsodium:x64-windows
```

检查安装结果：

```powershell
C:\vcpkg\vcpkg.exe list | Select-String libsodium
```

主要文件：

```text
C:\vcpkg\installed\x64-windows\include\sodium.h
C:\vcpkg\installed\x64-windows\include\sodium\
C:\vcpkg\installed\x64-windows\lib\libsodium.lib
C:\vcpkg\installed\x64-windows\debug\lib\libsodium.lib
C:\vcpkg\installed\x64-windows\debug\bin\libsodium.dll
C:\vcpkg\installed\x64-windows\bin\libsodium.dll
```

如果项目后续改成 vcpkg manifest 模式，应在 `vcpkg.json` 中加入：

```json
{
  "dependencies": [
    "libsodium"
  ]
}
```

当前项目仍使用经典模式，因此需要确保构建机器已经安装该端口。

## 4. CMake 接入

### 4.1 查找 libsodium

在需要密码哈希的目标中增加：

```cmake
find_package(unofficial-sodium CONFIG REQUIRED)
```

vcpkg 导出的 imported target 名称是：

```text
unofficial-sodium::sodium
```

### 4.2 链接 GateServer

当前只有 GateServer 负责注册、登录和重置密码，因此只在 GateServer 中链接：

```cmake
target_link_libraries(GateServer PRIVATE
    # 其他依赖
    unofficial-sodium::sodium
)
```

同时把实现文件加入源文件列表：

```cmake
set(GATE_SERVER_SOURCES
    # ...
    PasswordHasher.cpp
)
```

ChatServer 不读取密码哈希，因此无需链接 libsodium。

### 4.3 为什么找不到 `unofficial-sodium`

常见原因：

- vcpkg 没有安装 libsodium。
- CMake 没有使用项目配置的 vcpkg toolchain。
- 安装的 triplet 与构建目标不一致。

项目当前使用 `CMakePresets.json` 指定：

```text
C:/vcpkg/scripts/buildsystems/vcpkg.cmake
```

并构建 x64-windows 目标。

## 5. 初始化 libsodium

使用任何 libsodium API 前应调用：

```cpp
sodium_init();
```

返回值必须检查：

- `>= 0`：初始化成功。
- `-1`：初始化失败，不能继续执行密码哈希。

初始化只需要一次，当前项目使用 `std::call_once`：

```cpp
namespace {
bool initialized = false;
std::once_flag init_flag;
}

bool PasswordHasher::EnsureInitialized()
{
    std::call_once(init_flag, [] {
        initialized = sodium_init() >= 0;
    });
    return initialized;
}
```

## 6. 生成 Argon2id 哈希

### 6.1 API

```cpp
int crypto_pwhash_str(
    char out[crypto_pwhash_STRBYTES],
    const char* passwd,
    unsigned long long passwdlen,
    unsigned long long opslimit,
    size_t memlimit);
```

参数：

| 参数 | 含义 |
| --- | --- |
| `out` | 输出缓冲区，长度至少为 `crypto_pwhash_STRBYTES` |
| `passwd` | 原始密码字节 |
| `passwdlen` | 密码字节数 |
| `opslimit` | 计算成本参数 |
| `memlimit` | 内存成本参数 |

返回值：

- `0`：成功。
- `-1`：内存分配失败或参数不合理。

### 6.2 当前项目封装

```cpp
bool PasswordHasher::Hash(
    const std::string& password,
    std::string& encoded_hash)
{
    if (!EnsureInitialized() || password.empty()) {
        return false;
    }

    std::array<char, crypto_pwhash_STRBYTES> hash{};
    if (crypto_pwhash_str(
            hash.data(),
            password.data(),
            password.size(),
            crypto_pwhash_OPSLIMIT_INTERACTIVE,
            crypto_pwhash_MEMLIMIT_INTERACTIVE) != 0) {
        return false;
    }

    encoded_hash.assign(hash.data());
    sodium_memzero(hash.data(), hash.size());
    return true;
}
```

### 6.3 关于参数

这里使用 libsodium 推荐的交互式参数：

```cpp
crypto_pwhash_OPSLIMIT_INTERACTIVE
crypto_pwhash_MEMLIMIT_INTERACTIVE
```

它们适合普通交互式登录。参数越高越难暴力破解，但单个请求占用的 CPU 和内存也越多。

如果服务端同时处理大量登录请求，需要同时考虑：

- 登录限流。
- 密码哈希并发数。
- 服务器内存容量。
- 压测下的平均和尾延迟。

不要为了“更高安全”随意把参数调到服务器无法承受的程度。

### 6.4 输出格式

Argon2id 输出是自描述字符串，形如：

```text
$argon2id$v=19$m=65536,t=2,p=1$<salt>$<hash>
```

数据库应保存完整字符串，不要只保存最后的哈希部分。

当前数据库字段：

```sql
password_hash VARCHAR(255) NOT NULL
```

`crypto_pwhash_STRBYTES` 的输出上限远小于 255，因此字段长度充足。

## 7. 校验密码

### 7.1 API

```cpp
int crypto_pwhash_str_verify(
    const char* str,
    const char* passwd,
    unsigned long long passwdlen);
```

返回值：

- `0`：密码匹配。
- `-1`：密码不匹配或哈希格式无效。

### 7.2 当前项目封装

```cpp
bool PasswordHasher::Verify(
    const std::string& password,
    const std::string& encoded_hash)
{
    if (!EnsureInitialized() || password.empty() || encoded_hash.empty()) {
        return false;
    }

    return crypto_pwhash_str_verify(
               encoded_hash.c_str(),
               password.data(),
               password.size()) == 0;
}
```

不要自行拆分哈希字段，也不要用普通 `==` 对比哈希值。

## 8. 在项目中如何使用

### 8.1 注册

GateServer 收到原始密码和确认密码：

```cpp
if (pwd != confirm) {
    root["error"] = ErrorCodes::Passwd_Error;
    return;
}

std::string password_hash;
if (!PasswordHasher::Hash(pwd, password_hash)) {
    root["error"] = ErrorCodes::Passwd_Invalid;
    return;
}

MysqlMgr::GetInst()->RegUser(name, email, password_hash);
```

存储过程只接收哈希字符串：

```sql
INSERT INTO user (uid, name, email, password_hash)
VALUES (@new_id, new_name, new_email, new_password_hash);
```

### 8.2 登录

根据邮箱读取哈希：

```cpp
SELECT uid, name, email, password_hash
FROM user
WHERE email = ?;
```

校验：

```cpp
if (!PasswordHasher::Verify(input_password, password_hash)) {
    return false;
}
```

登录成功后只需把 UID、用户名和 Token 等信息传给后续服务，不需要把密码哈希继续传递。

### 8.3 重置密码

修改密码时生成新哈希：

```cpp
std::string password_hash;
if (!PasswordHasher::Hash(new_password, password_hash)) {
    return false;
}
```

更新数据库：

```sql
UPDATE user
SET password_hash = ?
WHERE name = ?;
```

不要更新为原始密码或客户端 XOR 后的字符串。

## 9. 客户端应该怎么做

客户端不应尝试自行生成 Argon2id。

错误做法：

```text
客户端哈希密码 → 把哈希当密码发给服务端
```

这会带来两个问题：

1. 服务端收到的哈希会变成可重放的认证凭据。
2. 客户端哈希本身无法替代 TLS。

正确职责：

```text
客户端：采集密码并安全传输
服务端：生成和校验 Argon2id
数据库：保存完整 Argon2id 字符串
```

当前项目已经删除 `xorString`，直接发送原始密码；在未启用 HTTPS 前，只应在本机或受控内网使用。

## 10. Windows DLL 部署

### 10.1 Debug

Debug 构建需要：

```text
C:\vcpkg\installed\x64-windows\debug\bin\libsodium.dll
```

当前 CMake/MSBuild 会把它复制到 GateServer 的 Debug 输出目录。

### 10.2 Release

Release 发布包需要：

```text
C:\vcpkg\installed\x64-windows\bin\libsodium.dll
```

如果程序启动时报找不到 DLL，先检查运行目录，而不是优先怀疑密码代码。

可以使用 Visual Studio 的 `dumpbin /dependents GateServer.exe` 检查依赖。

## 11. 常见错误

### 11.1 `Cannot open include file: sodium.h`

原因：

- libsodium 没有安装。
- CMake 没有加载 vcpkg toolchain。
- target 没有链接 `unofficial-sodium::sodium`。

### 11.2 LNK2019 `sodium_init`

原因通常是找到了头文件但没有链接库。

检查：

```cmake
target_link_libraries(GateServer PRIVATE unofficial-sodium::sodium)
```

### 11.3 程序启动提示找不到 `libsodium.dll`

把对应 Debug/Release DLL 放到可执行文件目录。

### 11.4 登录始终失败

检查顺序：

1. 注册时是否保存了 Argon2id 字符串。
2. 数据库是否仍使用旧 `pwd` 字段。
3. `reg_user` 存储过程是否仍引用旧字段。
4. 登录查询是否读取 `password_hash`。
5. 客户端是否仍然对密码执行 XOR。

### 11.5 数据库字段太短

使用 `VARCHAR(255)`。不要使用固定长度且刚好等于明文密码长度的字段。

## 12. 安全注意事项

- 不要在日志中打印密码、哈希或完整登录请求体。
- 不要向客户端返回密码哈希。
- 不要让 ChatServer 读取密码哈希。
- 不要使用 MD5、SHA-1、普通 SHA-256 保存密码。
- 不要自行生成盐或实现 Argon2。
- 不要把 Argon2id 当作 HTTPS 的替代品。
- 为登录接口增加频率限制。
- 为 Token 设置有效期。
- 密码参数升级后，可考虑在成功登录时对旧哈希执行 rehash。

当前版本使用干净数据库重建，没有实现旧哈希渐进迁移。

## 13. 测试

### 13.1 验证密码哈希

执行 P0 冒烟测试前启动：

```text
StatusServer
GateServer
ChatServer
ChatServer2
```

然后运行：

```powershell
node server\tests\p0_security_smoke.js
```

成功输出：

```text
P0 security smoke test passed
```

该脚本会真实经过 HTTP 注册、登录和 TCP 聊天链路，因此可以同时验证：

- Argon2id 哈希写入数据库。
- 登录时 Argon2id 校验成功。
- Session 身份来源。
- 好友关系校验。

### 13.2 检查数据库格式

确认字段：

```sql
SHOW COLUMNS FROM user LIKE 'password_hash';
```

确认值：

```sql
SELECT uid, name, LEFT(password_hash, 20)
FROM user;
```

输出应以 `$argon2id$` 开头。

不要把真实哈希贴到公开日志或文档中。

## 14. 与其他密码方案的关系

| 方案 | 是否适合保存密码 | 说明 |
| --- | --- | --- |
| 明文 | 否 | 数据库泄露即密码泄露 |
| XOR | 否 | 可逆、密钥弱 |
| SHA-256 | 否 | 速度太快，容易被批量猜解 |
| bcrypt | 可以 | 成熟，但需要额外库 |
| Argon2id | 推荐 | 内存困难型密码哈希 |

Argon2id 不是会话 Token 算法，也不是消息加密算法。

## 15. 本项目的完整调用链

```text
Qt 客户端输入密码
  → HTTP /user_register 或 /user_login
  → GateServer 获取原始密码
  → PasswordHasher::Hash / Verify
  → libsodium crypto_pwhash_str / crypto_pwhash_str_verify
  → MySQL password_hash
```

认证成功后：

```text
GateServer
  → StatusServer 分配 ChatServer 并签发 Token
  → 客户端连接 ChatServer
  → ChatServer 只校验 Redis Token
  → Session SetUserId
  → 后续业务使用 Session UID
```

这条边界保证密码哈希只服务于认证入口，聊天服务不接触密码材料。
