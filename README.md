# opus-ogg
> Data: 2024/11/10

关键点
1. 使用`C/C++`封装的opus-ogg音频编解码库，可直接用于`C/C++`项目。
2. 使用`Cgo`提供`Golang`接口，方便接入`Golang`项目。

操作系统: Linux

开源库版本
- opus: 1.5.2
- ogg: 1.3.5

编码
- 输入：24kHz-16-mono-pcm 
- 输出: opus

```text
encoding: opus
channels: 1
sampleRage: 24000
frameSize: 480 (20ms@24kHz)
bitRate: 48kbps
```

TODO
1. 规范错误码


### 文件夹 opus
- 支持pcm、裸opus音频数据的编解码。

### 文件夹 opus-codec
- 采用自定义封装，每个opus数据帧前2字节，按照大端序写了该数据帧的长度。
- 仅实现了编码操作。

### 文件夹 opus-codec-dlopen
- 使用 `dlopen` 动态打开动态库。

### 文件夹 opus-ogg-codec
- 采用 ogg 开源封装格式
- 支持 pcm、opus-ogg 的音频数据的编解码。





