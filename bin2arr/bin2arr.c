#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void print_usage(const char* program_name) {
    printf("用法: %s <输入文件> [输出文件]\n", program_name);
    printf("如果不指定输出文件，将输出到标准输出\n");
}

int main(int argc, char *argv[]) {
    FILE *input_file, *output_file;
    unsigned char buffer[16];
    size_t bytes_read;
    size_t total_bytes = 0;
    
    // 检查命令行参数
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    // 打开输入文件
    input_file = fopen(argv[1], "rb");
    if (!input_file) {
        printf("错误：无法打开输入文件 '%s'\n", argv[1]);
        return 1;
    }

    // 确定输出目标
    output_file = (argc == 3) ? fopen(argv[2], "w") : stdout;
    if (!output_file) {
        printf("错误：无法打开输出文件 '%s'\n", argv[2]);
        fclose(input_file);
        return 1;
    }

    // 获取文件名（不包含路径）
    const char *filename = strrchr(argv[1], '/');
    filename = filename ? filename + 1 : argv[1];

    // 生成变量名前缀
    char var_prefix[256] = {0};
    const char *p = filename;
    int i = 0;
    while (*p && i < sizeof(var_prefix) - 1) {
        if ((*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) {
            var_prefix[i++] = *p;
        } else {
            var_prefix[i++] = '_';
        }
        p++;
    }
    var_prefix[i] = '\0';

    // 写入头文件内容
    fprintf(output_file, "// 由 bin2arr 工具生成\n");
    fprintf(output_file, "// 源文件: %s\n\n", filename);
    
    // 写入数据数组
    fprintf(output_file, "const unsigned char _%s_data[] = {\n    ", var_prefix);

    // 读取文件内容并转换为数组格式
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), input_file)) > 0) {
        for (size_t i = 0; i < bytes_read; i++) {
            fprintf(output_file, "0x%02X, ", buffer[i]);
            total_bytes++;
            
            // 每16个字节换行以提高可读性
            if (total_bytes % 16 == 0) {
                fprintf(output_file, "\n    ");
            }
        }
    }

    // 移除最后一个逗号和空格（如果有的话）
    if (total_bytes > 0) {
        if (fseek(output_file, -2, SEEK_CUR) != 0) {
            // 如果fseek失败（例如在stdout上），不进行处理
        }
    }

    // 完成数组定义
    fprintf(output_file, "\n};\n\n");
    fprintf(output_file, "const size_t _%s_size = %zu;\n", var_prefix, total_bytes);

    // 关闭文件
    fclose(input_file);
    if (output_file != stdout) {
        fclose(output_file);
    }

    return 0;
}