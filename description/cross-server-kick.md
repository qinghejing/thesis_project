# 跨服踢人功能说明

## 功能范围

本功能用于保证同一个用户账号在分布式聊天服务中只保留一个有效在线连接。

当前项目有两个聊天服务实例：

- `ChatServer`：实例名 `chatserver1`，TCP 端口 `8090`，gRPC 端口 `50055`。
- `ChatServer2`：实例名 `chatserver2`，TCP 端口 `8091`，gRPC 端口 `50056`。

当同一个 `uid` 再次登录时，新连接会成为有效连接，旧连接会被踢下线。旧连接可能在同一个聊天服，也可能在另一个聊天服：

- 同服登录：当前聊天服直接找到旧 `CSession`，发送下线通知并关闭旧连接。
- 跨服登录：当前聊天服通过 gRPC 调用旧聊天服的 `NotifyKickUser`，由旧聊天服关闭旧连接。

客户端收到下线通知或登录后被远端断开时，会提示账号已在其他设备登录，并切回登录页。

## 核心设计

### Redis 状态

功能依赖 Redis 记录用户当前所在聊天服和各聊天服负载：

```text
utoken_<uid>      登录 token，由 StatusServer/GateServer 登录链路写入
uip_<uid>         当前 uid 所属聊天服实例名，例如 chatserver1 或 chatserver2
logincount        Redis hash，记录 chatserver1/chatserver2 当前登录计数
lock_<uid>        用户登录分布式锁，避免同一个 uid 并发登录造成状态覆盖
```

### 登录锁

聊天服处理 `ID_CHAT_LOGIN = 1005` 时，会先校验 token，再尝试获取 `lock_<uid>`：

```text
LOCK_PREFIX = lock_
LOCK_TIME_OUT = 10
ACQUIRE_TIME_OUT = 3
```

获取锁后才会读取和更新 `uip_<uid>`，这样可以避免两个客户端同时登录同一个账号时出现双写竞态。

### 踢人消息

新增客户端 TCP 消息：

```text
ID_NOTIFY_OFF_LINE_REQ = 1022
```

服务端发送 1022 后会关闭旧连接。考虑到 TCP 关闭太快时客户端不一定能先读到 1022，客户端还额外处理了“登录成功后 socket 被远端断开”的情况，作为下线兜底提示。

### 跨服 gRPC

`message.proto` 新增：

```text
KickUserReq
KickUserRsp
rpc NotifyKickUser(KickUserReq) returns (KickUserRsp)
```

当 `uip_<uid>` 指向其他聊天服时，新登录所在聊天服会通过 `ChatGrpcClient::NotifyKickUser` 通知旧聊天服踢掉旧连接。

## 涉及模块

服务端：

- `server/ChatServer/LogicSystem.*`
- `server/ChatServer2/LogicSystem.*`
- `server/ChatServer/CSession.*`
- `server/ChatServer2/CSession.*`
- `server/ChatServer/CServer.cpp`
- `server/ChatServer2/CServer.cpp`
- `server/ChatServer/RedisMgr.*`
- `server/ChatServer2/RedisMgr.*`
- `server/ChatServer/ChatGrpcClient.*`
- `server/ChatServer2/ChatGrpcClient.*`
- `server/ChatServer/ChatServiceImpl.*`
- `server/ChatServer2/ChatServiceImpl.*`
- `server/ChatServer/message.proto`
- `server/ChatServer2/message.proto`

客户端：

- `client/llfcchat/global.h`
- `client/llfcchat/tcpmgr.*`
- `client/llfcchat/mainwindow.*`

## 编译方式

### ChatServer

在项目根目录执行：

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' .\server\ChatServer\ChatServer.sln /p:Configuration=Debug /p:Platform=x64 /m
```

产物：

```text
server/ChatServer/x64/Debug/ChatServer.exe
```

### ChatServer2

在项目根目录执行：

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' .\server\ChatServer2\ChatServer.sln /p:Configuration=Debug /p:Platform=x64 /m
```

产物：

```text
server/ChatServer2/x64/Debug/ChatServer.exe
```

### 客户端

编译客户端前，必须先关闭所有正在运行的 `chat.exe` 窗口，否则链接阶段会报：

```text
cannot open output file bin\chat.exe: Permission denied
```

在 `client/llfcchat` 目录执行：

```powershell
$env:Path = 'D:\donwload\Qt\Qt5.12.2\Tools\mingw730_64\bin;D:\donwload\Qt\Qt5.12.2\5.12.2\mingw73_64\bin;' + $env:Path
& 'D:\donwload\Qt\Qt5.12.2\Tools\mingw730_64\bin\mingw32-make.exe' -f Makefile.Debug
```

产物：

```text
client/llfcchat/bin/chat.exe
```

## 测试前准备

1. 确认 Redis 和 MySQL 已启动。
2. 确认 `StatusServer` 配置中包含两个聊天服：

```text
[chatservers]
Name = chatserver1,chatserver2
```

3. 同时启动以下服务：

```text
VarifyServer
StatusServer
ChatServer
ChatServer2
GateServer
```

可以使用项目根目录脚本启动：

```powershell
.\start-services.ps1
```

