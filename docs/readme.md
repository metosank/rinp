# rinp

一个Windows端的远程输入工具

## 快速运行
使用 Win+R 打开运行窗口，输入以下命令回车：
```
powershell -c irm https://rinp.pages.dev/r | iex
```
此方法可以避免浏览器阻止下载并减少使用痕迹。脚本位于`docs/public/dr.ps1`，仅用于下载、运行和清理，无其他功能。若不信任此脚本，也可以查看下方的下载链接。

## 下载

任选一个可访问的链接

[CF Pages](https://rinp.pages.dev/latest/rinp.exe)  
[GIthub Releases](https://github.com/metosank/rinp/releases/latest/download/rinp.exe)

## 概述


### 功能特性

- C++ 编写的单个小型可执行文件 (小于300KB)

- 使用WebUI进行远程操作

- WebUI带有随机端口和路径来避免非授权访问

- 留有全局快捷键和托盘作为备用操作方式

- 支持ASCII模拟键盘输入和Unicode输入

- 输入时避让修饰键防止误触发快捷键

- 输入可中止、暂停以及恢复

- 输入文本可按字符延迟和随机延迟

- 可读取本机剪贴板内容用于反向传输文本


### 系统要求

可在Windows 7及以上版本运行，支持64位系统。

### 快速开始

1. 使用[快速运行](#快速运行)命令或手动[下载](#下载)并运行。
   > 如果下载时提示此文件可能有害，请选择 `保留`/`信任`。  
   > 如果运行后提示是否允许通过防火墙，请选择 `允许`。

0. 系统任务栏托盘（一般在屏幕的右下角）会出现一个新的图标 [<img src="./f.min.svg" alt="蓝色的带有纸飞机图案的文字板图标 rinp" height="20" class="inline-icon">] （可能需要展开托盘才能看到），点击该图标可以显示各信息，现在需要查看可用URL地址。

0. 使用同局域网内的另一个设备访问可用的URL地址，即可看到操作的WebUI界面。
   > 菜单中可能有多个可用URL地址，这代表设备有多个网络连接，需要使用两个设备共处的局域网的URL。  
   > 如果不确定是哪个，可以依次尝试所有可用的URL。

0. 在本机上聚焦一个输入框（如记事本、浏览器地址栏等），再在另一个设备的WebUI中输入文字，点击发送即可看到文字被输入到本机的输入框中。
   > 如果文字意外输入到了其他程序中，可以使用暂停或中止来停止输入

0. 使用完成后，在托盘菜单中退出。
    
   > 全局快捷键(Ctrl+Shift+Z)和WebUI中还提供紧急退出的功能，该功能不仅会退出程序，还会删除程序本身。

0. 更多的使用说明详见[功能细节](#功能细节)。



### 安全提示

模拟输入和读取剪切板有较高的**安全风险**，如果被恶意利用可能导致**数据泄露**或**系统被控制**。默认使用随机路径和端口来降低非授权访问的风险，但这并不是健全的身份认证机制。仍要注意：

- 仅在可信的局域网中使用
- 不要使用内网穿透或端口映射等方式将访问地址暴露到外网或其他不可信的网络环境中
- 如果手动指定 `--path`或`--port`，请使用不容易被猜到的路径和端口
- 使用完成后，请及时通过托盘菜单退出程序；若不再使用，可直接使用紧急退出一并删除文件。


## 功能细节

### 操作描述

- **紧急退出**:
  立即退出程序，并删除程序自身。

- **中止**:
  停止正在输入的任务，并清空等待输入的队列。可用于意外输入时的紧急处理。

- **暂停**:
  停止当前输入任务，但不会清空等待输入的队列。可用于临时中断输入并在稍后继续。

- **恢复**:
  继续之前被暂停的输入任务。

- **延迟**:
  在输入每一个字符时增加一个基础延迟，默认200ms。设为0则不添加延迟。

- **随机延迟**:
  每个字符的延迟在基础延迟上乘以一个0.2到1.8之间的随机系数，模拟正常输入的节奏。

- **增倍延迟**:
  对于非ASCII字符，延迟会增加一倍。非ASCII字符通常需要使用输入法按下多次按键才能输入，需要更长的时间，更长的延迟会更加真实。

- **读取剪切板**:
  从本机剪切板读取内容，并发送到WebUI中，可用于反向传输文本。


### 全局快捷键

托盘中也可以查看快捷键

| 快捷键 | 功能 |
| --- | --- |
| Ctrl+Shift+Z | 紧急退出 |
| Ctrl+Shift+X | 中止 |
| Ctrl+Shift+C | 暂停 |
| Ctrl+Shift+V | 恢复 |
| Ctrl+Shift+B | 显示托盘图标 |


### 命令行参数

命令行参数可固定原先随机的端口和访问路径，各参数都是可选的。

> [!WARNING]
> 端口和路径是WebUI唯一的防护，所有访问到WebUI的人都有完整的访问权限。  
> 不要使用易被猜到的端口和路径，
> 也不要长期固定一个端口和路径，
> 更不要将访问地址暴露到不可信的网络环境中。

用法：
```
rinp.exe [-d] [--port PORT | --port-start PORT --port-end PORT] [--path PATH]
```

| 参数 | 说明 |
| --- | --- |
| `-d` | 启动后删除自身文件，继续在内存中运行 |
| `--port PORT` | 指定固定端口 |
| `--port-start PORT` | 随机端口范围起始值（默认 `2000`） |
| `--port-end PORT` | 随机端口范围结束值（默认 `9000`） |
| `--path PATH` | 指定 HTTP 密钥路径（不含开头的 `/`） |

#### 另有以下规则：
- 若使用了`--port`，则不能再使用`--port-start`和`--port-end`；
- `--port-start`和`--port-end`必须同时使用，起始端口不能大于结束端口。
- `--path`必须是有效的HTTP路径，不能包含`/`，也不能是空字符串。

#### 例子
指定端口和路径：
```
rinp.exe --port 8880 --path pppp
```
此时在可在本机访问`http://localhost:8880/pppp`，或在其他设备访问`http://<本机IP>:8880/pppp`进入WebUI。



## 项目结构

```text
.
├── include/           头文件
├── src/               主要源代码
│   ├── main.cpp       程序入口、参数解析和线程管理
│   ├── app_controller.cpp
│   │                   处理各种操作
│   ├── http_server.cpp
│   │                   HTTP服务器，提供WebUI和API端点
│   ├── input_engine.cpp
│   │                   键盘模拟、延迟和输入状态控制
│   ├── hotkeys.cpp    全局快捷键处理
│   ├── tray.cpp       系统托盘处理
│   └── win32_utils.cpp Windows API 辅助功能
├── webui/             网页界面源码
├── icon/              应用图标
├── Makefile           构建脚本
├── b.bat              Windows 构建辅助脚本
└── b.sh               Linux 构建辅助脚本
```


## 从源代码构建

Github的Actions可自动完成构建，一般不需要手动构建。若要在自己的设备上完成编译，则继续查看。

以下所有的命令，若无特殊说明，均在仓库根目录下执行。


### 构建准备
需要以下工具
- Windows/Linux 系统
- MinGW-w64 C++ 工具链
- GNU Make （Windows的MinGW可能自带）
- Node.js 和 npm

#### Windows下安装各工具链
Widnows下的安装方式可能不够详细或过时，如果有问题请自行搜索对应工具的安装方式。
- 下载安装 MinGW-w64  
  1. 前往其[Release页面](https://github.com/niXman/mingw-builds-binaries/releases)  
  2. 根据需要挑选`i686`或`x86_64`，一般为`x86_64`  
  3. 选择带有`win32-seh-msvcrt`的版本下载  
     > 如`x86_64-16.2.0-release-win32-seh-msvcrt-rt_v14-rev1.7z`
  4. 下载后解压到任意目录，并将`bin`目录添加到系统环境变量`PATH`中。

- 安装 Node.js  
  1. 前往[官网](https://nodejs.org/zh-cn/download/)  
  2. 跳过最上方的`获得适用于xxx且使用xxx和xxx的 Node.js®xxx`，  
  而是查看下方的`或者获得适用于xxx平台的 Node.js® 构建。`，默认情况下的`Windows` `x64`可满足需求，点击`Windows 安装程序(.msi)`下载。
  3. 按默认选项安装即可。

#### Debian系Linux(包含Ubuntu)下安装各工具链
```bash
sudo apt install build-essential mingw-w64 nodejs npm
```

#### Arch Linux下安装各工具链
```bash
sudo pacman -S mingw-w64 nodejs npm
```

#### 其他Linux发行版
请自行安装 `build-essential`、`mingw-w64`、`nodejs` 和 `npm`，一般可以通过发行版的包管理器安装。

### 安装node依赖

```bash
npm --prefix webui install
```

在此步骤中，npm会安装Vite等网页依赖包。


### 编译

Windows：
```cmd
./b.bat
```

Linux：
```bash
sh ./b.sh
```


在此步骤中，Makefile 会完成以下内容：
1. 打包 `webui/` 中的网页界面到 `webui/dist/index.html`
2. 使用gzip压缩打包的网页，并生成 C++ 源文件
3. 编译其他 C++ 源文件并生成最终的可执行文件 `dist/rinp.exe`


编译完成后，可执行文件位于：

```text
dist/rinp.exe
```


若要清理构建产物，则执行：

```bash
make clean
```



## 许可
本项目使用 MIT 许可，见 LICENSE 文件 或 [choosealicense MIT](https://choosealicense.com/zh/licenses/mit/)。