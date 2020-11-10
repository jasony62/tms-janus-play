基于`janus-gateway`实现的流媒体服务器，用于学习和验证`janus`的功能。

`ue_demo`是`janus`自带的演示程序。`ue_play`是媒体播放端，对接了`mp4`插件。

播放端（ue_play）和`janus`服务通过 http 端口（8088）或 https 端口 （8089）服务建立 WebRTC 链接。

# 环境准备

项目目录下新建`docker-compose.override.yml`文件。

在 linux 环境下，服务 janus 应使用的网络模式为`host`，否则会报错（和 mDNS 有关，目前不知如何怎样解决）。但是，在 Mac 和 Windows 环境下不支持`host`模式。

因为浏览器使用 WebRTC 默认要使用 https，所以最好安装 ssl 证书。服务器生成好 ssl 证书后，janue 和 nginx 需要开启 https 端口，需要将存放证书的目录挂载到容器中。

如果需要开启 stun 和 ssl，需要给环境变量赋值。新建文件`local.env`（可以根据需要命名），指定使用这个文件。

```
# ssl证书位置
ssl_certificate=
ssl_certificate_key=

# janus stun-server
stun_server=coturn:3478

# debug级别
debug_level=4
```

`stun_server`设置为 docker 中的`coturn`（使用部署位置的公网地址），或者公共的服务地址，例如：stun.stunprotocol.org:3478

## ssl

webrtc 要求通过 ssl 访问。

## coturn

为了在互联网上实现点对点通信，需要支持穿越 Nat，配置自己的`stun`服务。

在有公网 ip 的服务器上，用下面的命令行启动`instrumentisto/coturn`容器。

> docker run --name coturn-test --network=host instrumentisto/coturn

参考：

https://github.com/coturn/coturn

https://github.com/instrumentisto/coturn-docker-image

# 制作镜像

下载`https://github.com/meetecho/janus-gateway/archive/v0.9.1.tar.gz`文件到`janus-9`目录下，执行命令`tar -zxf v0.9.1.tar.gz`，解压后的目录为`janus-gateway-0.9.1`。

> docker-compose -f docker-compose.9.yml build

> docker-compose -f docker-compose.9.yml -f docker-compose.override.yml up

> docker exec -it tms-janus_0.9.1 bash

> docker-compose -f docker-compose.9.yml down

> docker-compose -f docker-compose.9.yml -f docker-compose.override.yml up janus

> docker exec -it tms-janus_0.9.1 bash -c "cd /usr/src/janus-plugins/play; make"

> docker exec -it tms-janus_0.9.1 bash -c "cd /usr/src/janus-plugins/play; make install"

> docker exec -it tms-janus_0.9.1 bash -c "cd ../janus-plugins/play; make; make install"

自定义插件复制到/usr/src/janus-plugins

编译插件

进入容器执行

./bootstrap && \
./configure --prefix=/opt/janus && \
make && make install

配置文件

> 编译安装后要重启 janus。如果是 down，插件需要编译和安装。

> docker-compose -f docker-compose.9.yml restart janus

# janus

Janus-gateway 是开源的 WebRTC 服务器。

参考：https://janus.conf.meetecho.com

| 变量 | 说明 | 默认值 |
| ---- | ---- | ------ |
|      |      |        |
|      |      |        |
|      |      |        |
|      |      |        |

| 命令       | 说明                                                            |
| ---------- | --------------------------------------------------------------- |
| probe.file | 获取文件信息。                                                  |
| ctrl.play  | 第 1 次执行，开始播放；播放过程中执行，暂停；暂停时执行，恢复。 |
| stop.play  | 停止播放。                                                      |

必须停止当前播放的文件，才能播放新文件。

# 播放端（ue_play）

在 nginx 中运行控制媒体播放的前端代码。

默认`janus`服务和播放端在同一服务器上，使用 8088 作为`janus`服务的 http 端口，使用 8089 作为 https 端口。

可以通过环境变量 VUE_APP_JANUS_HTTP_SERVER 直接指定 janus。

环境变量

| 变量                     | 说明                                                        | 默认值 |
| ------------------------ | ----------------------------------------------------------- | ------ |
| VUE_APP_JANUS_ADDRESS    | `janus`服务的地址。如果不指定，认为和播放端部署在同一主机。 | 无     |
| VUE_APP_JANUS_HTTP_PORT  | `janus`服务 http 服务端口。与浏览器地址栏的协议一致。       | 8088   |
| VUE_APP_JANUS_HTTPS_PORT | `janus`服务 https 服务端口。与浏览器地址栏的协议一致。      | 8089   |
|                          |                                                             |        |

发布 npm 包。

通道状态

| 状态        | 说明                                           |     |
| ----------- | ---------------------------------------------- | --- |
| initialized | 完成 Janus 客户端检查和适配等初始化工作。      |     |
| connected   | 已经连接 Janus 服务器，生成了 janus 会话实例。 |     |
| attached    | 已经连接到指定插件，可以开始发送消息。         |     |
| webrtcUp    | WebRTC 通道已建立，可以传送媒体数据。          |     |

播放状态

| 状态   | 说明                                 |     |
| ------ | ------------------------------------ | --- |
| ready  | 通道已经就绪，要播放的文件检查通过。 |     |
| going  | 播放中。播放结束后回到`ready`状态。  |     |
| paused | 播放暂停。                           |     |

需要执行

> cnpm i vue

> cnpm i tms-vue@0.0.9

# ue-demo

janus-gateway 自带的演示程序。

# 运行

> docker-compose up --build

## 样本数据

mp4

> ffmpeg -t 0:3.40 -i digit-4_14s.wav -t 0:3.40 -i testsrc2-baseline31-gop10-3_80s.h264 -ar 8000 -c:a aac -c:v libx264 -profile:v baseline -level 3.1 -g 10 digit_8k_testsrc2-baseline31-gop10-3_40s.mp4

## player

在浏览器中打开：https://yourdomain:8443/player

## demo

在浏览器中打开：https://yourdomain:8444

因为 ssl 的问题需要手工执行一遍对 api 的调用。
