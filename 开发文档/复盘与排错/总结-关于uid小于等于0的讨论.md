# 总结:关于 uid ≤ 0 的讨论

> 整理日期：2026-10-14
> 来源：主对话中"在线状态簿记"三个问题（未登录连接误减计数 / `uip_` 被误删 / 重复登录错配）的排查，与侧边聊天中"什么情况下会出现 uid ≤ 0"的推演，合并整理
> 涉及文件：`server/ChatServer/CSession.h`、`CSession.cpp`、`CServer.cpp`、`LogicSystem.cpp`、`UserMgr.cpp`（`server/ChatServer2/` 是同一份代码的副本，改动已同步）
> 行号说明：本文所有行号均以 **2026-10-14 当前工作区**为准，已用 `Select-String` 逐条实测
> 一句话结论：**`uid <= 0` 不是业务值，而是"这条连接还没有身份"这个初始态。它出现在两个性质完全不同的场合——"从未成功登录"和"登录正在进行中"——前者不记账是对的，后者不记账会永久漏掉一笔账。所以只加一句 `uid > 0` 判断挡不住全部问题。**

---

## 0. 结论速览

| 问题 | 结论 |
| --- | --- |
| `uid <= 0` 是什么意思 | 这条连接建立过，但还没有身份 |
| 合法 uid 的下界 | **1**（发号器 `user_id` 从 1 开始，实测当前值 10；`utoken_<uid>` 只会被真实 uid 写入） |
| 什么时候会出现 | 5 个场景，见第 2 节；其中 4 个是 `uid = 0`，但性质分成两类 |
| 断开时应该做什么 | `uid <= 0` → 一件账都不记；`uid >= 1` → 计数 `−1`，并按条件清 `uip_` |
| 只加 `uid > 0` 判断够不够 | **不够**。它挡得住"从未成功登录"的那三个场景，挡不住"登录进行中"那一行（第 3 节） |
| 应该怎么修 | 务实版：把"身份可见"提到"记账"之前；彻底版：用显式 `_counted` 标记 + 同一把锁互斥（第 4 节） |

---

## 1. 为什么 0 可以安全地当哨兵

| 依据 | 结论 |
| --- | --- |
| MySQL `user_id` 表是发号器，从 1 开始逐次 `+1`（实测当前值 `10`） | 业务 uid 一定 **≥ 1** |
| 存储过程 `reg_user` 的返回值：`> 0` 成功、`0` 重名、`-1` 异常回滚 | 只有成功时才会产生 uid，而它 ≥ 1 |
| `uid` 是 `int` | 取值范围里 0 与负数都**不是**合法业务用户 |
| `CSession.cpp:10` 构造时 `_user_id(0)` | 连接一建立就存在，登录是之后才可能发生的事 |

所以 `0` 可以表示"没有用户"。但要注意：**这不是巧合，而是"发号器从 1 开始"这条约定带来的性质**——一旦约定改变（比如 uid 从 0 开始发），哨兵值立刻失效。所以这条约定本身值得写进文档。

---

## 2. 什么情况下会出现 uid ≤ 0（核心）

`_sessions` 在 `accept` 完成的瞬间就登记了 session（`CServer.cpp:70`），而 uid 要等客户端发出 1005、穿过拆帧队列、在工作线程里查完 Redis 之后才写入（`LogicSystem.cpp:186`）。两头之间隔着"一次完整 TCP 往返 + Redis 校验"，所以 **uid 为 0 的条目是常态，不是异常**。

| 场景 | 断开时的 uid | **应该**做什么 | 修复前**实际**会做什么 | 4.0.2 现状 |
| --- | --- | --- | --- | --- |
| 连上后不发 1005（客户端刚启动、端口扫描、探测工具） | 0 | 不记账、不动 `uip_` | 计数 `−1`（错） | 已修正：`uid <= 0` 早退 |
| 发了 1005 但校验失败（无 token / token 不对 / 查不到用户） | 0 | 不记账 | 计数 `−1`（错） | 已修正 |
| 报文畸形（JSON 解析失败、缺字段、非法帧） | 0 | 不记账 | 计数 `−1`（错） | 已修正 |
| **登录处理中连接断掉**（Redis 慢、网络抖动） | **0（尚未写入）** | **补记一次 `−1`** | **什么都不做 → 永久 `+1`** | **仍未解决，见第 3 节** |
| 登录成功后断开 | ≥ 1 | `−1`，并按条件清 `uip_` | 正确 | 正确 |

