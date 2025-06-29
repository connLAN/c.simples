# bin2arr - 二进制文件转C数组工具

这个项目提供了一对实用工具，用于在二进制文件和C语言头文件之间进行转换。这对于需要将二进制资源（如图像、音频或其他数据文件）嵌入到C/C++程序中的开发者特别有用。

## 组件

项目包含两个主要组件：

1. **bin2arr** - 将任意二进制文件转换为C语言头文件，其中包含表示原始数据的字节数组
2. **extract** - 从生成的头文件中提取数据并还原为原始二进制文件

## 安装与构建

1. 克隆此仓库（如果适用）：
```bash
git clone <repository-url>
cd bin2arr
```

2. 编译工具：
```bash
gcc -o bin2arr bin2arr.c
gcc -o extract extract.c
```

3. 创建输出目录（extract程序需要）：
```bash
mkdir output
```

## 测试

1. 使用bin2arr转换测试文件：
```bash
./bin2arr 10k.jpg 10k.jpg.h
```

2. 检查生成的头文件：
```bash
cat 10k.jpg.h
```

3. 运行extract程序提取数据：
```bash
./extract
```

4. 验证输出文件：
```bash
ls -l output/
```

## 使用方法

### bin2arr

将二进制文件转换为C语言头文件：

```bash
./bin2arr <输入文件> <输出头文件>
```

例如：

```bash
./bin2arr image.jpg image.h
```

这将创建一个名为`image.h`的头文件，其中包含：
- 一个名为`_image_jpg_data`的字节数组，包含原始文件的内容
- 一个名为`_image_jpg_size`的变量，表示数据的大小（字节数）

### extract

从头文件中提取数据并还原为原始文件：

```bash
./extract
```

extract程序会读取头文件中的数据，并将其写入到output目录中。默认情况下，extract.c被配置为使用"10k.jpg.h"头文件，并将数据提取到"output/10k.jpg"。

**注意:** 如果要处理不同的文件，需要修改extract.c中的以下部分：
1. 更改包含的头文件名
2. 更新输出文件路径
3. 更新使用的数组变量名

## 工作原理

### bin2arr

bin2arr程序读取输入的二进制文件，并将其内容转换为C语言数组格式。它会：

1. 打开输入的二进制文件
2. 读取文件内容
3. 生成一个C语言头文件，其中包含：
   - 一个表示文件内容的无符号字符数组
   - 一个表示文件大小的常量

生成的变量名基于输入文件名，例如：
- 对于`image.jpg`，生成的变量名为`_image_jpg_data`和`_image_jpg_size`
- 对于`sound.wav`，生成的变量名为`_sound_wav_data`和`_sound_wav_size`

### extract

extract程序从生成的头文件中读取数据，并将其写入到输出文件中。它会：

1. 包含生成的头文件
2. 访问头文件中定义的数据数组和大小变量
3. 将数据写入到输出文件中

## 示例

以下是一个完整的示例，展示如何使用这些工具：

```bash
# 将图像文件转换为C语言头文件
./bin2arr image.jpg image.h

# 编译使用该头文件的程序
gcc -o myprogram myprogram.c

# 运行extract程序提取数据
./extract
```

## 应用场景

这个工具对以下场景特别有用：

- 嵌入式系统开发，需要将资源文件嵌入到固件中
- 创建独立的可执行文件，无需外部资源文件
- 将图像、字体或其他资源直接编译到应用程序中
- 保护或隐藏资源文件，避免它们作为单独的文件存在

## 高级用法

### bin2arr的高级功能

1. **输出到标准输出**：
```bash
./bin2arr input.bin
```
这将把生成的C数组直接输出到终端。

2. **使用管道重定向输出**：
```bash
./bin2arr input.bin > output.h
```

3. **处理特殊文件名**：
bin2arr会自动将文件名中的非字母数字字符转换为下划线，例如：
- `my-file.bin` → `my_file_bin`
- `test@123.data` → `test_123_data`

## 注意事项

### 修改extract.c以适应不同文件

extract.c默认配置为处理"10k.jpg"文件。要处理其他文件，需要修改以下部分：

1. 更改包含的头文件：
```c
#include "10k.jpg.h" → #include "yourfile.h"
```

2. 更新输出路径：
```c
snprintf(output_path, sizeof(output_path), "output/10k.jpg");
→ 
snprintf(output_path, sizeof(output_path), "output/yourfile.ext");
```

3. 更新使用的数组变量名：
```c
fwrite(_10k_jpg_data, 1, _10k_jpg_size, fp);
→ 
fwrite(_yourfile_ext_data, 1, _yourfile_ext_size, fp);
```

## 限制

- 对于非常大的文件，生成的头文件可能会变得非常大，可能导致编译时间增加
- 某些编译器可能对数组大小有限制
- extract.c当前版本需要手动修改源代码来处理不同的文件

## 许可

本项目使用 [MIT许可证](LICENSE)。基本权限包括：

- 自由使用、复制、修改、合并、发布、分发、再许可和/或出售软件
- 允许将软件用于任何目的，包括商业用途
- 唯一的限制是在所有副本或实质性部分中包含版权声明和许可声明

如果你没有看到LICENSE文件，可以创建一个包含以下内容的文件：

```
MIT License

Copyright (c) [year] [fullname]

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

[在此添加许可信息]