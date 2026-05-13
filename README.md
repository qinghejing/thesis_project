# llfcchat

llfcchat 是一个基于 Windows 环境的 C++/Qt 分布式聊天系统实践项目。项目由 Qt 桌面客户端和多个后端服务组成，覆盖注册、登录、邮箱验证码、好友申请、联系人管理、聊天会话、服务状态分配和跨聊天服务消息转发等功能。

## 技术栈

- 客户端：Qt Widgets、C++11、HTTP、TCP
- C++ 服务端：Boost.Asio、Boost.Beast、gRPC、protobuf、MySQL Connector/C++、Redis
- 验证服务：Node.js、@grpc/grpc-js、nodemailer、ioredis
- 数据存储：MySQL、Redis

## 项目结构

- `client/llfcchat`：Qt 桌面客户端，负责登录、注册、重置密码、好友、联系人和聊天界面。
- `server/GateServer`：HTTP 网关服务，处理客户端注册、登录、验证码等接口请求。
- `server/VarifyServer`：Node.js gRPC 验证码服务，负责生成邮箱验证码并写入 Redis。
- `server/StatusServer`：gRPC 状态服务，负责为登录用户分配聊天服务器并签发 token。
- `server/ChatServer`、`server/ChatServer2`：两个聊天服务实例，负责 TCP 长连接、好友关系、在线通知和消息转发。
- `sql备份/llfc.sql`：MySQL 表结构、样例数据和相关存储过程。
- `start-services.ps1`、`stop-services.ps1`：本地启动和停止服务的辅助脚本。

## 运行说明

项目依赖本地 MySQL、Redis、Qt、Visual Studio、gRPC/protobuf 以及 Node.js 环境。首次运行前需要导入 `sql备份/llfc.sql`，配置各服务的本地连接信息，并在 `server/VarifyServer` 下执行 `npm install` 安装 Node 依赖。

本仓库不会提交本地配置、运行日志、编译产物、Node 依赖目录和开发文档目录。相关配置文件请按本机环境自行维护。
