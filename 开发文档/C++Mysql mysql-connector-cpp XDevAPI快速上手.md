## C++/Mysql: `mysql-connector-cpp` XDevAPI快速上手

下面这份讲解我按你机器上**实际安装的 9.7.0 版本**核对过 API（比如 `Value::get<T>()` 存在、`Row` 只能按下标取、`sess.sql()` 用 `?` 而链式 CRUD 用 `:名字`），你可以直接照着写。

## 一、先纠正一个认知：不是"新旧两套库"，是同一份 Connector 的两条通路

MySQL Connector/C++ 8.0/9.x 内部同时实现了两套 API，它们**走的是两条服务端协议**：

| 对比项     | JDBC 风格（`sql::`）                            | X DevAPI（`mysqlx::`）                                  |
| ---------- | ----------------------------------------------- | ------------------------------------------------------- |
| 服务端协议 | 经典 MySQL 协议                                 | X Protocol                                              |
| 端口       | 3306                                            | 33060                                                   |
| 服务端要求 | 无                                              | X Plugin（MySQL 5.7.12+ / 8.0 自带）                    |
| 头文件     | `jdbc/mysql_driver.h` 等                        | `mysqlx/xdevapi.h` 一个头就够                           |
| 入口对象   | `sql::mysql::get_mysql_driver_instance()`       | `mysqlx::Session` / `mysqlx::Client`                    |
| 连接池     | 要自己写                                        | `mysqlx::Client` 内置                                   |
| 执行 SQL   | `Statement` / `PreparedStatement`               | `session.sql()` + `bind()`                              |
| 表操作     | 只能写 SQL                                      | SQL **或** 链式 CRUD（`table.select()...`）             |
| 结果集     | `ResultSet`，可以 `getString("email")` 按列名取 | `RowResult`，**只能按下标** `row[0].get<std::string>()` |
| 异常类型   | `sql::SQLException`                             | `mysqlx::Error`                                         |
| 事务       | `setAutoCommit(false)` / `commit` / `rollback`  | `startTransaction()` / `commit` / `rollback`            |
| 额外能力   | 无                                              | 文档存储（`Collection`）、连接池、链式 CRUD             |

同一个查询，两种写法对比：

```
// JDBC 风格
std::unique_ptr<sql::PreparedStatement> ps(
    conn->prepareStatement("SELECT id, email FROM users WHERE email = ?"));
ps->setString(1, email);
std::unique_ptr<sql::ResultSet> rs(ps->executeQuery());
if (rs->next()) {
    int id = rs->getInt("id");
    std::string mail = rs->getString("email");
}

// X DevAPI
auto sess = MysqlMgr::GetInst()->GetSession();
mysqlx::Row row = sess.sql("SELECT id, email FROM users WHERE email = ?")
                       .bind(email)
                       .execute()
                       .fetchOne();
if (row) {
    int id = row[0].get<int>();              // 注意：只能下标
    std::string mail = row[1].get<std::string>();
}
```

结论：X DevAPI 在"连接管理、事务、表操作"上更省事，但在"结果集按列名取值"这一点上**不如 JDBC 顺手**，这是你写 DAO 时要专门弥补的地方（后面给了方案）。

## 二、X DevAPI 的对象树（先记住这张图，一切就顺了）

```
Client（连接池，线程安全，整个程序只建 1 个，放单例里）
 │  getSession()  ← 借一条连接出来
 └─ Session（一条连接 + 事务边界；不可跨线程；move-only）
      ├─ getDefaultSchema() / getSchema("guisan") → Schema（就是"数据库"）
      │     ├─ getTable("users") → Table
      │     │     ├─ insert("email","passwd").values(...)    .execute()
      │     │     ├─ select("id","email").where("...")        .execute() → RowResult
      │     │     ├─ update().set("passwd", x).where("...")   .execute()
      │     │     └─ remove().where("...")                    .execute()
      │     └─ getCollection("jsondoc") → Collection（文档存储，可选，先不用）
      ├─ sql("SELECT ...").bind(...).execute() → SqlResult（结果集也是它）
      └─ startTransaction() / commit() / rollback()
```

三条句柄语义规则，能帮你避开一半的坑：

