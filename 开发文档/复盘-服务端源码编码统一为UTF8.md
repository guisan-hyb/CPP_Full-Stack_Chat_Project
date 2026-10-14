# 复盘：服务端源码编码从 GBK 统一到 UTF-8

> 复盘日期：2026-10-14
> 触发点：服务端源码出现"44 个 GBK + 100 个 UTF-8"混用，而根 `CMakeLists.txt` 早已全局 `add_compile_options(/utf-8)`
> 涉及范围：`server/` 下 44 个文件（ChatServer 10、ChatServer2 10、GateServer 13、StatusServer 11，含 3 个 `CMakeLists.txt`）
> 一句话结论：**混用编码时不存在"正确的编译选项"——无论 `/utf-8` 开不开，总有一组文件报错、总有一组文件里的中文字节是"另一种编码"。所以唯一解是把源码统一到 UTF-8（无 BOM），并且顺手把"防复发"的约束补上（`.editorconfig`）。**
> 与其它文档的关系：本篇是这次编码整改的完整复盘；`服务端架构与处理流程4.0.md` 第 9 章是它的方案与执行记录摘要，第 4.5 节与《踩坑复盘-登录成功但界面不跳转.md》第 3.4 节里"必须按 936 读写"的旧说明已同步作废。

---

## 0. 阅读指南

- 第 1–3 章讲**为什么必须改**（含 MSVC 实测证据），第 4–6 章讲**怎么改、怎么验**，第 7–9 章是**没做到的、踩过的坑、防复发**。
- 第 10 章是可复用的**速查命令与脚本**，以后要再盘点/再转换，直接抄。
- 所有"实测"都标注了采集方式，不是推断。

---

## 1. 起因:一条被忽略的地基

### 1.1 表面症状

用任何 UTF-8 工具读服务端源码，中文注释都是乱码，例如：

```
// ����¼������Ϊ 0
1.����Դ�ļ�
```

第一份文档里也留下了这条记录："服务端这几份源码是 GBK 编码，直接用 UTF-8 工具整文件重写会把中文注释变成乱码，做法是按 936 读、按 936 写。"——也就是说，**每次改服务端代码都要额外做一次编码往返**，这就是持续在付的税。

### 1.2 更根本的矛盾

根 `CMakeLists.txt` 里已经有：

```cmake
if(MSVC)
    add_compile_options(/utf-8)   # 强制源码字符集与执行字符集都是 UTF-8
endif()
```

而它作用的 44 个文件其实是 GBK。**编译器被告诉"源码是 UTF-8"，文件却是 GBK**——这是一个一直存在、却因为没有中文字面量而没有爆发的矛盾。

---

## 2. 现状盘点（实测）

判定方法：逐文件"严格 UTF-8 试解码，失败则回退 GB18030"（不能用"含非 ASCII 就当 GBK"，见第 8 章坑 6）。

| 位置 | 整改前 GBK | 整改前已是 UTF-8（含非 ASCII） | 纯 ASCII（无编码概念） |
| --- | --- | --- | --- |
| `server/ChatServer/` | 10（含 `const.h`、`data.h`、`CMakeLists.txt`） | 0 | 22 |
| `server/ChatServer2/` | 10（与上面同名的副本） | 0 | 22 |
| `server/GateServer/` | 13 | 4（`GateServer.cpp`、`LogicSystem.cpp`、`RedisMgr.cpp`、`CMakeLists.txt`） | 8 |
| `server/StatusServer/` | 11 | 1（`RedisMgr.cpp`） | 7 |
| `server/` 根与其它子目录 | 0 | 5（`server/CMakeLists.txt`、`proto/message.proto`、`VerifyServer/*.js` × 3） | 6 |
| `client/` | 0 | 64 | 34 |
| `开发文档/` | 0 | 27（含本篇） | 0 |
| 仓库根 | 0 | 1（`CMakeLists.txt`） | 1 |

（口径：上表统计的是"文本类文件"——`.h/.cpp/.proto/.ini/.js/.json/.md/.txt/.ui/.qss/.pro`，已排除 `node_modules`、`build`、`out`、`.vs`、`x64/Debug`。）

两个关键发现：

1. **GBK 全部集中在 `server/`**，而且**两份 ChatServer 副本都中招**；3 个 `CMakeLists.txt`（ChatServer / ChatServer2 / StatusServer）也在其中——这就是 CMake 报错时输出乱码（`1.����Դ�ļ�`）的原因。
2. **服务端的中文字符串字面量是 0 个**：344 行含中文，全部是注释。所以当时**没有运行时故障**，问题只在"可读性 + 编译噪音 + 未来风险"。这一点决定了整改的紧迫性排序：不紧急，但必须做，因为下一步就要写中文日志。

