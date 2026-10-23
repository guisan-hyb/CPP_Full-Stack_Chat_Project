# P0 Session 身份校验与好友消息授权

## 1. 文档目标

本文记录聊天业务的身份可信边界改造，重点解决：

- 客户端能否伪造自己的 UID。
- 好友申请和认证应以谁为操作者。
- 聊天发送者应该由谁决定。
- 什么条件下允许发送文本消息。
- 跨 ChatServer 转发时应传递哪一组身份。
- 如何为后续测试和日志追踪保留稳定字段。

本次改造对应提交：

- `security(session): bind friend actions to login identity`

## 2. 原问题

TCP 登录成功后，Session 已经保存了认证用户：

```cpp
sess->SetUserId(uid);
```

但部分业务处理器仍然直接读取请求 JSON：

```cpp
auto uid = src_root.value("uid", 0);
auto uid = src_root.value("from_uid", 0);
```

这意味着任何已登录用户都可以修改请求体，尝试以其他用户身份：

- 发起好友申请。
- 同意好友申请。
- 发送文本消息。
- 影响通知中的 `from_uid`。

问题本质是混淆了两种字段：

```text
认证身份：当前 TCP Session 对应的用户，由服务端确认。
业务参数：目标用户、申请说明、消息内容，可以来自客户端。
```

客户端可以决定“要对谁操作”，不能决定“我是谁”。

## 3. 改造目标

本次遵循三条规则：

1. 当前操作者一律取 `sess->GetUserId()`。
2. 请求中的目标 UID 必须单独校验。
3. 涉及私聊时，双方必须具备双向好友关系。

未登录 Session：

```cpp
sess->GetUserId() <= 0
```

不得执行用户搜索、好友申请、好友认证或文本聊天。

## 4. 修改前后的信任边界

| 操作 | 改造前操作者来源 | 改造后操作者来源 |
| --- | --- | --- |
| 用户搜索 | 无显式身份校验 | 已登录 Session |
| 好友申请 | 请求体 `uid` | Session UID |
| 好友认证 | 请求体 `from_uid` | Session UID |
| 文本消息发送者 | 请求体 `from_uid` | Session UID |
| 文本消息接收者 | 请求体 `to_uid` | 请求体 `to_uid`，额外校验 |
| 跨服申请通知 | 请求体申请 UID | Session UID |
| 跨服认证通知 | 请求体认证 UID | Session UID |

## 5. 登录流程

初次 TCP 登录仍需要客户端提供：

```json
{
  "uid": 1001,
  "token": "..."
}
```

这是因为连接刚建立时，服务端还不知道用户是谁。

服务端使用 UID 查询 Redis Token 并校验。成功后：

```cpp
sess->SetUserId(uid);
UserMgr::GetInst()->SetUserSession(uid, sess);
```

同一个 Session 再次发送登录请求会被拒绝，避免重复增加登录计数或替换自身绑定。

## 6. 用户搜索

搜索请求仍可携带用户名或 UID 作为查询内容：

```json
{
  "uid": "alice"
}
```

但执行搜索前必须确认当前连接已认证：

```cpp
if (sess->GetUserId() <= 0) {
    root["error"] = ErrorCodes::Token_Invalid;
    return;
}
```

返回资料不再包含密码或密码哈希。

## 7. 好友申请

### 7.1 新协议

客户端只发送：

```json
{
  "applyname": "验证信息",
  "backname": "备注",
  "to_uid": 1002
}
```

不再发送申请人 UID。

### 7.2 服务端流程

```text
读取 Session UID
  → 校验 Session 已登录
  → 校验 to_uid 是正整数
  → 禁止添加自己
  → 查询目标用户是否存在
  → 检查是否已经是好友
  → 写入 friend_apply
  → 设置成功响应
  → 查询目标在线节点
  → 同节点直接通知 / 跨节点 gRPC 转发
```

申请人身份：

```cpp
int uid = sess->GetUserId();
```

目标身份：

```cpp
int to_uid = src_root["to_uid"].get<int>();
```

数据库操作失败时返回 `Database_Failed`，不会先把响应设置为成功。

## 8. 好友认证

### 8.1 新协议

客户端只发送申请人和备注：

```json
{
  "to_uid": 1001,
  "back": "好友备注"
}
```

`to_uid` 表示原申请人。当前同意者由 Session 决定。

### 8.2 服务端流程

```text
读取 Session UID 作为 accepter_uid
  → 读取 to_uid 作为 applicant_uid
  → 校验两个 UID 均为正且不相同
  → 执行 AuthAndAddFriend(applicant_uid, accepter_uid, back)
  → 事务更新申请状态
  → 事务插入双方 friend 关系
  → 成功后返回并通知申请人
```

数据库事务中存在条件：

```sql
UPDATE friend_apply
SET status = 1
WHERE from_uid = ?
  AND to_uid = ?
```

其中 `to_uid` 必须是当前 Session 用户。因此不能通过修改请求体“替别人同意”申请。

## 9. 文本聊天身份与好友校验

### 9.1 新协议

客户端不再发送发送者 UID：

```json
{
  "to_uid": 1002,
  "text_array": [
    {
      "unique_id": "...",
      "content": "hello"
    }
  ]
}
```

### 9.2 发送者来源

```cpp
int uid = sess->GetUserId();
```

