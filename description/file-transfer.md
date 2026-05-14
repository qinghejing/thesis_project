# 聊天文件传输功能说明

## 功能范围

本功能基于 llfcchat 现有 TCP 长连接聊天协议实现文件上传链路：

- 客户端在聊天页点击文件按钮选择本地文件。
- 客户端计算文件 MD5，并按固定大小拆成多个分片。
- 每个分片通过 TCP 长连接发送到当前分配的 ChatServer。
- ChatServer 接收分片后异步写入本地 `file_uploads` 目录。
- 服务端保存文件名格式为 `<md5>_<原文件名>`，便于校验和避免同名覆盖。

当前实现的是第一阶段“发送端上传到聊天服务器并落盘”。接收方文件通知、点击下载、断点续传、秒传等能力还没有在这一版展开。

## 协议说明

为了支持文件分片 JSON 体超过 64KB，本次把客户端和 ChatServer 的 TCP 包头从旧的 4 字节升级为 6 字节：

```text
2 字节消息 ID + 4 字节包体长度 + JSON 包体
```

新增消息 ID：

```text
ID_UPLOAD_FILE_REQ = 1020
ID_UPLOAD_FILE_RSP = 1021
```

文件分片请求 JSON 主要字段：

```json
{
  "fromuid": 1,
  "touid": 2,
  "md5": "文件MD5",
  "name": "原文件名",
  "seq": 1,
  "total_size": "文件总大小",
  "trans_size": "当前已传输大小",
  "last": 0,
  "data": "base64后的分片内容"
}
```

说明：

- `seq` 从 1 开始递增。
- `last` 为 1 表示最后一个分片。
- `total_size` 和 `trans_size` 使用字符串承载，避免大文件大小在 JSON 整型上出现兼容问题。
- 客户端当前限制发送文件大小小于 4GB。
- 单个文件分片大小由客户端 `MAX_FILE_CHUNK_LEN` 控制，当前为 2KB。

## 涉及模块

客户端：

- `client/llfcchat/global.h`：新增文件传输消息 ID、6 字节包头常量、文件大小/分片常量。
- `client/llfcchat/tcpmgr.*`：TCP 收发协议改为读取 6 字节包头。
- `client/llfcchat/chatpage.*`：接入文件按钮选择、文件 MD5、分片发送。
- `client/llfcchat/MessageTextEdit.cpp`：文件大小限制调整为 4GB。

服务端：

- `server/ChatServer/const.h` 和 `server/ChatServer2/const.h`：同步协议常量。
- `server/ChatServer/CSession.*` 和 `server/ChatServer2/CSession.*`：TCP 包头长度解析改为 2+4。
- `server/ChatServer/MsgNode.*` 和 `server/ChatServer2/MsgNode.*`：发送包体长度改为 4 字节。
- `server/ChatServer/LogicSystem.*` 和 `server/ChatServer2/LogicSystem.*`：注册并处理文件上传消息。
- `base64.*`、`FileWorker.*`、`FileSystem.*`：分片解码和异步落盘。

## 编译方式

### 客户端

在 `client/llfcchat` 目录执行：

```powershell
& 'D:\mingw64\bin\mingw32-make.exe' -f Makefile.Debug
```

编译成功后客户端产物：

```text
client/llfcchat/bin/chat.exe
```

### ChatServer

在项目根目录执行：

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' .\server\ChatServer\ChatServer.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

产物：

```text
server/ChatServer/x64/Debug/ChatServer.exe
```

### ChatServer2

在项目根目录执行：

```powershell
& 'C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\amd64\MSBuild.exe' .\server\ChatServer2\ChatServer.sln /p:Configuration=Debug /p:Platform=x64 /m /v:minimal
```

产物：

```text
server/ChatServer2/x64/Debug/ChatServer.exe
```

注意：两个 ChatServer 项目已显式关闭 vcpkg 自动集成，避免本机 vcpkg 的另一套 gRPC/upb 静态库被自动注入，导致重复符号链接错误。

## 测试步骤

### 1. 运行协议烟测

在项目根目录执行：

```powershell
& 'C:\Users\sadliu\.cache\codex-runtimes\codex-primary-runtime\dependencies\python\python.exe' .\tests\chat_file_transfer\protocol_smoke.py
```

期望输出：

```text
PASS test_header_can_carry_large_json_body
PASS test_base64_chunks_reassemble_binary_payload
PASS test_source_protocol_constants_match
```

这一步验证 6 字节包头、大 JSON 包体、base64 分片重组和源码协议常量是否一致。

### 2. 重启服务

由于 TCP 包头已经从 4 字节改为 6 字节，客户端和 ChatServer 必须同时使用新版本。测试前建议重启服务：

```powershell
.\stop-services.ps1
.\start-services.ps1
```

