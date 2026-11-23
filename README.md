# Miss. Christine

Miss. Christine 是一只坐在键盘前的 Windows 桌宠。她有两条黑色尾巴、蓝眼黑瞳，红丝带领结中央镶着一颗蓝宝石。移动鼠标时，她会跟着转头。打字时，她会伸爪敲键，闲置一会儿又会打瞌睡。

![Miss. Christine 在键盘前转头、敲键、摆出捕猎表情，又在被摸头时眯起眼睛](assets/preview.gif)

## 下载与运行

支持 Windows 10 和 Windows 11 x64。已在 Windows 11 25H2、2560 × 1440 分辨率、150% 显示缩放下验证。

1. 下载 [Windows x64 压缩包](https://github.com/astr-yuan/Miss-Christine/releases/latest/download/Miss-Christine-Windows-x64.zip)。版本记录见 [最新发行版](https://github.com/astr-yuan/Miss-Christine/releases/latest)。
2. 解压到准备长期存放的文件夹，双击 `KuroCat.exe`。小猫会出现在桌面，任务栏通知区域也会出现猫咪图标。
3. 拖动小猫可以移动位置。右键小猫或点击托盘图标打开菜单，选择「退出 KuroCat」即可关闭。

## 与小猫互动

- 移动光标让小猫转头，快速移动会触发捕猎表情。用光标轻轻来回摸头或摸背，小猫会眯眼。
- 在菜单里选择「演示打字动画」即可看她敲键。开启「3 键同时按『用脚按』彩蛋」后，同时按住三个支持的键可以看到她伸出后脚。
- 「尺寸大小」提供五档缩放。「猫咪皮肤」可切回 Miss. Christine 或其他毛色。「重置位置到右下角」可将小猫移回屏幕工作区。
- 「完全鼠标穿透」让鼠标直接操作小猫下方的窗口。用 `Ctrl + Alt + Shift + P` 或托盘菜单可切换穿透。

程序通过 Windows Raw Input 在本机接收键鼠活动，其他窗口获得焦点时小猫也能响应。按键标签会短暂显示在桌面，可在菜单里关闭「显示按键气泡」。按键状态和本次运行的击键统计保存在内存中，退出后清空。外观与交互偏好保存在当前 Windows 用户的设置里。

## 从源码构建

在已配置 x64 MinGW-w64（含 `g++` 和 `windres`）的终端，或 Visual Studio 的 x64 Native Tools 命令提示符中，从仓库根目录运行以下命令。

```bat
cd windows
build.bat
```

构建产物为 `windows/KuroCat.exe`。工程也支持 CMake 3.20 及以上版本。

## 来源与许可

本项目基于 chanke17 的 KuroCat Windows 版，沿用 Win32 与 GDI+ 实时绘制，加入双尾蓝宝石皮肤，并改进了 Windows 显示缩放、交互和构建。本项目采用 MIT 许可证，并保留上游版权声明。[上游源码版本](https://github.com/chanke17/KuroCat/tree/5fbe37930b07ad01f3c004eae47eaa4d1339ebfe) · [MIT 许可证](LICENSE)