---

## 3. 为什么"混用"必须解决（MSVC 实测）

### 3.1 对照实验

同一个 3 行探针文件（含中文行注释、中文块注释、中文字面量 `"中文"`），分别存成 GBK / UTF-8，各用 VS2022 Community 14.44 的 `cl.exe` 编译两次，再直接扫描 `.obj` 里那两个字面量的字节：

| 源文件编码 | 编译选项 | 诊断 | `.obj` 里 `"中文"` 的实际字节 |
| --- | --- | --- | --- |
| GBK | 无 `/utf-8`（默认 936） | 无 | `D6 D0 CE C4`（GBK） |
| GBK | `/utf-8` ← 整改前的状态 | **20 条 `warning C4828`** | `D6 D0 CE C4`（**仍是 GBK**） |
| UTF-8 | 无 `/utf-8` | `warning C4819` | `E4 B8 AD E6 96 87`（UTF-8） |
| UTF-8 | `/utf-8` | 无 | `E4 B8 AD E6 96 87`（UTF-8） |

### 3.2 三条结论

1. **运行时字节 = 源文件字节**。遇到无法解码的字节，MSVC 原样透传并告警，既不报错也不替换成 U+FFFD（探针里专门搜过 `EF BF BD`，未出现）。所以"服务端产出的中文是什么编码"，由源码编码直接决定。
2. **混用时不存在正确的编译选项**：不开 `/utf-8`，UTF-8 文件报 `C4819`；开了，GBK 文件报 `C4828`。只能选"哪一半文件报错"，不可能两边都对。
3. 整改前的组合（GBK + `/utf-8`）是最差的一种：**既有警告洪水，又没有换来任何正确性**。

### 3.3 代价量化

| 文件 | 高位字节 | 单文件 `C4828` 条数 |
| --- | --- | --- |
| `ChatServer/LogicSystem.cpp` | 382 | **208** |
| `StatusServer/StatusServiceImpl.cpp` | 88 | **54** |

按 44 个文件约 6600 个高位字节推算，**每次全量编译约 3000+ 条**。这直接解释了为什么当初必须加 `/WX-`（关闭"警告即错误"）——代价是真正的警告也被淹没。

### 3.4 那"一直没出事"是为什么

顺便测了两个字节级隐患，结论是极小/为零：

- GBK 字符尾字节撞上 `\`、`"`、`*`、`/` 的情况：**0 处**（这类碰撞会导致注释提前闭合、字符串被转义）；
- 全项目块注释只有 **2 处**，块注释提前闭合的暴露面几乎为零。

所以"没出事"是运气 + 中文只出现在行注释里的共同结果，不是设计。

### 3.5 真正会踩的坑在哪

一旦写下第一句中文日志或中文错误提示：

- 该字面量会以 **GBK 字节**进 `.obj`、进运行时；
- 送给客户端（UTF-8）→ **客户端显示乱码**；
- 打进控制台（936 代码页）→ **本地看着正常**，于是"本地对、客户端乱"极难排查。

---

## 4. 目标状态与判据

| 项 | 目标 |
| --- | --- |
| 源文件编码 | 全部 UTF-8 **无 BOM**（`.proto` / `.ini` / `.js` 尤其不能有 BOM） |
| 编译选项 | 保持 `/utf-8`（它才是"源码全 UTF-8"这一目标的正确配套） |
| 判定规则 | 只转换"严格 UTF-8 解码失败"的文件；能通过严格解码的一律跳过（避免二次转换） |
| 解码码页 | 用 **GB18030（54936）** 而不是 GBK(936)：它是 936 的超集，不会出现"解不出来被替换成 `?`" |
| 防复发 | `.editorconfig` 锁定 `charset = utf-8`；`.git-blame-ignore-revs` 登记这次整体重写 |
| 控制台 | 中文日志需要 `SetConsoleOutputCP(CP_UTF8)` 或 `chcp 65001`（当前无中文字面量，暂不影响） |

---

## 5. 执行过程

### 5.1 步骤

1. **提交基线**：转换会让整个文件进入 diff，必须先让工作区干净，让"纯编码转换"独立成一个提交。
2. **判定 + 转换**：严格 UTF-8 试解码 → 失败者按 GB18030 解码 → 以 UTF-8 无 BOM 写回。
3. **逐文件校验**：转后能严格按 UTF-8 解码；**ASCII 骨架逐字节一致**；失败即回滚该文件。
4. **编译验证**：`cl.exe` 全量语法+语义检查（见第 6 章）。

### 5.2 脚本要点