服务端响应中的 `from_uid` 也使用这个值。

为了兼容短暂存在的旧客户端，如果请求中仍然带有 `from_uid`，服务端会检查：

- 该字段必须是整数。
- 该字段必须等于 Session UID。

不相等时返回 `Token_Invalid`。

### 9.3 目标校验

```cpp
int to_uid = src_root["to_uid"].get<int>();
```

要求：

- `to_uid > 0`。
- `to_uid != uid`。
- 目标用户存在。
- 双方是好友。

### 9.4 AreFriends

新增接口：

```cpp
bool MysqlDao::AreFriends(int uid, int friend_id);
```

当前好友表是双向存储：

```text
(A, B)
(B, A)
```

查询：

```sql
SELECT COUNT(*)
FROM friend
WHERE (self_id = ? AND friend_id = ?)
   OR (self_id = ? AND friend_id = ?);
```

只有计数为 2 才返回好友成立。

这样不仅防止陌生用户直接私聊，也能识别只有单边记录的异常好友关系。

### 9.5 错误码

新增：

```text
Database_Failed = 1014
Friend_Required = 1015
```

- `Database_Failed`：好友申请、认证等持久化操作失败。
- `Friend_Required`：发送者与目标不是双向好友。

## 10. 跨服务器转发

同服务器路径：

```text
1017 请求
  → Session 校验
  → AreFriends
  → 找到目标 Session
  → 1019 通知
```

跨服务器路径：

```text
1017 请求
  → Session 校验
  → AreFriends
  → 查 Redis 目标节点
  → gRPC TextChatMsgReq
  → 目标 ChatServer 1019 通知
```

gRPC 请求中的 `from_uid` 和 `to_uid` 全部来自服务端已经验证过的值。

当前内部 gRPC 仍是明文且无服务间认证，因此它只能作为受控内网边界使用。mTLS 和服务令牌属于后续 P1。

## 11. 客户端修改

修改文件：

```text
client/applyfriend.cpp
client/authenfriend.cpp
client/chatpage.cpp
client/tcpmgr.cpp
```

具体变化：

- 好友申请移除 `jsonObj["uid"]`。
- 好友认证移除 `jsonObj["from_uid"]`。
- 文本消息移除 `textObj["from_uid"]`。
- 修复认证通知中把 `nick` 误当 `icon` 使用的旧问题。

客户端仍然决定目标用户，但不再决定当前操作者。

## 12. 边界情况

| 场景 | 结果 |
| --- | --- |
| 未登录连接搜索用户 | `Token_Invalid` |
| 未登录连接申请好友 | `Token_Invalid` |
| 未登录连接认证好友 | `Token_Invalid` |
| 未登录连接发送文本 | `Token_Invalid` |
| 添加自己为好友 | `Uid_Invalid` |
| 目标用户不存在 | `Uid_Invalid` |
| 已经是好友 | `User_Exist` |
| 无申请记录却认证 | `Database_Failed` |
| 给陌生人发消息 | `Friend_Required` |
| 请求中伪造其他 `from_uid` | `Token_Invalid` |
| 好友关系只有单边记录 | `Friend_Required` |

## 13. 测试清单

### 13.1 身份伪造

1. A 登录后发送声称 `from_uid=B` 的文本请求。
2. 预期返回 `1018`，错误码 `Token_Invalid`。
3. B 不应收到任何 `1019` 通知。

### 13.2 陌生人聊天

1. A、B 都登录，但还没有好友关系。
2. A 发送只包含 `to_uid=B` 的文本。
3. 预期返回 `Friend_Required`。

### 13.3 好友申请与认证

1. A 申请 B。
2. 检查 `friend_apply` 中 `from_uid=A, to_uid=B`。
3. B 使用自己的 Session 同意 A。
4. 检查 `friend` 中存在 `(A,B)` 和 `(B,A)`。

### 13.4 未登录访问

建立 TCP 连接但不发送聊天登录请求，直接发送搜索、好友或文本消息，全部应被拒绝。

### 13.5 正常聊天

A、B 成为好友后，A 发送文本，B 应收到 `1019`，且通知中的 `from_uid` 等于服务端 Session UID。

上述场景已经整理为可重复执行的脚本：

```powershell
node server\tests\p0_security_smoke.js
```

测试需要 StatusServer、GateServer 和两个 ChatServer 已启动。本次实测结果：

```text
P0 security smoke test passed
```

## 14. 性能与后续优化

当前每条文本消息都会查询一次 `AreFriends`。

对于第一版这是可接受的安全优先方案。后续可考虑：

- 好友关系写入后缓存到 Redis。
- 缓存 key 使用 `friend_pair:minUid:maxUid`。
- 好友变更时主动失效。
- 缓存未命中再查 MySQL。

不能在好友关系只写单边时认定好友，否则历史脏数据可能造成越权聊天。

## 15. 当前边界

已解决：

- 请求体伪造当前操作者。
- 未登录访问好友与聊天接口。
- 非好友直接私聊。
- 好友申请、认证和聊天的服务端身份来源统一。

尚未解决：

- HTTP 和 TCP 明文传输。
- 内部 gRPC mTLS。
- Token TTL 和踢人策略。
- 好友关系缓存。
- 用户级消息发送速率限制。
- 重复登录与 Session 映射竞争问题。

这些应按 P1 逐项继续迭代。
