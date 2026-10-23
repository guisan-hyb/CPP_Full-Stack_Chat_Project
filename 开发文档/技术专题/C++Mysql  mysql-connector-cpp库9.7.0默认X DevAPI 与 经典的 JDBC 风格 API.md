## C++/Mysql  `mysql-connector-cpp`库9.7.0默认X DevAPI 与 经典的 JDBC 风格 API

我把你机器上的 vcpkg 实际安装内容和你改了之后的代码/CMake 都核对了一遍，问题定位很明确。

## 根因：你装的 `mysql-connector-cpp` 里根本没有 JDBC 那套 API

vcpkg 的 `mysql-connector-cpp` 9.7.0，**默认只编译并安装 X DevAPI**（`mysqlcppconnx`）；经典的 JDBC 风格 API（`jdbc/*.h`、`mysqlcppconn.lib`）属于可选 `jdbc` feature，而这个 feature 在 vcpkg 里**只支持静态 triplet**。

我实际查到的安装结果：

```
C:\vcpkg\installed\x64-windows\include\mysqlx\      ← 有（X DevAPI）
C:\vcpkg\installed\x64-windows\include\jdbc\        ← 不存在
C:\vcpkg\installed\x64-windows\lib\mysqlcppconnx.lib
C:\vcpkg\installed\x64-windows\bin\mysqlcppconnx-2-vs14.dll
share\unofficial-mysql-connector-cpp\...targets.cmake ← 只导出 connector 这一个 target
```

而你在 `GateServer.cpp` 里写的是：

```
#include <jdbc/mysql_driver.h>     // ← 这个头文件在你机器上不存在
#include <jdbc/cppconn/statement.h>
```

所以“打不开库”会按你卡在哪一步表现成三种不同报错：

| 报错                                            | 含义   | 原因                                 |
| ----------------------------------------------- | ------ | ------------------------------------ |
| `C1083: 无法打开包括文件 "jdbc/mysql_driver.h"` | 编译期 | 装的是 X DevAPI 版，没有 jdbc 头文件 |
| `LNK1104: 无法打开文件 "xxx.lib"`               | 链接期 | 没有 `mysqlcppconn.lib` 这个导入库   |
| 运行时报“找不到 mysqlcppconnx-2-vs14.dll”       | 运行期 | DLL 没跟 exe 放同一目录              |

你先用这两条命令自查一下，确认我说的和你看到的一致：

```
Get-ChildItem C:\vcpkg\installed\x64-windows\include | Select-Object Name
Get-ChildItem C:\vcpkg\installed\x64-windows\lib  -Filter *mysql*, C:\vcpkg\installed\x64-windows\bin -Filter *mysql*
```

CMake 部分你其实已经写对了：`find_package(unofficial-mysql-connector-cpp CONFIG REQUIRED)` + 链接 `unofficial::mysql-connector-cpp::connector` 是有效的，报错不在 CMake，在“这个包不提供你用的那套 API”。

## 方案 A（推荐，不用重装任何东西）：改用 X DevAPI 写测试代码

这是 vcpkg 已经给你装好的 C++ API，CMake 一行都不用改。把 `GateServer.cpp` 里的 `testMySQL()` 换成：

```
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

两个必须注意的点：

1. **X DevAPI 走的是 X 协议 33060 端口，不是 3306。** 你文档里的 `docker run` 只映射了 3306，所以现在直接连会失败。重建容器时补上端口（数据在挂载卷里，不会丢）：

```
docker stop chat-mysql && docker rm chat-mysql
docker run -d --name chat-mysql -p 3306:3306 -p 33060:33060 \
  -e MYSQL_ROOT_PASSWORD=123456 \
  -v /mydata/mysql/data:/var/lib/mysql \
  mysql:8.0 --character-set-server=utf8mb4 --collation-server=utf8mb4_unicode_ci
```

1. 验证 X 插件确实在监听：`docker exec -it chat-mysql mysql -uroot -p123456 -e "SHOW VARIABLES LIKE 'mysqlx_port'"`。

## 方案 B（想保留 `#include <jdbc/...>` 的写法）：换静态 triplet

JDBC feature 只支持静态 triplet，所以整个项目的依赖都要为 `x64-windows-static-md` 重装一遍（boost、grpc、protobuf、redis++ 全都要重新编译，**大概几十分钟到两小时**）：

```
cd C:\vcpkg
.\vcpkg install mysql-connector-cpp[jdbc]:x64-windows-static-md
.\vcpkg install grpc protobuf boost redis++ nlohmann-json openssl:x64-windows-static-md
```

然后 `CMakePresets.json` 的 cacheVariables 加上 triplet，并把链接目标换成 jdbc 那个：

```
"VCPKG_TARGET_TRIPLET": "x64-windows-static-md"
unofficial::mysql-connector-cpp::connector-jdbc   # 替代 ::connector
```

改完**必须删掉 `build` 和 `.vs` 重新 Configure**，否则旧 triplet 的缓存会继续捣乱。

## 方案 C（最轻量的替代）：用 `libmysql` 的 C API

如果你不执着于 C++ 类接口，`libmysql` 安装快、走 3306、和你 Docker 现状最契合。注意你文档里写的 target 名是错的，正确的是：

```
find_package(unofficial-libmysql CONFIG REQUIRED)
target_link_libraries(GateServer PRIVATE unofficial::libmysql::libmysql)
#include <mysql.h>
MYSQL* conn = mysql_real_connect(mysql_init(nullptr),
    "192.168.182.129", "root", "123456", "chat_db", 3306, nullptr, 0);
```

## 运行期 DLL 的通用保险

只要链的是动态库，`mysqlcppconnx-2-vs14.dll` 就必须和 `GateServer.exe` 同目录。你的 `CMakeCache.txt` 里 `VCPKG_APPLOCAL_DEPS=ON`，正常重新构建一次 vcpkg 会自动拷；如果还是不放心，在 `server/GateServer/CMakeLists.txt` 末尾加一段（`$<TARGET_FILE:...>` 会自动按 Debug/Release 选对应的 DLL）：

```
add_custom_command(TARGET GateServer POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:unofficial::mysql-connector-cpp::connector>"
        "$<TARGET_FILE_DIR:GateServer>"
)
```

顺带说一句，你文档里记的那个坑（`target_link_libraries(main PRIVATE ...)` 会报 `LNK1104: 无法打开文件 "main.lib"`）也对——你现在写的是 `GateServer PRIVATE`，这块没问题。

我的建议是走方案 A：它零重装，而且 X DevAPI 恰好就是 `mysql-connector-cpp` 现在主推的 C++ 接口，你后面封装 `MysqlDao`/`MysqlMgr` 时智能指针和异常这两点它同样具备（只是结果集取列要用下标，或者先用 `res.getColumnNames()` 建个名字到下标的映射）。

需要我直接动手改吗？我可以把 `GateServer.cpp` 的测试函数换成上面这版 X DevAPI 代码（保留你原来的 `main` 结构），或者你要是决定走方案 B，我就把 preset 和两个 CMakeLists 一起改成 static-md + `connector-jdbc`。