clear# C Client-Server Application / C语言客户端-服务器应用

This project implements a simple client-server architecture in C, featuring IP address filtering through a blacklist and whitelist mechanism with dynamic configuration reloading.

本项目实现了一个简单的C语言客户端-服务器架构，具有通过黑白名单机制进行IP地址过滤和动态配置重载功能。

## Core Features / 核心功能

- **Client-Server Communication** / 客户端-服务器通信:
  - TCP-based communication on port 8080 / 基于TCP的8080端口通信
  - Client sends "Hello from client" message / 客户端发送"Hello from client"消息
  - Server responds with acknowledgment / 服务器返回确认响应

- **IP Address Filtering** / IP地址过滤:
  - Blacklist/Whitelist system / 黑白名单系统
  - Supports up to 100 IP addresses / 支持最多100个IP地址
  - Dynamic reloading via SIGHUP signal (kill -HUP <pid>) / 通过SIGHUP信号动态重载配置(kill -HUP <pid>)

- **Logging System** / 日志系统:
  - Detailed debug output / 详细的调试输出
  - Connection attempts logging / 记录连接尝试
  - Filtering decisions logging / 记录过滤决策

## Project Structure / 项目结构

```
c-client-server-app
├── src
│   ├── client.c        # Client implementation / 客户端实现
│   │   - Creates TCP socket / 创建TCP套接字
│   │   - Connects to server / 连接服务器
│   │   - Sends test message / 发送测试消息
│   │   - Receives server response / 接收服务器响应
│   ├── server.c        # Server implementation / 服务器实现
│   │   - Creates TCP server / 创建TCP服务器
│   │   - Binds to port 8080 / 绑定到8080端口
│   │   - Implements IP filtering / 实现IP过滤
│   │   - Handles SIGHUP for config reload / 处理SIGHUP信号重载配置
│   ├── utils.c         # Utility functions / 工具函数
│   │   - IP list loading / IP列表加载
│   │   - IP validation / IP验证
│   │   - Debug output / 调试输出
│   ├── blacklist.txt    # Blocked IP addresses / 黑名单IP地址
│   ├── whitelist.txt    # Allowed IP addresses / 白名单IP地址
│   └── Makefile         # Build rules / 构建规则
└── README.md            # Project documentation / 项目文档
```

## Getting Started / 快速开始

### Prerequisites / 先决条件

- GCC compiler (version 9.4.0 or higher) / GCC编译器(9.4.0或更高版本)
- Make utility / Make工具
- Linux/Unix environment (for signal handling) / Linux/Unix环境(用于信号处理)
- libcjson-dev (for JSON support) / libcjson-dev(用于JSON支持)

Install dependencies on Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install -y gcc make libcjson-dev
```

### Building the Project / 构建项目

```bash
make
```

This will compile: / 这将编译:
- Client and server executables / 客户端和服务器可执行文件
- Static library (libipfilter.a) / 静态库(libipfilter.a)
- Shared library (libipfilter.so) / 动态库(libipfilter.so)

To install the libraries system-wide: / 系统级安装库:
```bash
sudo make install
```
This will install: / 这将安装:
- Libraries to /usr/local/lib / 库文件到/usr/local/lib
- Headers to /usr/local/include / 头文件到/usr/local/include

### Running the Server / 运行服务器

```bash
./src/server
```

Server options: / 服务器选项:
- Runs on port 8080 by default / 默认运行在8080端口
- Uses `blacklist.txt` and `whitelist.txt` from current directory / 使用当前目录下的黑白名单文件
- Logs connections and filtering decisions to stdout / 将连接和过滤决策记录到标准输出

### Running the Client / 运行客户端

```bash
./src/client
```

Client options: / 客户端选项:
- Connects to localhost:8080 by default / 默认连接到localhost:8080
- Sends test message and displays server response / 发送测试消息并显示服务器响应

## IP Address Filtering Configuration / IP地址过滤配置

### File Format / 文件格式

Example `blacklist.txt`:
```
# Blocked IP addresses
192.168.1.100
10.0.0.5
# Malicious IP
203.0.113.42
```

Example `whitelist.txt`:
```
# Allowed IP addresses
192.168.1.101
10.0.0.1
```

Rules: / 规则:
- One IP address per line / 每行一个IP地址
- Lines starting with # are comments / 以#开头的行是注释
- Maximum 100 IP addresses per list / 每个列表最多100个IP地址
- Whitelist takes precedence over blacklist / 白名单优先于黑名单

### Dynamic Reloading / 动态重载

To reload configuration without restarting server: / 无需重启服务器即可重载配置:
```bash
kill -HUP $(pgrep server)
```

## Example Usage Scenarios / 使用示例

1. Basic Communication: / 基本通信:
```bash
# Terminal 1
./src/server

# Terminal 2
./src/client
```

2. IP Filter Testing: / IP过滤测试:
```bash
# Add client IP to blacklist / 将客户端IP加入黑名单
echo "127.0.0.1" > src/blacklist.txt
kill -HUP $(pgrep server)

# Client will now be blocked / 客户端现在将被阻止
./src/client
```

## Troubleshooting / 故障排除

**Server won't start:** / 服务器无法启动:
- Check if port 8080 is available: `netstat -tuln | grep 8080` / 检查8080端口是否可用
- Ensure you have permissions to bind to the port / 确保有绑定端口的权限

**Client can't connect:** / 客户端无法连接:
- Verify server is running / 确认服务器正在运行
- Check firewall settings / 检查防火墙设置
- Ensure client IP isn't in blacklist / 确保客户端IP不在黑名单中

**Configuration not reloading:** / 配置未重载:
- Verify server process ID is correct / 确认服务器进程ID正确
- Check file permissions on blacklist.txt/whitelist.txt / 检查黑白名单文件的权限

## Contributing / 贡献指南

1. Fork the repository / Fork代码库
2. Create your feature branch (`git checkout -b feature/fooBar`) / 创建特性分支
3. Commit your changes (`git commit -am 'Add some fooBar'`) / 提交更改
4. Push to the branch (`git push origin feature/fooBar`) / 推送分支
5. Create a new Pull Request / 创建Pull Request

Testing requirements: / 测试要求:
- All new code should include unit tests / 所有新代码应包含单元测试
- Manual testing of IP filtering scenarios / 手动测试IP过滤场景
- Verify signal handling works correctly / 验证信号处理正常工作

## License / 许可证

MIT License - see [LICENSE](LICENSE) file for details. / MIT许可证 - 详见[LICENSE](LICENSE)文件