1. `Session` 是 **move-only**（像 `unique_ptr`），只能 `auto sess = ...` 或 `std::move`，不能拷贝；复制 `Session` 编译不过。
2. `Schema` / `Table` / `Row` / `Value` / 结果集 都是**轻量句柄**（内部共享实现），可以正常赋值、返回、放进容器。
3. 这些对象**都不是**手动 new/delete 的，离开作用域自动释放、自动归还连接。

## 三、最小可运行 demo（先跑通再谈封装）

```
#include <mysqlx/xdevapi.h>
#include <iostream>

int main() {
    try {
        mysqlx::Client pool(
            "mysqlx://root:123456@192.168.182.129:33060/guisan?ssl-mode=disabled");
        auto sess = pool.getSession();          // 借连接

        mysqlx::RowResult res =
            sess.sql("SELECT id, email FROM users LIMIT 3").execute();

        for (mysqlx::Row row : res) {           // 结果集支持 range-for
            std::cout << row[0].get<int>() << " "
                      << row[1].get<std::string>() << std::endl;
        }
    }
    catch (const mysqlx::Error& e) {
        std::cerr << "MySQL Error: " << e.what() << std::endl;
    }
}

--------------------------------------------------------------------------------------------------------

#include <mysqlx/xdevapi.h>
#include <iostream>

void testMySQL() {
    try {
        // 注意：ssl-mode=disabled 会走明文通道，此时 caching_sha2_password 账号
        // 只能用 SHA256_MEMORY（服务端需已缓存过该账号口令）或 MYSQL41（native 密码），
        // 否则就会报 "Authentication failed using MYSQL41 and SHA256_MEMORY"。
        // 这里保留默认的 TLS（ssl-mode=required，不校验自签证书）。
        mysqlx::Session sess(
            "mysqlx://root:123456@192.168.182.129:33060/chat_db");

        mysqlx::SqlResult res =
            sess.sql("SELECT id, email FROM users LIMIT 10").execute();

        for (mysqlx::Row row : res) {
            // X DevAPI 的 Row 只支持按下标取，不支持按列名
            std::cout << "ID: " << row[0] << ", Email: " << row[1] << std::endl;
        }
        std::cout << "MySQL Connector C++ 测试成功！" << std::endl;
    }
    catch (const mysqlx::Error& e) {
        std::cerr << "MySQL Error: " << e.what() << std::endl;
    }
}

int main() {
    testMySQL();

    return 0;
}

```

URI 逐段拆开看：

```
mysqlx://  root  :  123456  @  192.168.182.129  :  33060  / guisan  ? ssl-mode=disabled
协议        用户名     密码          主机             X端口     默认库      关闭TLS
```

三个必须确认的前提（缺一个就连不上，而且报错各不相同）：

- Docker 容器要映射 X 端口：`-p 3306:3306 -p 33060:33060`（你现在的容器只有 3306，要 `docker rm` 后用新端口重建，数据在挂载卷里不会丢）。
- 容器里 X Plugin 是否在监听：`docker exec -it chat-mysql mysql -uroot -p123456 -e "SHOW VARIABLES LIKE 'mysqlx_port'"`。
- 本地 MySQL 8 的证书是自签的，直接加 `ssl-mode=disabled` 最省事（不加时默认是 `REQUIRED`）。

## 四、必备 API 速查（配等价 SQL）

**1）连接池**

```
mysqlx::Client pool(
    uri,
    mysqlx::ClientOption::POOL_MAX_SIZE,      8,
    mysqlx::ClientOption::POOL_QUEUE_TIMEOUT, std::chrono::milliseconds(1000),
    mysqlx::ClientOption::POOL_MAX_IDLE_TIME, std::chrono::minutes(5));
auto sess = pool.getSession();     // 池满时按 timeout 等待；sess 析构时连接归还并被复用
```

**2）执行原生 SQL + 参数绑定（防注入）**

```
// 位置参数，注意：session.sql() 只支持 ? 这种占位符
auto r = sess.sql("SELECT id FROM users WHERE email = ?").bind(email).execute();
```

**3）链式 CRUD（X DevAPI 特色；注意它用 :名字 命名参数）**

