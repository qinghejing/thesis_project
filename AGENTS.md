# 项目解读：llfcchat

## 项目概览

这是一个 Windows 下的 C++/Qt 聊天系统实战项目，目录名和文档对应 `llfcchat`。项目不是单体应用，而是由桌面客户端和多个后端服务组成：

- `client/llfcchat`：Qt 5 桌面客户端，负责登录、注册、重置密码、联系人、好友申请和聊天 UI。
- `server/GateServer`：C++ HTTP 网关服务，使用 Boost.Beast/Asio，对客户端提供注册、登录、验证码等 HTTP API。
- `server/VarifyServer`：Node.js gRPC 验证码服务，负责生成邮箱验证码，并将验证码写入 Redis。
- `server/StatusServer`：C++ gRPC 状态服务，负责给登录用户分配聊天服务器并签发 token。
- `server/ChatServer` 和 `server/ChatServer2`：两个 C++ 聊天服务实例，负责 TCP 长连接、好友关系、在线通知和文本消息转发。
- `sql备份/llfc.sql`：MySQL 表结构、样例数据和注册存储过程。
- `开发文档/day*.md`：按课程天数拆分的开发笔记，覆盖客户端、网关、Redis、MySQL、gRPC、好友和聊天功能。

整体技术栈包括 Qt Widgets、Boost.Asio、Boost.Beast、gRPC/protobuf、jsoncpp、MySQL Connector/C++、hiredis、Redis、Node.js、nodemailer 和 ioredis。

## 运行时链路

典型登录和聊天流程如下：

1. Qt 客户端启动后读取 `client/llfcchat/config.ini`，默认连接 `GateServer` 的 `localhost:8080`。
2. 注册或重置密码时，客户端请求 `GateServer` 的 `/get_varifycode`，网关再通过 gRPC 调用 `VarifyServer:50051`。
3. `VarifyServer` 生成验证码，写入 Redis 的 `code_` 前缀 key，并尝试发邮件；发信失败时会在控制台打印开发用验证码。
4. 登录时，客户端请求 `GateServer` 的 `/user_login`，网关校验 MySQL 用户密码后调用 `StatusServer:50052`。
5. `StatusServer` 根据 Redis 中各聊天服务器的登录计数选择负载较低的聊天服务器，返回 host、port 和 token。
6. 客户端再建立 TCP 长连接到 `ChatServer` 或 `ChatServer2`，发送 `ID_CHAT_LOGIN` 和 token。
7. 聊天服务器从 Redis 校验 token，加载用户信息、好友列表和好友申请，然后处理搜索、添加好友、认证好友和聊天消息。
8. 当目标用户在另一个聊天服务实例上时，两个 `ChatServer` 之间通过 `ChatService` gRPC 互相通知。

## 服务和端口

| 服务 | 默认端口 | 协议/用途 |
| --- | --- | --- |
| Redis | `6380` | 验证码、token、用户缓存、登录计数 |
| MySQL | `3306` | 用户、好友、好友申请数据 |
| GateServer | `8080` | HTTP API |
| VarifyServer | `50051` | gRPC 验证码服务 |
| StatusServer | `50052` | gRPC 状态/分配聊天服务器 |
| ChatServer | `8090` | TCP 聊天长连接 |
| ChatServer | `50055` | gRPC 跨聊天服通知 |
| ChatServer2 | `8091` | TCP 聊天长连接 |
| ChatServer2 | `50056` | gRPC 跨聊天服通知 |

## 关键目录

`client/llfcchat`

- `llfcchat.pro` 是 Qt/qmake 项目文件，启用 `core gui network widgets` 和 C++11，输出目录是 `bin`。
- `main.cpp` 初始化配置并设置全局网关地址。
- `httpmgr.*` 封装客户端 HTTP 请求，处理注册、重置、登录等短连接请求。
- `tcpmgr.*` 封装 TCP 长连接和聊天协议收发。
- `global.h` 定义客户端请求 ID、错误码、模块枚举和全局数据结构。
- `logindialog.*`、`registerdialog.*`、`resetdialog.*` 是登录、注册、重置密码界面。
- `chatdialog.*`、`chatpage.*`、`ChatView.*`、`*Bubble.*`、`contactuserlist.*`、`applyfriend*` 等是聊天、联系人、好友申请界面。
- `style/stylesheet.qss` 是主要 QSS 样式文件，`res/` 和 `static/` 存放图标、头像和资源。

`server/GateServer`

- `GateServer.cpp` 是 HTTP 网关入口，启动 Boost.Asio/Beast 服务。
- `LogicSystem.cpp` 注册 HTTP 路由：`/get_test`、`/test_procedure`、`/get_varifycode`、`/user_register`、`/reset_pwd`、`/user_login`。
- `VerifyGrpcClient.*` 调用验证码服务。
- `StatusGrpcClient.*` 调用状态服务。
- `MysqlMgr/MysqlDao` 和 `RedisMgr` 负责访问 MySQL/Redis。
- `config.ini` 配置 HTTP 端口、gRPC 目标、MySQL 和 Redis。

`server/VarifyServer`

- `server.js` 是 Node.js gRPC 入口，监听 `0.0.0.0:50051`。
- `proto.js` 加载 `message.proto`。
- `redis.js` 封装验证码 Redis 操作。
- `email.js` 使用 nodemailer 发验证码。
- `package.json` 依赖 `@grpc/grpc-js`、`@grpc/proto-loader`、`ioredis`、`nodemailer`、`uuid`。

