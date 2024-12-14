# opus-ogg
> Data: 2024/11/10

## 简介
- 采用opus开源库对pcm原始音频进行编解码操作。
- 两种封装方式
  - 自定义封装，每个opus数据帧前2字节，按照大端序写了该opus数据帧的长度。
  - ogg开源库进行封装。
- 操作系统: Linux

## 文件夹指路
### doc
- opus，ogg知识参考文档
### lib 依赖库
- opus: 1.5.2
- ogg: 1.3.5
### cpp
- cpp 版本
- cpp/opus: opus编解码操作，采用自定义封装
- cpp/opus-ogg: opus编解码操作，采用ogg封装

### golang-cgo
- golang版本，通过cgo调用c动态库
- golang-cgo/opus: opus编解码操作，采用自定义封装
- golang-cgo/opus-dlopen: opus编解码操作，采用自定义封装，c通过dlopen引入第三方库
- golang-cgo/opus-ogg: opus编解码操作，采用ogg封装

## TODO
1. 规范错误码