前四行的 uid 都是 0，但性质分成两类，必须分开看：

**（一）"从未成功登录"（前三行）**：客户端连上就走，或者走到一半被拒绝。注意 `SetUserId` 是登录回调的**最后几步之一**，前面任何一个 `return` 都不会写 uid，所以这些连接在 `_sessions` 里就是 uid = 0 的合法条目，断开时照样会走到 `ClearSession`。这类场景"不记账"是**完全正确**的。

**（二）"登录进行中"（第四行）**：这是最容易漏的一个。看当前代码的顺序：

```cpp
// LogicSystem.cpp 登录回调内的记账段（行号为实测）
174  auto rd_res = RedisMgr::GetInst()->HGet(std::string(LOGIN_COUNT), server_name);  // 读
179  conn_count++;                                                                    // 计数已经 +1
182  RedisMgr::GetInst()->HSet(std::string(LOGIN_COUNT), server_name, conn_str);      // 写
186  sess->SetUserId(uid);                                                             // 身份此刻才可见
190  RedisMgr::GetInst()->Set(ip_key, server_name);                                    // SET uip_<uid>
193  UserMgr::GetInst()->SetUserSession(uid, sess);
```

计数 `+1` 发生在 `SetUserId` **之前**，于是存在一个"**钱已经收了、人还没落名**"的瞬间。此时连接断掉，`ClearSession` 读到的 uid 还是 0 → 判定为"未登录，不记账" → 那次 `+1` 就永远没人抵消。

---

## 3. 为什么只加 `uid > 0` 不够

因为 `uid` 这个数值同时承担了两件事：**"这个用户是谁"**和**"我有没有记过账"**。第 2 节的第四行正好落在两者之间。

| 失效方式 | 后果 | 为什么后果不同 |
| --- | --- | --- |
| 漏减一次（第四行） | 计数**永久偏高 1** | 没人会发现：`StatusServer` 只会把偏高的机器判为"更忙"，只会少派一点流量，不会报错 |
| 多减一次 | 计数偏低，甚至到 0 以下 | 会被 `if (conn_count) conn_count--;` 与 StatusServer 侧的取值夹住，属于"看得见、能兜住"的误差 |

所以方向上的原则是：**宁可少记一次，也不要多记一次**——漏减会被兜住，多加不会被发现。

还有一个放大器：`_user_id` 是**非原子的 `int`**（`CSession.h:36`），由 LogicSystem 工作线程写（`CSession.cpp:34`）、由 io 线程在 `ClearSession` 里读（`CSession.cpp:39`）。这既是标准意义上的数据竞争，也让第四行的窗口从"指令级"扩大到"编译器优化与内存可见性级"。

---

## 4. 两种修法

### 4.1 务实版（建议先做）：让"身份"先于"记账"可见

把 `SetUserId` 提到记账之前：

```cpp
// 现在的顺序：记账 → 落名            // 调整后：落名 → 记账
174  HGet logincount                  sess->SetUserId(uid);                    // ① 身份先可见
179  conn_count++                     UserMgr::GetInst()->SetUserSession(uid, sess);
182  HSet logincount                  // ② 再记账（当前仍是 HGet/HSet 读改写，建议之后换 HIncrBy）
186  sess->SetUserId(uid)             auto rd_res = RedisMgr::GetInst()->HGet(LOGIN_COUNT, server_name);
190  SET uip_<uid>                    ...  conn_count++;  HSet ...
193  SetUserSession                   RedisMgr::GetInst()->Set(ip_key, server_name);
```

