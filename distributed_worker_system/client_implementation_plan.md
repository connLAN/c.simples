# 分布式工作系统客户端实现计划

## 系统当前状态

经过调查，当前的分布式工作系统包含以下组件：
1. **中央服务器(Central Server)**：负责接收工作服务器的注册，分配任务，并收集结果
2. **工作服务器(Worker Server)**：连接到中央服务器，执行分配的任务，并返回结果

目前系统中**没有专门的客户端组件**来提交任务。任务是通过在中央服务器代码中调用`create_sample_task()`函数来创建的。

## 客户端实现计划

### 1. 创建客户端源文件

在client目录中创建以下文件：
- `client.c`：客户端主要实现
- `client.h`：客户端头文件（如果需要）

### 2. 实现客户端功能

客户端应具备以下功能：
- 连接到中央服务器
- 提交任务请求
- 接收任务执行结果
- 提供用户友好的界面（命令行或简单的交互式界面）

### 3. 更新通信协议

在`common/comm_protocol.h`中添加客户端相关的消息类型和结构：
- `MSG_SUBMIT_TASK`：客户端提交任务的消息类型
- `MSG_TASK_RESULT`：服务器返回任务结果的消息类型

### 4. 更新中央服务器

修改`central_server.c`，使其能够：
- 处理来自客户端的连接
- 接收客户端提交的任务
- 将任务结果返回给客户端

### 5. 更新Makefile

添加客户端组件的编译规则，例如：
```makefile
client: client/client.c common/comm_protocol.h
	$(CC) $(CFLAGS) -o build/client client/client.c $(LDFLAGS)
```

### 6. 更新README.md

添加关于客户端使用的说明，包括：
- 如何编译客户端
- 如何运行客户端
- 如何提交任务
- 如何查看任务结果

## 客户端实现细节

### 客户端基本流程

1. 连接到中央服务器（默认端口8888）
2. 发送任务提交请求，包含任务类型和参数
3. 等待任务执行结果
4. 显示结果或错误信息
5. 断开连接或继续提交新任务

### 客户端命令行界面示例

```
Usage: client [options]
Options:
  -h, --host HOST     Central server host (default: localhost)
  -p, --port PORT     Central server port (default: 8888)
  -t, --task TYPE     Task type (required)
  -a, --args ARGS     Task arguments (required)
  -v, --verbose       Enable verbose output
  --help              Display this help message
```

### 客户端交互式界面示例

```
Connected to central server at localhost:8888
Available task types:
1. TASK_TYPE_COMPUTE
2. TASK_TYPE_IO
3. TASK_TYPE_NETWORK

Enter task type (1-3): 1
Enter task arguments: data=123&iterations=1000
Submitting task...
Task submitted successfully. Task ID: 42
Waiting for result...
Task completed. Result: 12345
Submit another task? (y/n): n
Disconnecting from server. Goodbye!
```

## 下一步建议

1. **实现基本客户端**：
   - 首先实现一个简单的命令行客户端，能够连接到中央服务器并提交预定义的任务
   - 测试与中央服务器的通信是否正常

2. **扩展客户端功能**：
   - 添加更多任务类型和参数选项
   - 实现更友好的用户界面
   - 添加错误处理和重试机制

3. **增强系统安全性**：
   - 添加身份验证机制
   - 实现加密通信
   - 添加访问控制

4. **改进系统可靠性**：
   - 实现断线重连
   - 添加任务状态查询功能
   - 实现任务取消功能

5. **优化系统性能**：
   - 实现异步任务提交
   - 添加任务优先级
   - 实现任务批量提交

## 示例代码片段

### 客户端连接到服务器

```c
int connect_to_server(const char *host, int port) {
    int sock = 0;
    struct sockaddr_in serv_addr;
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("\n Socket creation error \n");
        return -1;
    }
    
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);
    
    if (inet_pton(AF_INET, host, &serv_addr.sin_addr) <= 0) {
        printf("\nInvalid address/ Address not supported \n");
        return -1;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("\nConnection Failed \n");
        return -1;
    }
    
    return sock;
}
```

### 客户端提交任务

```c
int submit_task(int sock, int task_type, const char *task_args) {
    message_t msg;
    msg.type = MSG_SUBMIT_TASK;
    msg.status = STATUS_OK;
    
    task_t task;
    task.type = task_type;
    strncpy(task.args, task_args, sizeof(task.args) - 1);
    task.args[sizeof(task.args) - 1] = '\0';
    
    memcpy(msg.data, &task, sizeof(task));
    msg.data_len = sizeof(task);
    
    return send(sock, &msg, sizeof(message_t), 0);
}
```

### 客户端接收任务结果

```c
int receive_task_result(int sock, char *result, size_t result_size) {
    message_t msg;
    int bytes_received = recv(sock, &msg, sizeof(message_t), 0);
    
    if (bytes_received <= 0) {
        return -1;
    }
    
    if (msg.type != MSG_TASK_RESULT) {
        return -2;
    }
    
    if (msg.status != STATUS_OK) {
        return msg.status;
    }
    
    strncpy(result, (char *)msg.data, result_size - 1);
    result[result_size - 1] = '\0';
    
    return 0;
}
```

这个计划提供了实现客户端组件的详细步骤和建议。根据系统的具体需求和复杂性，可能需要进一步调整和扩展。