| 操作 | 代码                                                         | 等价 SQL                                      |
| ---- | ------------------------------------------------------------ | --------------------------------------------- |
| 增   | `t.insert("email","passwd").values(email, pwd).execute();`   | `INSERT INTO users(email,passwd) VALUES(?,?)` |
| 查   | `t.select("id","email").where("email = :e").bind("e", email).execute();` | `SELECT id,email FROM users WHERE email='…'`  |
| 改   | `t.update().set("passwd", pwd).where("id = :i").bind("i", id).execute();` | `UPDATE users SET passwd=… WHERE id=…`        |
| 删   | `t.remove().where("id = :i").bind("i", id).execute();`       | `DELETE FROM users WHERE id=…`                |
| 分页 | `.orderBy("id DESC").limit(10).offset(20)`                   | `ORDER BY id DESC LIMIT 10 OFFSET 20`         |

**4）结果集取值（最容易踩坑的地方）**

```
mysqlx::SqlResult r = sess.sql("SELECT id, email FROM users").execute();

if (!r.hasData()) return;                     // INSERT/UPDATE 这类语句没有行数据
for (mysqlx::Row row : r) {
    int id            = row[0].get<int>();
    std::string mail  = row[1].get<std::string>();
    bool is_null      = row[2].is_null();
}
// 只取一条
mysqlx::Row one = r.fetchOne();
if (!one) { /* 没有数据 */ }
// 影响行数 / 自增值
uint64_t affected = r.getAffectedItemsCount();
uint64_t new_id   = r.getAutoIncrementValue();
```

`Row` 只有 `row[下标]`，**没有** `row["email"]`。所以 DAO 里必须固定列顺序（用枚举）。

**5）事务**

```
auto sess = MysqlMgr::GetInst()->GetSession();   // 事务必须在同一条 Session 上
try {
    sess.startTransaction();
    sess.sql("UPDATE users SET points = points - ? WHERE id = ?").bind(100, 1).execute();
    sess.sql("UPDATE users SET points = points + ? WHERE id = ?").bind(100, 2).execute();
    sess.commit();
} catch (const mysqlx::Error& e) {
    try { sess.rollback(); } catch (...) {}      // rollback 自己也可能抛
    // 上报错误
}
```

不显式调 `startTransaction()` 时，每条语句自动提交——概念上就是 JDBC 的 autocommit。

**6）异常**

统一 `catch (const mysqlx::Error& e)`，`e.what()` 里是服务端返回的原始信息（例如 `Table 'guisan.users' doesn't exist`）。建议在 DAO 层把它翻译成你自己的 `ErrorCodes` 再往上返回，别把连接器异常泄漏到 `LogicSystem`。

**7）线程安全**

`Client` 可以多线程共用；`Session` / 结果集**不能**跨线程。你的 `AsioIOServicePool` 是多线程的，所以规矩是：每次数据库操作从池里借一条自己的 Session，用完就还。

## 五、落地到你项目：MysqlMgr + MysqlDao

职责分工就是你文档里写的那套：

- `MysqlMgr`（单例）：管 `Client` 连接池，对外只暴露 `GetSession()`。
- `UserDao`（或其他 Dao）：一次数据库操作，输入输出都是业务结构体，不含任何连接管理代码。
- `LogicSystem`：只调 Dao，不出现任何 `mysqlx::` 类型。

**第一步：`config.ini` 增加 X 端口**（你现在 `[Mysql]` 里的 3306 是经典协议端口，别改它，另加一个）

```
[Mysql]
Host = 192.168.182.129
Port = 3306
XPort = 33060
User = root
Passwd = 123456.
Schema = guisan
```

**第二步：`MysqlMgr.h`**

```
#pragma once
#include "const.h"

class MysqlMgr : public Singleton<MysqlMgr>
{
    friend class Singleton<MysqlMgr>;
public:
    ~MysqlMgr();

    // 从连接池借一条连接，析构时自动归还；池不可用时抛 mysqlx::Error
    mysqlx::Session GetSession();
    bool IsReady() const { return _pool != nullptr; }

private:
    MysqlMgr();
    std::unique_ptr<mysqlx::Client> _pool;
};
```

**第三步：`MysqlMgr.cpp`**