```powershell
$gb     = [Text.Encoding]::GetEncoding(54936)          # GB18030
$u8     = New-Object System.Text.UTF8Encoding($false)   # UTF-8 无 BOM
$strict = New-Object System.Text.UTF8Encoding($false, $true)   # 严格模式:非法字节会抛异常

foreach ($f in $files) {
    $old = [IO.File]::ReadAllBytes($f.FullName)

    # ① 已经是合法 UTF-8(或纯 ASCII)就跳过,绝不二次转换
    try { [void]$strict.GetString($old); continue } catch { }

    # ② 按 GB18030 解码,以 UTF-8 无 BOM 写回
    [IO.File]::WriteAllText($f.FullName, $gb.GetString($old), $u8)

    # ③ 校验:必须能严格解码 + ASCII 骨架与原文完全一致,否则回滚
    $new = [IO.File]::ReadAllBytes($f.FullName)
    try { [void]$strict.GetString($new) } catch { [IO.File]::WriteAllBytes($f.FullName, $old); continue }
    $a1 = [byte[]]($old | Where-Object { $_ -lt 128 })
    $a2 = [byte[]]($new | Where-Object { $_ -lt 128 })
    if ($a1.Length -ne $a2.Length -or (Compare-Object $a1 $a2)) { [IO.File]::WriteAllBytes($f.FullName, $old) }
}
```

要点：**用 `[IO.File]::WriteAllText` + `UTF8Encoding($false)`，不要用 `Set-Content -Encoding UTF8`**（PowerShell 5.1 会写 BOM，见第 8 章坑 2）。

### 5.3 执行结果

```
转换成功 44   跳过 61（已是 UTF-8 或纯 ASCII）   回滚 0
```

文件体积普遍增加（GBK 汉字 2 字节 → UTF-8 汉字 3 字节），例如 `ChatServer/const.h` 2456 → 2698 字节、`LogicSystem.cpp` 8288 → 8479 字节——这是预期现象，不是内容变化。

---

## 6. 验证：四层证据

| 层 | 方法 | 结果 |
| --- | --- | --- |
| ① 字节级 | 每个文件的 `git diff --numstat` 行数是否**恰好等于**该文件含非 ASCII 的行数 | **44/44 全部命中** → 除中文所在行，其余字节一个都没动 |
| ② 形态 | 检查 BOM、U+FFFD、抽查中文内容 | 无 BOM、无 U+FFFD；`CServer.cpp:44`、`GateServer/MysqlDao.cpp:129` 等显示正常 |
| ③ 编译诊断 | 同一文件转换前后用 `cl /c /utf-8` 对比 `C4828` 条数 | `ChatServer/LogicSystem.cpp` 208 → **0**；`StatusServer/StatusServiceImpl.cpp` 54 → **0** |
| ④ 编译 | `cl.exe /Zs /utf-8` + vcpkg/MSVC/Windows SDK 头文件路径，全量检查服务端 `.cpp` | **47/47 通过，0 失败**（25 个本次转换的 + 22 个本就 UTF-8 的对照组） |

第 ① 层是最有说服力的一条：`numstat` 的增删行数等于"含中文的行数"，等价于"只有含中文的行变了"；而 UTF-8 的多字节序列**永远不会出现 <0x80 的字节**，所以引号、反斜杠、括号这些语法符号的位置与数量在转换前后完全一致——**词法结构不可能被这次转换改变**。

---

## 7. 没能验证的部分

**完整 CMake 构建没能在协作环境里跑通**，原因是环境而非代码：

- 沙箱内：MSBuild 报 `对路径 C:\Users\guisan\AppData\Local\Microsoft SDKs 的访问被拒绝`（SDK 探测被拦）；
- 放开权限后：CMake 在**编译器识别**阶段报 `No preprocessor test for "Renesas"`，仍未产生任何编译单元。

这与文件内容无关（转换前后 CMake 的解析输入按第 6 章①已证明等价）。**建议在 Visual Studio 里点一次"生成"做最终确认**——首次会因 `server/` 下 3 个 `CMakeLists.txt` 的 mtime 变化而重新 configure，属正常现象。

---

## 8. 踩过的坑