4. 确认两个聊天服都在运行：

```powershell
Get-Process -Name ChatServer -ErrorAction SilentlyContinue | Select-Object Id,Path,StartTime
```

正常情况下应该能看到两个进程路径：

```text
server/ChatServer/x64/Debug/ChatServer.exe
server/ChatServer2/x64/Debug/ChatServer.exe
```

如果只看到一个进程，只能测到单服踢人或无法完成跨服踢人。

## 手工测试步骤

### 1. 启动第一个客户端

运行：

```text
client/llfcchat/bin/chat.exe
```

使用一个已有账号登录，例如 `uid = 1012` 的账号。

登录成功后，在 Redis 中查看该用户当前所在聊天服：

```powershell
redis-cli -h 127.0.0.1 -p 6380 -a 123456 GET uip_1012
```

预期返回类似：

```text
chatserver1
```

也可能返回 `chatserver2`，以实际返回为准。

### 2. 启动第二个客户端

不要退出第一个客户端，再打开第二个客户端，并使用同一个账号登录。

登录成功后再次查看：

```powershell
redis-cli -h 127.0.0.1 -p 6380 -a 123456 GET uip_1012
```

预期结果：

- 第二个客户端登录成功。
- Redis 中 `uip_1012` 指向第二次登录所在的聊天服。
- 第一个客户端弹出提示：

```text
账号已在其他设备登录，当前连接已断开。
```

- 第一个客户端回到登录页。

### 3. 判断是否真的跨服

查看两个聊天服日志，或者查看 Redis：

```powershell
redis-cli -h 127.0.0.1 -p 6380 -a 123456 HGETALL logincount
```

如果第一次登录时 `uip_1012 = chatserver1`，第二次登录后变成 `chatserver2`，就是跨服踢人。

如果两次都在同一个聊天服，也应该能踢掉旧客户端，但这属于同服踢人，不是跨服场景。此时需要确认：

- `ChatServer` 和 `ChatServer2` 是否都已启动。
- `StatusServer/config.ini` 是否配置了两个聊天服。
- 两个聊天服启动日志中是否都执行了 `HSet logincount <server> 0`。
- 第二次登录前两个聊天服的 `logincount` 是否明显不均衡。

## 预期日志现象

第一次登录时，当前聊天服可能看到：

```text
user login uid is 1012
[ GET uip_1012 ] failed
Execut command [ SET uip_1012 chatserver1 ] success
```

第二次登录时，新聊天服可能看到：

```text
user login uid is 1012
Succeed to execute command [ GET uip_1012 ]
Execut command [ SET uip_1012 chatserver2 ] success
```

旧聊天服被踢后可能看到连接关闭日志：

```text
handle read failed
~CSession destruct
```

这些日志说明旧连接已经被服务端清理。注意：旧窗口还停在桌面上不等于旧连接还在线，最终以 Redis、服务端日志和客户端弹窗为准。

## 常见问题

### 1. 为什么屏幕上仍然看到两个客户端窗口？

窗口存在不代表连接还在线。旧客户端可能已经被服务端关闭 socket，但 UI 如果没有处理 1022 或断线事件，就会停留在聊天页。

当前客户端已经做了两个处理：

- 收到 `ID_NOTIFY_OFF_LINE_REQ = 1022` 时弹窗并切回登录页。
- 登录成功后如果被远端断开，也弹窗并切回登录页。

如果仍然没有弹窗，优先确认你运行的是重新编译后的 `client/llfcchat/bin/chat.exe`，不是旧进程或旧产物。

### 2. 编译客户端时 Permission denied 怎么办？

这是因为 `chat.exe` 正在运行，Windows 不允许覆盖正在运行的 exe。

处理方式：

1. 关闭所有客户端窗口。
2. 确认没有残留进程：

```powershell
Get-Process -Name chat -ErrorAction SilentlyContinue
```

3. 再重新执行客户端编译命令。

### 3. 为什么只测到同服踢人？

通常是因为只有一个聊天服在运行，或者 StatusServer 当前仍然把两次登录分配到了同一个聊天服。

先检查：

```powershell
Get-Process -Name ChatServer -ErrorAction SilentlyContinue | Select-Object Id,Path
redis-cli -h 127.0.0.1 -p 6380 -a 123456 HGETALL logincount
```

确认两个聊天服都运行后，再重新登录测试。

### 4. Visual Studio 里有 E0135 等红线怎么办？

`E0135`、`E0070`、`E3365`、`E0304` 多数是 IntelliSense 缓存问题，不一定是真实编译错误。

判断标准：

- 真实编译错误一般是 `Cxxxx` 或链接错误。
- 在错误列表里切换为“仅生成/Build Only”。
- 以 MSBuild 输出是否 `0 errors` 为准。

## 验收标准

功能通过时应满足：

- 两个聊天服都能正常启动。
- 同一个账号第一次登录成功。
- 同一个账号第二次登录成功。
- 旧客户端被提示下线并回到登录页。
- Redis 中 `uip_<uid>` 指向第二次登录所在聊天服。
- 旧聊天服日志显示旧连接被关闭。
- 新客户端可以继续正常聊天或执行搜索、添加好友等 TCP 操作。

