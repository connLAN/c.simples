#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include "10k.jpg.h"

int main() {
    FILE *fp;
    char output_path[256] = {0};
    
    // 检查output文件夹是否存在，如果不存在则创建它
    struct stat st = {0};
    if (stat("output", &st) == -1) {
        #ifdef _WIN32
        if (mkdir("output") != 0) {
        #else
        if (mkdir("output", 0755) != 0) {
        #endif
            printf("无法创建output文件夹: %s\n", strerror(errno));
            return 1;
        }
        printf("已创建output文件夹\n");
    }
    
    // 构建输出路径
    snprintf(output_path, sizeof(output_path), "output/10k.jpg");
    printf("正在提取文件到: %s\n", output_path);
    
    // 打开输出文件
    fp = fopen(output_path, "wb");
    if (fp == NULL) {
        printf("无法创建输出文件 %s: %s\n", output_path, strerror(errno));
        return 1;
    }
    
    // 写入数据
    size_t written = fwrite(_10k_jpg_data, 1, _10k_jpg_size, fp);
    
    // 关闭文件
    fclose(fp);
    
    // 检查是否成功写入所有数据
    if (written == _10k_jpg_size) {
        printf("成功: 已将数据写入到 a.jpg (%zu 字节)\n", written);
        return 0;
    } else {
        printf("错误: 只写入了 %zu 字节，应为 %zu 字节\n", written, _10k_jpg_size);
        return 1;
    }
}