```
#include "MysqlMgr.h"
#include "ConfigMgr.h"

MysqlMgr::MysqlMgr()
{
    auto& cfg = ConfigMgr::GetInst();
    std::string host   = cfg["Mysql"]["Host"];
    std::string xport  = cfg["Mysql"]["XPort"];
    std::string user   = cfg["Mysql"]["User"];
    std::string pwd    = cfg["Mysql"]["Passwd"];
    std::string schema = cfg["Mysql"]["Schema"];
    if (xport.empty()) xport = "33060";

    std::string uri = "mysqlx://" + user + ":" + pwd + "@" + host + ":" +
                      xport + "/" + schema + "?ssl-mode=disabled";
    try {
        _pool = std::make_unique<mysqlx::Client>(
            uri,
            mysqlx::ClientOption::POOL_MAX_SIZE, 8,
            mysqlx::ClientOption::POOL_QUEUE_TIMEOUT, std::chrono::milliseconds(1000),
            mysqlx::ClientOption::POOL_MAX_IDLE_TIME, std::chrono::minutes(5));

        auto probe = _pool->getSession();      // 探活一次，早点发现问题
        std::cout << "MySQL connect success!" << std::endl;
    }
    catch (const mysqlx::Error& e) {
        std::cerr << "MySQL connect failed: " << e.what() << std::endl;
        _pool.reset();
    }
}

MysqlMgr::~MysqlMgr() = default;

mysqlx::Session MysqlMgr::GetSession()
{
    if (!_pool) {
        throw mysqlx::Error("MySQL pool is not ready");
    }
    return _pool->getSession();      // Session 是 move 语义，直接返回即可
}
```

**第四步：`UserDao.h`**

```
#pragma once
#include "const.h"

struct UserInfo {
    int         id = 0;
    std::string email;
    std::string user;
    std::string passwd;   // 生产环境请存哈希，别存明文
    std::string name;
};

class UserDao
{
public:
    bool GetUserByEmail(const std::string& email, UserInfo& out);  // 查不到返回 false
    bool GetUserById(int id, UserInfo& out);
    bool InsertUser(UserInfo& out);                                // 成功回填自增 id
    bool UpdatePasswdByEmail(const std::string& email, const std::string& new_passwd);
    bool DeleteUserById(int id);
    bool TransferPoints(int from_id, int to_id, int points);       // 事务示例

private:
    static UserInfo RowToUser(const mysqlx::Row& row);
};
```

**第五步：`UserDao.cpp`（核心：列顺序 + bind）**

```
#include "UserDao.h"
#include "MysqlMgr.h"

namespace {
    // X DevAPI 只能按下标取值，所以这里固定列顺序，改 SQL 时务必同步改这里
    constexpr const char* USER_COLS = "id, email, user, passwd, name";
    enum UserCol : int { COL_ID = 0, COL_EMAIL, COL_USER, COL_PASSWD, COL_NAME };
}

UserInfo UserDao::RowToUser(const mysqlx::Row& row)
{
    UserInfo u;
    u.id     = row[COL_ID].get<int>();
    u.email  = row[COL_EMAIL].get<std::string>();
    u.user   = row[COL_USER].get<std::string>();
    u.passwd = row[COL_PASSWD].get<std::string>();
    u.name   = row[COL_NAME].get<std::string>();
    return u;
}

bool UserDao::GetUserByEmail(const std::string& email, UserInfo& out)
{
    try {
        auto sess = MysqlMgr::GetInst()->GetSession();
        std::string sql = std::string("SELECT ") + USER_COLS +
                          " FROM users WHERE email = ? LIMIT 1";

        mysqlx::Row row = sess.sql(sql).bind(email).execute().fetchOne();
        if (!row) {
            return false;                 // 查不到：正常业务分支，不是异常
        }
        out = RowToUser(row);
        return true;
    }
    catch (const mysqlx::Error& e) {
        std::cerr << "[UserDao] GetUserByEmail failed: " << e.what() << std::endl;
        return false;                     // 真出错：交给上层决定返回哪个 ErrorCodes
    }
}

bool UserDao::InsertUser(UserInfo& out)
{
    try {
        auto sess = MysqlMgr::GetInst()->GetSession();
        mysqlx::SqlResult res = sess.sql(
            "INSERT INTO users(email, user, passwd, name) VALUES(?, ?, ?, ?)")
            .bind(out.email, out.user, out.passwd, out.name)
            .execute();

        out.id = static_cast<int>(res.getAutoIncrementValue());   // 仅自增主键有效
        return true;
    }
    catch (const mysqlx::Error& e) {
        std::cerr << "[UserDao] InsertUser failed: " << e.what() << std::endl;
        return false;
    }
}

bool UserDao::UpdatePasswdByEmail(const std::string& email, const std::string& new_passwd)
{
    try {
        auto sess = MysqlMgr::GetInst()->GetSession();
        mysqlx::SqlResult res =
            sess.sql("UPDATE users SET passwd = ? WHERE email = ?")
                .bind(new_passwd, email)
                .execute();
        return res.getAffectedItemsCount() > 0;    // 0 表示没匹配到行
    }
    catch (const mysqlx::Error& e) {
        std::cerr << "[UserDao] UpdatePasswd failed: " << e.what() << std::endl;
        return false;
    }
}

bool UserDao::TransferPoints(int from_id, int to_id, int points)
{
    auto sess = MysqlMgr::GetInst()->GetSession();   // 事务必须同一条 Session
    try {
        sess.startTransaction();
        sess.sql("UPDATE users SET points = points - ? WHERE id = ?")
            .bind(points, from_id).execute();
        sess.sql("UPDATE users SET points = points + ? WHERE id = ?")
            .bind(points, to_id).execute();
        sess.commit();
        return true;
    }
    catch (const mysqlx::Error& e) {
        std::cerr << "[UserDao] transfer failed: " << e.what() << std::endl;
        try { sess.rollback(); } catch (...) {}
        return false;
    }
}
```

