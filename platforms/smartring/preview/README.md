# SmartRing 圆屏浏览器预览

此入口把同一份 LVGL 界面按 SmartRing 的 360×360、中文和像素字体编译到浏览器。页面、歌曲和导航数据是示例；未模拟手机蓝牙、屏幕颜色、硬件触摸和电源电路。

在已安装 Emscripten 的终端，从项目根目录运行：

```sh
emcmake cmake -S . -B build/smartring-web -DMOTO_BUILD_WEB=ON -DMOTO_BUILD_TESTS=OFF -DMOTO_SMARTRING_PREVIEW=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build/smartring-web --parallel
python -m http.server 4173 --directory .
```

浏览器访问 `http://127.0.0.1:4173/platforms/smartring/preview/`。切换按钮可查看导航、速度、指南针、音乐，音乐页可切换中文和长歌名。请使用 HTTP 访问，不要直接双击 HTML；重新编译后刷新网页。

构建文件写入 `runtime/`。本入口不会读取或刷写固件，最终画面和操作仍需在实体设备确认。
