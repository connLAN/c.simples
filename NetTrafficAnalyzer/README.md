# 网络流量分析器 (Network Traffic Analyzer)

这是一个用C语言编写的网络流量分析工具，用于监控和分析网络连接，检测可疑IP活动，并生成流量报告。

## 主要功能

- 记录网络连接信息（IP地址、时间戳、传输字节数）
- 生成每小时和每日流量统计报告
- 检测可疑IP活动（基于短时间内的高频请求）
- 导出流量报告和可疑IP报告为CSV格式
- 支持流量数据的排序和分析

## 构建说明

### 系统要求

- GCC编译器
- Make工具
- Linux/Unix环境

### 编译步骤

```bash
# 克隆仓库后，在项目目录中执行：
make

# 清理编译文件
make clean
```

## 使用示例

### 基本用法

```c
// 添加连接记录
add_connection("192.168.1.1", time(NULL), 1500);

// 生成每日报告
size_t report_count;
TrafficReport* reports = generate_daily_report(time(NULL), &report_count);

// 导出报告为CSV
export_csv(reports, report_count, "report.csv");

// 释放内存
free_report(reports, report_count);
```

### 可疑IP检测

```c
// 重置IP统计信息
reset_ip_stats();

// 添加连接记录（系统会自动检测可疑IP）
add_connection("192.168.0.100", time(NULL), 500);

// 导出可疑IP报告
export_suspicious_ips("suspicious_ips.csv");
```

## API文档

### 核心函数

#### `void add_connection(const char* ip, time_t ts, uint64_t bytes)`
添加一条新的连接记录。

参数：
- `ip`: IP地址字符串
- `ts`: 连接时间戳
- `bytes`: 传输的字节数

#### `TrafficReport* generate_daily_report(time_t ref_ts, size_t* count)`
生成指定日期的流量报告。

参数：
- `ref_ts`: 参考时间戳
- `count`: 用于存储报告数量的指针

返回：
- 流量报告数组指针，使用后需要通过`free_report`释放

#### `void export_suspicious_ips(const char* filename)`
将可疑IP报告导出为CSV文件。

参数：
- `filename`: 输出文件名

### 数据结构

#### `TrafficReport`
```c
typedef struct {
    char period[32];      // 时间段标识
    uint64_t total_bytes; // 总流量（字节）
} TrafficReport;
```

#### `SuspiciousIP`
```c
typedef struct {
    char ip[16];         // IP地址
    uint32_t request_count; // 请求总数
    time_t first_seen;   // 首次检测时间
    time_t last_seen;    // 最后检测时间
    char reason[128];    // 被标记为可疑的原因
} SuspiciousIP;
```

## 配置参数

以下参数可以在 `net_traffic_analyzer.h` 中配置：

- `MAX_RECORDS`: 最大连接记录数
- `MAX_IP_STATS`: 最大IP统计记录数
- `SUSPICIOUS_TIME_WINDOW`: 可疑IP检测的时间窗口（秒）
- `SUSPICIOUS_REQUESTS_THRESHOLD`: 时间窗口内触发可疑标记的请求阈值

## 输出文件格式

### 流量报告 (report.csv)
```csv
Period,Total Bytes
2023-07-01,150000
```

### 可疑IP报告 (suspicious_ips.csv)
```csv
IP,Requests,First Seen,Last Seen,Reason
192.168.0.100,110,2023-07-01 00:09:58,2023-07-01 00:10:02,"High frequency: 110 requests in 60 seconds"
```

## 注意事项

1. 所有字符串缓冲区都已实现安全的边界检查
2. 内存管理已经过优化，使用后需要正确释放资源
3. 时间戳使用UNIX时间格式（秒）
4. CSV导出使用UTF-8编码

## 许可证

本项目采用MIT许可证。详见LICENSE文件。