建立 WebRTC 连接

分配接收 RTP 的视频和音频端口。

将 RTP 包转发给 WebRTC 连接。

插件名称：janus.plugin.tms.rtprx

| 命令           | 说明       |     |
| -------------- | ---------- | --- |
| create.webrtc  |            |     |
| prepare.source | 准备播放源 |     |
| destroy.source | 销毁播放源 |     |

| 事件      | 说明           |     |
| --------- | -------------- | --- |
| mounted   | 返回播放源端口 |     |
| unmounted | 完成销毁播放源 |     |

| 参数                |                   |                |
| ------------------- | ----------------- | -------------- |
| status              | 等于`mounted`     | 完成播放源准备 |
| ports               | 接收 RTP 包的端口 |                |
| ports.audioport     |                   |                |
| ports.audiortcpport |                   |                |
| ports.videoport     |                   |                |
| ports.videortcpport |                   |                |

建立和 janus 的会话连接，webrtc 连接

会话（session）

播放源（mountpoint）

浏览器本地完成插件初始化（initialized）

和 janus 后台服务建立会话（connected）

将插件绑定到会话上（attached）。（这时就可以向服务发送命令了。一个命令是建立 webrtc 通道，一个命令是建立接收端口）

向服务端发出建立 webrtc 请求（异步）。进入建立 webrtc 链接阶段。

服务端返回 sdp

客户端 createAnswer。new RTCPeerConnection（pluginHandle.webrtcStuff.pc）。指定了 iceServers 作为参数。setRemoteDescription setLocalDescription

oniceconnectionstatechange 和 iceServer 之间的连接情况

onicecandidate 将 candidate 通过 sendTrickleCandidate 发送给 janus。会并行发送多个 candidate

ontrace 回调 onremotestream

track.onended track.onmute trace.onunmute

setRemoteDescription 回调 createAnswer

发送本地 sdp 给 janus，等待 webrtcup 事件

获得远程媒体并绑定（RemoteStreamAttached）。

发起命令建立接收 RTP 包端口。