`start-services.ps1` 结束时应看到这些端口处于 `LISTEN`：

```text
6380
50051
50052
50055
50056
8090
8091
8080
```

其中：

- `8090` 是 ChatServer 的 TCP 聊天端口。
- `8091` 是 ChatServer2 的 TCP 聊天端口。
- `8080` 是 GateServer 的 HTTP 网关端口。

### 3. 启动客户端并发送文件

启动客户端：

```powershell
.\client\llfcchat\bin\chat.exe
```

测试流程：

1. 登录一个已有账号。
2. 进入某个好友聊天页面。
3. 点击聊天输入区附近的文件按钮。
4. 选择一个小的 `.txt` 文件。
5. 点击发送。
6. 再测试一个超过 2KB 的文件，确认分片链路也能工作。
7. 可以继续测试图片、中文文件名文件、空文件。

### 4. 查看服务端保存结果

文件会保存在当前用户被分配到的聊天服务运行目录下：

```powershell
Get-ChildItem .\server\ChatServer\x64\Debug\file_uploads
Get-ChildItem .\server\ChatServer2\x64\Debug\file_uploads
```

因为登录时 StatusServer 会给用户分配 ChatServer 或 ChatServer2，所以文件可能出现在其中任意一个目录。

保存后的文件名类似：

```text
e10adc3949ba59abbe56e057f20f883e_test.txt
```

### 5. 查看日志

可以在服务日志中查找保存成功记录：

```powershell
Select-String -Path .\run-logs\ChatServer*.out.log -Pattern "file upload saved"
```

如果看到类似输出，说明服务端已完成落盘：

```text
file upload saved: ...\file_uploads\<md5>_<原文件名>
```

### 6. 校验文件内容

用 MD5 对比原文件和服务端保存文件：

```powershell
Get-FileHash .\你的原文件.txt -Algorithm MD5
Get-FileHash .\server\ChatServer\x64\Debug\file_uploads\<保存后的文件名> -Algorithm MD5
```

如果两个 Hash 一致，说明文件内容在上传、分片、base64 解码和写入过程中没有损坏。

## 推荐测试用例

建议按下面顺序测试：

1. 小文本文件，例如 `hello.txt`。
2. 大于 2KB 的文本或二进制文件，用于验证多分片。
3. 图片文件，例如 `.png` 或 `.jpg`。
4. 中文文件名，例如 `测试文件.txt`。
5. 空文件，用于验证边界场景。
6. 较大文件，例如几十 MB，用于观察稳定性和日志输出。

## 常见问题

### 客户端发送后服务端没有文件

检查点：

- 是否重启过 ChatServer 和 ChatServer2。
- 客户端是否是新编译的 `client/llfcchat/bin/chat.exe`。
- `start-services.ps1` 输出里 `8090` 或 `8091` 是否处于 `LISTEN`。
- 文件是否保存到了另一个聊天服务目录。
- `run-logs/ChatServer*.err.log` 是否有异常输出。

### 连接后聊天异常或收不到响应

这通常是客户端和服务端协议版本不一致导致的。旧版本使用 4 字节包头，新版本使用 6 字节包头。测试时必须同时使用新编译的客户端和新编译的 ChatServer。

### MSBuild 出现 gRPC/upb 重复符号

如果看到 `LNK2005`，并且错误里同时出现 `grpc.lib`、`upb.lib`、`upb_message_lib.lib`，通常是 vcpkg 自动注入了另一套 gRPC 库。当前项目文件已经设置：

```xml
<VcpkgEnabled>false</VcpkgEnabled>
```

如果在其他工程配置中复现这个问题，也需要避免同时链接两套 gRPC/upb 静态库。

### 文件保存在哪里

服务由 `start-services.ps1` 启动时，工作目录分别是：

```text
server/ChatServer/x64/Debug
server/ChatServer2/x64/Debug
```

因此上传目录分别是：

```text
server/ChatServer/x64/Debug/file_uploads
server/ChatServer2/x64/Debug/file_uploads
```

## 当前限制

- 当前只实现上传和服务端落盘。
- 接收方暂时不会收到文件消息通知。
- 聊天记录里暂时没有保存文件元数据到 MySQL。
- 暂时没有下载接口。
- 暂时没有断点续传、暂停、取消、进度条。
- 服务端按分片顺序写入，客户端正常顺序发送时可以稳定保存；乱序重传需要后续增加分片索引校验和临时文件管理。

## 后续扩展建议

后续可以继续补充：

- 文件消息通知，让接收方聊天窗口显示文件卡片。
- 文件下载请求和响应协议。
- MySQL 保存文件消息元数据。
- 上传进度和失败重试。
- 服务端临时文件目录和上传完成后原子重命名。
- MD5 校验失败回包。
- 秒传和断点续传。
