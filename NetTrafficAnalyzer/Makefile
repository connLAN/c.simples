# Makefile for Network Traffic Analyzer

# 编译器设置
CC = gcc
CFLAGS = -Wall -Wextra -O2
DEBUG_FLAGS = -g -DDEBUG

# 目标可执行文件
TARGET = net_analyzer

# 源文件和对象文件
SRCS = main.c net_traffic_analyzer.c
OBJS = $(SRCS:.c=.o)

# 默认目标
all: $(TARGET)

# 编译可执行文件
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^

# 编译源文件为对象文件的规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 调试版本
debug: CFLAGS = -Wall -Wextra -g -DDEBUG
debug: clean all

# 清理生成的文件
clean:
	rm -f $(TARGET) $(OBJS) *.csv

# 安装
install: $(TARGET)
	mkdir -p $(DESTDIR)/usr/local/bin
	cp $(TARGET) $(DESTDIR)/usr/local/bin/
	chmod 755 $(DESTDIR)/usr/local/bin/$(TARGET)

# 卸载
uninstall:
	rm -f $(DESTDIR)/usr/local/bin/$(TARGET)

# 运行程序
run: $(TARGET)
	./$(TARGET)

# 生成依赖关系
depend: $(SRCS)
	$(CC) -MM $(CFLAGS) $^ > .depend

# 包含生成的依赖文件
-include .depend

# 伪目标声明
.PHONY: all debug clean install uninstall run depend