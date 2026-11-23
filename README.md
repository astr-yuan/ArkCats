# Miss. Christine

Miss. Christine 是一只坐在键盘前的 Windows 桌宠。她有两条黑色尾巴、蓝眼黑瞳，红丝带领结中央镶着一颗蓝宝石。移动鼠标时，她会跟着转头。打字时，她会伸爪敲键，闲置一会儿又会打瞌睡。

![Miss. Christine 在键盘前转头、敲键、摆出捕猎表情，又在被摸头时眯起眼睛](assets/preview.gif)

## 下载与运行

适用于 Windows 10（1607 及以上）和 Windows 11 x64。通知组件需要 .NET Framework 4.8 或 4.8.1。已在 Windows 11 25H2、2560 × 1440 分辨率、150% 显示缩放下验证。

1. 下载 [Windows x64 压缩包](https://github.com/astr-yuan/Miss-Christine/releases/latest/download/Miss-Christine-Windows-x64.zip)。版本记录见 [最新发行版](https://github.com/astr-yuan/Miss-Christine/releases/latest)。
2. 将整个 ZIP 解压到准备长期存放的文件夹，保留 `KuroCat.exe` 和 `ChristineNotifications.exe` 在同一目录。双击 `KuroCat.exe` 即可启动。小猫会出现在桌面，任务栏通知区域也会出现猫咪图标。
3. 拖动小猫可以移动位置。右键小猫或点击托盘图标打开菜单，选择「退出 KuroCat」即可关闭。

从旧版升级时，先在托盘中退出旧版，再把新版解压到原目录。已有的皮肤、位置和尺寸设置会沿用。

## 收到 Windows 通知时

右键小猫或点击托盘猫图标，勾选 **「收到 Windows 通知时做出反应」**，并在系统询问时允许通知访问。首次使用时此功能默认关闭。再次点击同一项即可关闭。

当任意应用在 Windows 通知中心添加一条通知，小猫会根据当时的姿势作出反应。

| 当时的状态 | 小猫的动作 |
| --- | --- |
| 待机 | 耳朵竖起，身子轻弹，两条尾巴一挑。 |
| 打瞌睡 | 慢半拍睁圆眼睛、抬起头，再慢慢眯回去。 |
| 敲键盘 | 两只爪子在键面上方停一下，然后接着敲。 |
| 用鼠标 | 一只爪子搭着鼠标，另一只轻缩，回头张望后继续追光标。 |

<details>
<summary>查看四种动作的同步演示</summary>

![同一条模拟通知到达时，Miss. Christine 在四种姿势下的反应](assets/notification-reactions.webp)

</details>

开启时已有的通知会被跳过，五秒内连续到达的多条通知合并为一次动作。程序在本机定期比较通知编号与创建时间，消息标题、正文和发送者信息均无需读取。要体验一次，可以让平时就会出现在通知中心的应用产生一条新通知，并保留几秒。在 Windows 11 上按 `Win + N` 可以查看通知中心。

菜单显示「已就绪，等待新通知」时，监听已开始。如果 Windows 拒绝访问，可以在系统设置中允许通知访问，再从小猫菜单选择「重新连接并检查通知权限」。

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

构建会同时生成 `windows/KuroCat.exe` 和 `windows/ChristineNotifications.exe`。通知组件使用 Windows 的 .NET Framework 4.8 和 WinRT 通知接口。工程也支持 CMake 3.20 及以上版本。

## 来源与许可

本项目基于 chanke17 的 KuroCat Windows 版，沿用 Win32 与 GDI+ 实时绘制，加入双尾蓝宝石皮肤，并改进了 Windows 显示缩放、交互和构建。本项目采用 MIT 许可证，并保留上游版权声明。[上游源码版本](https://github.com/chanke17/KuroCat/tree/5fbe37930b07ad01f3c004eae47eaa4d1339ebfe) · [MIT 许可证](LICENSE)