效果：第四行的后果从"**永久 +1**"降级为"**最多 −1 的漂移**"，而后者会被 clamp 和"重启归零"兜住。改动量只有几行，不需要新标记、不需要新的锁。

### 4.2 彻底版：显式"已记账"标记 + 同一把锁互斥

把"有没有记过账"从 uid 里解耦出来，做成显式状态：

```cpp
// CSession.h：顺手把 _user_id 也改成原子
std::atomic<int>  _user_id{0};
std::atomic<bool> _counted{false};    // 这条连接是否已经记过账
```

约定：

1. **登录路径**：Redis 记账**完成之后**，在锁内把 `_counted` 置 `true`；
2. **清理路径**：`ClearSession` 在"认领"那一步（从 `_sessions` 里摘掉）读 `_counted`，并且**用的是同一把锁**——这样"记账"与"认领"互相排斥，第四行的窗口才真正消失。

实现代价与注意点：

- 两处必须共用**同一把**锁才有效：现在 `ClearSession` 走 `CServer::_mtx`，登录路径走 `UserMgr::_session_mtx`，直接放两个地方是无效的。最省事的落点是把整个簿记收进 `UserMgr`（它本来就同时管 uid→session 映射和 `_session_mtx`）。
- 登录路径会在锁内做一次 Redis 往返。如果计数已经换成一条 `HINCRBY`（而不是现在"HGet → stoi → HSet"三次），这个代价只有 0.1ms 量级，可以接受。

### 4.3 选型

| | 务实版 4.1 | 彻底版 4.2 |
| --- | --- | --- |
| 改动量 | 调几行顺序 | 新增一个标记 + 统一一把锁 |
| 第四行的残留后果 | 最多 `−1` 漂移（会被 clamp 兜住） | 窗口消失 |
| 是否依赖 `HIncrBy` | 不依赖 | 建议先换，否则锁内三次往返 |
| 适用时机 | 第一版（当前阶段） | 之后重构时随簿记一起做 |

---

## 5. 设计原则（这一节是本文的精华）

> **让"已经产生的副作用"永远不超过"已经被记为已登录"的状态。**

由此派生出三条可执行的规则：

1. **记账放在身份可见之后**——先 `SetUserId`，再动 Redis（务实版就这么做）。
2. **清理以身份为准**——`ClearSession` 先认领（从 `_sessions` 摘掉）、再看 `uid`/`_counted`，而不是"连上就假定已登录"。
3. **主动把 uid 置回 0 的分支必须一步做完**——将来做"同一条连接换账号"时，会先 `SetUserId(0)` 再走 leave + enter；这两步之间**不能中途返回**，否则那条连接断开时就真的不记账了（`uid` 已经被清零，"第四行"的窗口被人为放大）。

---

## 6. 代码里现有的四处判定（实测行号）

| # | 位置 | 写法 | 语义 | 覆盖了第 2 节的哪几行 |
| --- | --- | --- | --- | --- |
| 1 | `CSession.cpp:10`（构造） | `_user_id(0)` | 哨兵初值 | — |
| 2 | `CServer.cpp:44` | `if (uid <= 0) return;` | 从未登录 → 一件账都不记 | 前三行 ✅ / **第四行 ❌（所以需要 4.1 或 4.2）** |
| 3 | `LogicSystem.cpp:128` | `if (uid <= 0 \|\| token.empty()) { root["error"] = Uid_Invalid; return; }` | 请求本身不合法，直接拒（在查 `utoken_0` 之前） | — |
| 4 | `UserMgr::SetUserSession` / `RmvUserSession` | 不判 | 键空间天然不含 0 | — |

第 2 处所在的完整函数（`CServer.cpp:21-60`）就是三个必须同时成立的删除条件：