`server/StatusServer`

- `StatusServer.cpp` 启动 gRPC 服务。
- `StatusServiceImpl.cpp` 实现 `GetChatServer` 和 `Login`。
- `GetChatServer` 会读取配置里的 chat server 列表，并根据 Redis 的 `logincount` 哈希选择连接数较低的服务。

`server/ChatServer` 与 `server/ChatServer2`

- 两个目录基本是同一套代码，靠 `config.ini` 区分实例名、TCP 端口、gRPC 端口和对端服务。
- `ChatServer.cpp` 同时启动 TCP 服务和本实例的 gRPC 服务。
- `CServer/CSession/MsgNode` 负责 TCP 连接、消息包和收发队列。
- `LogicSystem.cpp` 注册 TCP 消息处理：登录、搜索用户、添加好友、认证好友、文本聊天。
- `ChatGrpcClient.*` 调用其他聊天服；`ChatServiceImpl.*` 接收其他聊天服发来的通知。
- `const.h` 定义 TCP 协议头长度、消息 ID、Redis key 前缀等。

## 协议和数据

- HTTP 网关使用 JSON 请求/响应，主要集中在 `server/GateServer/LogicSystem.cpp`。
- 客户端和聊天服之间的 TCP 包头总长 4 字节：2 字节消息 ID + 2 字节数据长度，定义在聊天服 `const.h`。
- 客户端消息 ID 在 `client/llfcchat/global.h`，服务端消息 ID 在 `server/ChatServer/const.h`，修改时必须保持同步。
- gRPC 接口定义在各服务目录的 `message.proto`，C++ 目录里同时提交了生成后的 `message.pb.*` 和 `message.grpc.pb.*`。
- MySQL schema 名为 `llfc`，核心表有 `user`、`friend`、`friend_apply`、`user_id`。

## 构建与启动

这个仓库偏 Windows/Visual Studio/QtCreator 环境：

- C++ 服务目录都有 `.sln`、`.vcxproj` 和 `PropertySheet.props`。
- `PropertySheet.props` 中写死了本机依赖路径，例如 Boost、jsoncpp、MySQL Connector、hiredis/vcpkg、gRPC、protobuf、OpenSSL、zlib、abseil、re2；换机器时需要改这些路径。
- Qt 客户端用 `client/llfcchat/llfcchat.pro` 构建，Debug/Release 后会把 `config.ini` 和 `static/` 复制到输出目录。
- Node 验证码服务在 `server/VarifyServer` 下运行；如果没有 `node_modules`，先执行 `npm install`，再执行 `npm run serve` 或 `node server.js`。
- MySQL 需要先导入 `sql备份/llfc.sql`，并确保各 `config.ini` 指向同一个 `llfc` schema。
- Redis 默认使用 `127.0.0.1:6380`，且需要配置密码；具体值在各服务配置文件中。
- 根目录的 `start-services.ps1` 会依次启动 Redis、VarifyServer、StatusServer、ChatServer1、ChatServer2、GateServer，并把日志写到 `run-logs/`。
- `start-services.ps1` 依赖本机硬编码的 Redis 和 Node 路径，也假定 C++ 服务已经编译出 `x64/Debug/*.exe`；换环境时先修改脚本路径。
- 根目录的 `stop-services.ps1` 会停止 `GateServer`、`StatusServer`、`ChatServer`、`node`、`redis-server` 进程。

## 修改功能时的落点

- 增加 HTTP API：改 `server/GateServer/LogicSystem.cpp` 注册路由，再在客户端 `HttpMgr` 和对应 dialog/page 中接入。
- 增加 TCP 消息：同步修改客户端 `global.h`、聊天服 `const.h`、客户端 `TcpMgr::initHandlers`、聊天服 `LogicSystem::RegisterCallBacks`。
- 增加跨聊天服通知：修改 `message.proto`、重新生成 C++ gRPC 文件，并更新 `ChatGrpcClient` 与 `ChatServiceImpl`。
- 修改用户/好友数据：优先看各服务的 `MysqlDao.*`、`MysqlMgr.*`，并同步更新 `sql备份/llfc.sql`。
- 修改 UI：优先改 `.ui`、对应 `.cpp/.h` 和 `style/stylesheet.qss`；不要直接改生成的 `ui_*.h`。
- 修改配置：优先改源目录下的 `config.ini/config.json`，注意构建脚本会把它们复制到输出目录。

## 注意事项

- 不要把 `bin/`、`debug/`、`x64/`、`node_modules/`、`run-logs/`、`ui_*.h`、`moc_*.cpp`、`.obj/.o/.pdb/.ilk` 当作主要源码修改。
- `.gitignore` 已经忽略了一部分构建产物，但当前工作区仍能看到一些历史产物；做变更时尽量只碰源代码、配置、文档和 schema。
- 配置文件和 SQL 里包含开发用账号、密码、邮箱授权码和样例用户数据；写文档或提交时不要扩散真实密钥，必要时改成本地私有配置。
- README 在当前终端显示有编码乱码，项目的中文说明优先看 `开发文档/` 下的 Markdown。
- `ChatServer` 和 `ChatServer2` 是分布式聊天示例的两个实例，改一边代码时通常要同步另一边，除非只是改实例配置。
