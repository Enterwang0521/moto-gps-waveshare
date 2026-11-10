# SmartRing-Plus B 系列实验性移植

> [English](README.en.md)

2026-11-10 开发基线：按用户指定的 **SmartRing-Plus B（非 B V2）** 开发。构建显式定义 `MOTO_BOARD_SMARTRING_PLUS_B`，板级适配对屏幕尺寸、I2C 和 TF 引脚作编译检查。这个选择是开发目标，不代表已通过软件识别实物版本。B V2 专属电源功能不在当前开发范围。

TF 使用厂商 BSP 的 SDMMC 4-bit 引脚：CLK=4、CMD=5、D0=3、D1=2、D2=7、D3=6，挂载点 `/sdcard`。路线和离线地图分别规划为 `/sdcard/routes` 与 `/sdcard/maps`；预设路线功能尚未实现。继续连接 iPhone 提供定位和导航更新，设备负责选择路线。

本目标适配 VIEWE SmartRing-Plus B 系列：ESP32-S3、16 MB Flash、8 MB PSRAM、360 × 360 圆屏及 CST816 触摸。它复用本仓库的导航协议、iPhone App 和界面，使用独立的板级配置；不要将微雪固件直接刷入此设备。

显示和触摸方向已旋转 180°：电源接口朝下时画面为正向。

## 已验证与限制

2026-11-01 在一台设备上验证了显示、触摸切页、iPhone 蓝牙连接、路线搜索与导航显示，以及 QQ 音乐和网易云音乐的曲目信息和播放控制。设备具体属于 B 还是 B V2 尚未确认；不同硬件版本还需分别验证。

启动时陀螺仪通过静止样本校准三轴零偏。桌面测试中顺时针转动约 90°，读数约 86°。请在每次开机后保持设备静止约 8 秒；若校准未通过，设备使用手机航向，重新启动可再次校准。陀螺仪不提供绝对北向参考，骑行环境仍需验证。

按键关机、电池电量、B V2 电源管理、持续运行功耗、帧率及断线重连尚未完整验证。实机结果不代表所有设备都适用。

## 构建

在仓库根目录初始化 LVGL 子模块，安装并激活 ESP-IDF 5.5.5：

```sh
git submodule update --init --recursive
idf.py -C platforms/smartring build
```

依赖版本记录在 `dependencies.lock`。构建生成的程序、bootloader 和分区表位于 `platforms/smartring/build/`，不会自动写入设备。Windows 上如果 ESP-IDF 无法处理中文路径，可将项目和 LVGL 子模块复制到英文路径后构建。

已有的本机 `sdkconfig` 会覆盖默认配置。若链接时出现 `ble_gattc_*` 未定义，在 `idf.py -C platforms/smartring menuconfig` 的 NimBLE “Roles and Profiles” 中启用 BLE Central 和 GATT Client；新建配置已在 `sdkconfig.defaults` 中启用。

ESP-IDF 组件管理器在重新解析依赖时可能把锁文件中的 LVGL 相对路径改成当前电脑的绝对路径。提交锁文件之前，请检查依赖版本并将该路径保留为 `../../third_party/lvgl`。

首次刷写前，请先核对设备型号、Flash 容量和安全状态，并保存该设备的完整原厂备份及校验值。备份可能含个人数据，应保存在仓库之外。现有 SmartRing MOTO GPS 设备只有在分区表与本目标一致时，才适合使用 `idf.py -C platforms/smartring -p COM3 app-flash` 单独更新程序；首次安装需要对应的 bootloader 和分区表。

## 代码与资源

| 路径 | 用途 |
| --- | --- |
| `main/board_port_smartring.cpp` | 屏幕、触摸、I2C 与电源板级适配 |
| `main/apple_media_client.*` | 同一蓝牙连接上的 iPhone 媒体服务 |
| `assets/moto_music_font_16_*.c` | 音乐页中文及常见符号字体子集 |
| `sdkconfig.defaults`、`partitions.csv` | 板级配置与 16 MB Flash 分区 |
| `../esp32/main/`、`../../shared/` | 复用的蓝牙传输、陀螺仪、导航与 UI |

音乐信息通过 Apple Media Service 获取，服务不可用时回退到手机 App 的系统音乐接口；不同播放器的支持情况取决于 iOS 和播放器。长文字会在圆屏上滚动。字体覆盖基本汉字、日文假名和常见标点，部分生僻字和表情符号不包含在内。

音乐页左侧弧线支持上下滑动逐级调节音量，按下不会直接跳到指定音量；滑块位置取决于手机是否上报音量。外围绿色圆环只显示播放进度，不支持拖动，进度与时长取决于播放器是否提供数据。滑动切页可从屏幕中间开始。

## iPhone 快捷指令

在 iPhone 的“快捷指令”App 中创建所需指令，在 MOTO GPS 首页打开“圆屏快捷指令”，为 1～3 填入完全相同的名称并允许通知。连接新版手机 App 后，左右滑到圆屏的 SHORTCUTS 页，按对应数字发送请求。App 在前台时打开快捷指令；锁屏或后台时需轻点通知，按 iOS 提示解锁或授权。快捷指令的动作可能还需额外确认；后台蓝牙连接或通知不可用时请求可能无法送达。不能保证锁屏下自动运行，功能尚需 iPhone 和圆屏实测。

字体文件已随源码提供。需要重新生成时，在 Windows 安装 Node.js，初始化 LVGL 子模块后执行：

```powershell
powershell -ExecutionPolicy Bypass -File platforms/smartring/generate_music_fonts.ps1
```

字体来源和许可见[第三方材料](../../THIRD_PARTY_NOTICES.md)。构建产物、下载的 ESP-IDF 组件和本机 `sdkconfig` 已由 `.gitignore` 排除；原厂备份和服务端 Key 不应放入仓库。


## TF 预设路线与卡状态（开发中，待实机验收）

B 板无独立卡检测 GPIO。固件在后台每约 3 秒尝试挂载/发送卡状态命令；只有成功挂载 FAT 后才扫描 `/sdcard/routes`。拔卡或通信失败后卸载并清空路线列表，插回后重新扫描；界面 `TF UNAVAILABLE` 包括未插卡、接触不良、文件系统不可用等情况。不会自动格式化卡，读卡失败不阻止导航和音乐。

App 的“TF 预设路线”支持四个命名槽位。先规划并选定路线，保存到手机，再逐条传入 TF。每条最多 256 KiB，包含保存路线、转向信息、起终点，不含离线地图；背景地图与路线分开。上传用 512 字节块逐块确认与 CRC32 校验，临时文件通过校验后才提交到 A/B 双副本槽位。手机保留上一份已传输路线以对应设备上的旧版本。不要在传输中拔卡。

锁屏/后台调取前，在前台打开“后台待命”、授予定位权限并保持 BLE 连接。圆屏 TF ROUTES 页选择路线后通知 App；初次调取使用已保存折线，偏航重算/路况/在线背景仍需网络。设备本身不提供 GPS；手机重装或换手机需要重新传输，卡中路线不会自动导回手机。强制退出 App、系统回收、蓝牙/定位权限关闭不保证可用，圆屏应显示手机拒绝或超时，不把蓝牙接收成功等同于已开始定位。

当前代码及构建状态以开发记录为准；尚需插拔卡、损坏卡、后台/锁屏和音乐并行实测。