```cpp
void CServer::ClearSession(const std::string& session_id)
{
	std::lock_guard<std::mutex> lk(_mtx);

	auto it = _sessions.find(session_id);
	if (it == _sessions.end()) return;          // 幂等：已经处理过

	int uid = it->second->GetUserId();
	std::string server_name = ConfigMgr::GetInst()["SelfServer"]["Name"];

	// 条件 C：本机是否还有该 uid 的其它会话（排除自己）
	bool has_other_local = false;
	if (uid > 0) {
		for (auto& kv : _sessions) {
			if (kv.first == session_id) continue;
			if (kv.second->GetUserId() == uid) { has_other_local = true; break; }
		}
	}

	_sessions.erase(it);                        // 先摘表：后面怎么 return 都不会重复处理
	if (uid <= 0) return;                       // 条件 A：从未登录 → 不记账，也不动 uip_

	UserMgr::GetInst()->RmvUserSession(uid);

	auto rd_res = RedisMgr::GetInst()->HGet(std::string(LOGIN_COUNT), server_name);
	int conn_count = rd_res.empty() ? 0 : std::stoi(rd_res);
	if (conn_count) conn_count--;
	RedisMgr::GetInst()->HSet(std::string(LOGIN_COUNT), server_name, std::to_string(conn_count));

	if (has_other_local) return;                // 条件 C
	std::string cur_owner;
	if (!RedisMgr::GetInst()->Get(std::string(USERIPPREFIX) + std::to_string(uid), cur_owner)) return;
	if (cur_owner != server_name) return;       // 条件 B：归属已经被别的机器接管
	RedisMgr::GetInst()->Del(std::string(USERIPPREFIX) + std::to_string(uid));
}
```

| 条件 | 判断 | 不满足时说明 |
| --- | --- | --- |
| A. 这条会话真的登录过 | `uid > 0` | 连上就断的探测连接，既没加过计数也没写过归属 |
| B. 归属还是我这台 | `GET uip_<uid>` 等于自己的 `[SelfServer]Name` | 用户已在别台登录并覆盖了 `uip_`，这台无权再删 |
| C. 本机没有该 uid 的其它会话 | 扫 `_sessions` 排除自己 | 同机多开同一账号，或同一连接重登过 |

顺序上有一个容易忽略的细节：**条件 C 必须在"自己已经不算在表里"的前提下才算得准**（代码靠"遍历时排除自己"达到同样效果），否则自己还在表里，`has_other_local` 永远为真。

---

## 7. 为什么写 `<= 0` 而不是 `== 0`

- `== 0` 只覆盖"未登录"这一种已知情况；`<= 0` 顺带把任何来源的非正 uid（未来的 bug、别处传错的参数、客户端伪造的负值）一并挡住。
- 代价为零：正常路径下真正会出现的值只有 `0` 和 `≥1`，多挡的一侧是空的。
- 更重要的原则是：**哨兵值必须配显式判断，不能靠"默认值恰好是 0"来兜**。原代码的问题不是"选错了 0"，而是**没有任何地方把"0 表示未登录"写成代码**——它只存在于 `CSession` 构造函数的初始化列表里。

---

## 8. 同源问题清单（都属于"第一版之后再重构"）

| # | 问题 | 与 uid ≤ 0 的关系 | 建议修法 |
| --- | --- | --- | --- |
| S1 | **登录进行中断开 → 永久 `+1`**（第 2 节第四行） | 同一件事：`uid` 身兼"我是谁"与"记过账没有" | 4.1 务实版 / 4.2 彻底版 |
| S2 | 同一条连接重复发 1005 会重复 `+1` | 同上，断开只 `−1` → 计数永久偏高 | 登录前判 `sess->GetUserId() != uid`：相等说明已记过账，只重新校验并回包，不再 `+1` |
| S3 | 同一条连接换 uid 登录留下"幽灵条目" | `UserMgr` 里会残留"旧 uid → 同一 session"，将来跨节点推送可能把消息发给另一个用户名下的连接 | 换 uid 时先按旧 uid 走一次"离开"，再按新 uid"进入"；注意第 5 节的第 3 条规则 |
| S4 | `_user_id` 是非原子的 `int` | 它既是 S1 的放大器，也是标准数据竞争 | `CSession.h:36` 改成 `std::atomic<int>` |
| S5 | `logincount` 读改写非原子 | 让 S1/S2 造成的偏差更难被发现（并发时会互相覆盖） | 给 `RedisMgr` 补 `HIncrBy`，两侧都改成 `HINCRBY ±1` |

