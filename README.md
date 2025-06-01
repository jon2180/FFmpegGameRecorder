# FFmpegGameRecorder

基于 FFmpeg 的UE4游戏录制插件，提供高性能的游戏画面和音频捕获与编码功能。

## 功能特性

- 支持游戏画面实时捕获和录制
- 支持游戏音频同步录制
- 支持自定义录制区域（可指定屏幕坐标和尺寸）
- 支持自定义帧率和比特率设置
- 支持硬件加速编码（可选）
- 支持多平台（Windows、Mac、Android）
- 基于FFmpeg强大的编解码能力，提供高质量视频输出
 
## 安装方法

1. 将插件复制到您的UE4项目的 Plugins 目录下
2. 重新启动UE4编辑器
3. 在编辑器中启用插件（编辑 > 插件 > 其他 > FFmpegGameRecorder）

## 使用方法
### 蓝图中使用

插件提供了 GameRecorderEntry 类，可以在蓝图中直接调用：

1. 开始录制：
   - 调用 StartRecord 函数，传入录制区域参数（ScreenX, ScreenY, ScreenW, ScreenH）
   - 函数将返回保存文件的路径
2. 停止录制：   
   - 调用 StopRecord 函数

### C++中使用

1. 包含相关头文件：
```
#include "GameRecorderEntry.h"
```

2. 开始录制：
```
FString FilePath = UGameRecorderEntry::StartRecord(0, 0,  1920, 1080);
```

3. 停止录制：
```
UGameRecorderEntry::StopRecord();
```

## 配置选项

录制配置通过 FRecorderConfig 结构体进行设置，主要参数包括：

- SaveFilePath ：保存文件路径
- FrameRate ：帧率
- CropArea ：截取区域
- bUseHardwareEncoding ：是否使用硬件编码
- VideoBitRate ：视频比特率
- AudioBitRate ：音频比特率
- AudioSampleRate ：音频采样率
- SoundVolume ：音量

## 系统要求

- Unreal Engine 4.25或更高版本
- 支持的平台：Windows、Mac、Android

## 许可证
### FFmpeg License
FFmpeg is licensed under the GNU Lesser General Public License (LGPL) version 2.1 or later.

FFmpeg is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License as published by the Free Software Foundation; either version 2.1 of the License, or (at your option) any later version.

FFmpeg is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License along with FFmpeg; if not, write to the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA

### x264 License
x264 is licensed under the GNU General Public License (GPL) version 2 or later.

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 2 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

## 作者
- 创建者：jon2180