想用 X DevAPI 的链式 CRUD 版本，同一个方法可以写成这样（注意 `:名字`）：

```
auto sess = MysqlMgr::GetInst()->GetSession();
mysqlx::Table users = sess.getDefaultSchema().getTable("users");   // URI 里已经指定库
mysqlx::SqlResult r = users.update()
                           .set("passwd", new_passwd)
                           .where("email = :email")
                           .bind("email", email)
                           .execute();
```

**第六步：接线到构建**

- `const.h` 里加一行 `#include <mysqlx/xdevapi.h>`（和 redis++ 一样全局可见），或只在 `MysqlMgr.h`/`UserDao.h` 里 include。
- `server/GateServer/CMakeLists.txt` 的 `GATE_SERVER_SOURCES` 里加上 `MysqlMgr.cpp` 和 `UserDao.cpp`。
- 链接目标你已经有了 `unofficial::mysql-connector-cpp::connector`，不用改。
- 运行期别忘了 `mysqlcppconnx-2-vs14.dll` 要和 `GateServer.exe` 同目录（上一轮给的那段 POST_BUILD 拷贝可以兜底）。

## 六、0 基础避坑清单

- 端口：`3306` 是经典协议，X DevAPI 必须 `33060`，且容器要 `-p 33060:33060`。
- 占位符：`sess.sql()` 用 `?`，链式 CRUD 的 `where()` 用 `:名字`，混用会报 "No value for parameter"。
- 认证插件：X DevAPI 原生支持 MySQL 8 默认的 `caching_sha2_password`，**不需要**改 `mysql_native_password`。
- 列顺序：`SELECT` 的列顺序和 `RowToUser` 的下标必须一一对应，这是 X DevAPI 最容易出 bug 的地方。
- `getAutoIncrementValue()` 只在自增主键插入时有值，否则是 0。
- 别在业务代码里拼 SQL 字符串，一律 `bind()`（防注入）。
- `Session` 不能拷贝、不能跨线程；不要为了让"每个请求一个 Session"而写全局 Session。
- 连接池只建一次（放在单例构造函数里），别每次查询都 `new Client`。
- 事务里的多条 SQL 必须在同一个 `Session`；异常路径一定要 `rollback`。
- 中文：建表和库都用 `utf8mb4`（`utf8mb4_0900_ai_ci`），连接器侧默认就是 utf8mb4，不用再 `set names`。
- `insert().values()` 的值个数必须等于列个数，否则报错在 `execute()` 时才出现（构造时不报）。

## 七、给你的三步上手路线

1. 先用第三节那段 demo 单独跑通（确认 33060 能连上，先把"环境"和"代码"的锅分清）。
2. 把 demo 包装成 `MysqlMgr::GetSession()`，只验证一件事：借连接、归还、池上限。
3. 写 `UserDao` 的两个方法 `GetUserByEmail` / `InsertUser`，接到你 `LogicSystem` 的注册/登录 handler 里，替换掉现在只存在 Redis 的临时状态——这时候你的 `MysqlDao/MysqlMgr` 分层就自然成型了。