---

## 9. 验收:用五组对账确认"账不再乱"

每步之间读一次 `HGETALL logincount` 与 `GET uip_<uid>`：

| # | 场景 | 期望结果 | 验的是 |
| --- | --- | --- | --- |
| 1 | 连上、不发 1005 就断开 | 计数不变、不产生 `uip_` | 前三行（条件 A） |
| 2 | 正常登录后断开 | 计数 `+1` 再 `−1`，`uip_` 出现后消失 | 基本路径 |
| 3 | chatserver1 登录 → chatserver2 登录 → 断开 chatserver1 那条 | `GET uip_<uid>` 仍为 `chatserver2` | 条件 B（需两台实例；也可手工 `SET uip_<uid> chatserver2` 模拟） |
| 4 | 发 `{"uid":"abc","token":"t"}` / `uid = -1` / `uid = 0` | 回 `{"error":1011}` 或 `{"error":1001}`，进程存活，紧接着正常登录仍成功 | 校验层 + 工作线程未被打死 |
| 5 | **"登录进行中断开"（第四行）** | 计数最终回到 0（或至少不出现永久 `+1`） | 4.1 / 4.2 的有效性 |

第 5 组怎么造：在登录回调的记账之后、`SetUserId` 之前**临时插一句 `std::this_thread::sleep_for(3s)`**（或用 `redis-cli DEBUG SLEEP` 拖慢 Redis），然后在那 3 秒内把客户端连接掐掉，观察 `logincount` 是否从此永久高 1。验证完把 sleep 删掉即可。

---

## 10. 口诀

> **uid ≤ 0 = 这条连接还没有身份。减在线数、删归属、注销映射这三件事，动手前先问一句"uid > 0 吗"。**
>
> **但更要记住：账不能记在身份之前——"已经产生的副作用"永远不许超过"已被记为已登录"的状态。**

---

## 附:代码位置索引（2026-10-14 实测）

| 文件:行 | 内容 |
| --- | --- |
| `CServer.cpp:21-60` | `ClearSession`：幂等认领 + 条件 A/B/C |
| `CServer.cpp:44` | `if (uid <= 0) return;`（条件 A） |
| `CServer.cpp:70` | `_sessions[new_session->GetSessionId()] = new_session;`（accept 即登记，这是"uid = 0 的条目是常态"的来源） |
| `LogicSystem.cpp:82-95` | `DealMsg` 全局 try/catch + `_msg_que.pop()` |
| `LogicSystem.cpp:104` | `Defer defer`（提前构造，任何分支都回包） |
| `LogicSystem.cpp:118-128` | 显式类型校验 + `uid <= 0 \|\| token.empty()` |
| `LogicSystem.cpp:174-193` | 登录记账段：`HGet`(174) → `conn_count++`(179) → `HSet`(182) → `SetUserId`(186) → `SET uip_`(190) → `SetUserSession`(193) |
| `CSession.h:36` | `int _user_id;`（非原子，建议改 `std::atomic<int>`） |
| `CSession.cpp:10` | `_user_id(0)` 构造初值 |
| `CSession.cpp:34` / `:39` | `SetUserId` / `GetUserId` |

> 注：本文初版引用过一组更早的行号（`SetUserId` 在 155 行、accept 登记在 38 行），那是修复并同步两份副本**之前**的版本；本次已按当前工作区重新实测。

---

## 附:相关文档

| 文档 | 关系 |
| --- | --- |
| `服务端架构与处理流程4.0.md` | 本文对应的架构描述；第 3.2 / 5.5 / 5.6 节与 6.2.1 记录了这次修复的前后状态 |
| `json获取数据不同写法：.md` | 第 6 节判定 3 的落地依据（`value` / `at` / `contains` 的取舍） |
| `踩坑复盘-登录成功但界面不跳转.md` | 同一条长连接链路上的另一次排查（帧长度字段），与本文的簿记问题互补 |