1. **探针命令被截断，制造了一个僵进程。** 第一次用裸 RESP 探 Redis 时写了 `"KEYS *`r`n"` 却只发送 7 字节，少了行尾的 LF，Redis 一直等命令结束，进程挂死。教训：**协议类探针要显式校验"发送字节数 == 缓冲区长度"**，这类错误的表现是"没有输出、也不报错"。
2. **PowerShell 5.1 的 `Set-Content -Encoding UTF8` 会写 BOM。** 对 `.proto` / `.ini` / `.js` 而言 BOM 可能顶掉第一个 key。改用 .NET 的 `[IO.File]::WriteAllText(..., UTF8Encoding($false))`。
3. **行号会漂移，引用前必须现场实测。** 侧边聊天里那轮推演引用的 `SetUserId` 在 155 行、accept 登记在 38 行，修复并同步副本后实际是 186 / 70。凡是文档里写行号，都要在写之前 grep 一次。
4. **不能"含非 ASCII 就当 GBK"。** 整改前服务端已经有 **10 个**含非 ASCII 的 UTF-8 文件（7 个 C++/CMake/proto：`server/CMakeLists.txt`、`GateServer/CMakeLists.txt`、`GateServer.cpp`、`GateServer/LogicSystem.cpp`、`GateServer/RedisMgr.cpp`、`StatusServer/RedisMgr.cpp`、`proto/message.proto`；外加 `VerifyServer/email.js`、`redis.js`、`server.js`），按字节特征一刀切会把它们**二次转换**成乱码。必须用"严格 UTF-8 试解码失败"作为判据。
5. **沙箱只给了仓库 `.git` 只读权限。** 建分支、提交都要单独放开；MSBuild 还需要读工作区外的 SDK 路径。这不是代码问题，遇到时直接申请放开即可。
6. **转换必须单独成一个提交。** 44 个文件整体重写会把业务 diff 淹没，也会污染 `git blame`——所以这次是"转换"和"配置/文档"两笔提交，并补了 `.git-blame-ignore-revs`。
7. **"编译不过"要先分清是 configure 还是 compile。** 本次的失败全部发生在 CMake configure（环境问题），编译单元一个都没跑到；改用 `cl.exe` 直编后 47/47 通过。

---

## 9. 防复发

| 措施 | 说明 |
| --- | --- |
| `.editorconfig` | `charset = utf-8` 作为硬约定，顺带把各行尾/缩进现状写下来；这是当初能出现"44 GBK + 100 UTF-8"混用的根本原因（仓库当时没有任何编码约束文件） |
| `.git-blame-ignore-revs` | 登记本次整体重写提交，避免 `git blame` 被它覆盖。本地启用一次：`git config blame.ignoreRevsFile .git-blame-ignore-revs`；GitHub 会自动识别 |
| 新文件默认 UTF-8 | 编辑器默认即可；**不要**用"UTF-8 带签名"另存 `.proto` / `.ini` / `.js` |
| 控制台 | 开始写中文日志时，在 `main()` 加 `SetConsoleOutputCP(CP_UTF8);`（需要 `<windows.h>`），或终端 `chcp 65001` |
| 定期体检 | 用第 10 章的盘点脚本，任何时候都能一眼看出"还有没有非 UTF-8 文件" |

---

## 10. 速查（附录）

**盘点：仓库里还有没有非 UTF-8 的文本文件**

```powershell
$encU8 = New-Object System.Text.UTF8Encoding($false, $true)
Get-ChildItem -Path . -Recurse -File |
  Where-Object { $_.Extension -in '.h','.cpp','.proto','.ini','.js','.json','.md','.txt','.ui','.qss','.pro' -and
                 $_.FullName -notmatch '\\(node_modules|x64|Debug|Release|build|out|\.vs|\.git)\\' -and $_.Length -lt 300000 } |
  ForEach-Object {
    $b = [IO.File]::ReadAllBytes($_.FullName)
    $high = $false; foreach ($x in $b) { if ($x -ge 128) { $high = $true; break } }
    if (-not $high) { return }                       # 纯 ASCII：没有编码概念
    try { [void]$encU8.GetString($b) } catch { "GBK?  $($_.FullName)" }
  }
```

**启用 blame 忽略表**（每台机器一次）

```powershell
git config blame.ignoreRevsFile .git-blame-ignore-revs
```

**转换脚本**：见第 5.2 节（注意判据是"严格 UTF-8 解码失败"，且转换前先让工作区干净）。

---

## 附:相关文档

| 文档 | 关系 |
| --- | --- |
| `服务端架构与处理流程4.0.md` | 第 9 章是本篇的方案+执行记录摘要；4.5 节的"源码编码"一行已改为"全部 UTF-8"，修订记录见 4.0.3 |
| `踩坑复盘-登录成功但界面不跳转.md` | 第 3.4 节原先要求"按 936 读写"，现已标注失效并指向本篇 |
| `总结-关于uid小于等于0的讨论.md` | 同期的簿记问题复盘；本篇第 3 章"运行时字节 = 源文件字节"的结论对理解它的日志排查也有帮助 |
