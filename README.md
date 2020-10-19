# tms-janus-streaming

基于`janus-gateway`实现的流媒体服务器。

播放端（ue_player）和`janus`服务通过 http 端口（8088）或 https 端口 （8089）服务建立 WebRTC 链接。

播放端（ue_player）和`ffmpeg`服务通过端口控制`ffmpeg`播放媒体，通过端口接收推送事件。

播放端（ue_player）将`janus`服务中用于接收`ffmpeg`服务发送的 RTP 包的地址和端口传递给`ffmpeg`服务。

## 制作镜像

下载：https://github.com/meetecho/janus-gateway/archive/v0.9.1.tar.gz

将文件放到`janus-9`目录下，执行命令`tar -zxf v0.9.1.tar.gz`，解压后的目录为`janus-gateway-0.9.1`。

> docker-compose -f docker-compose.9.yml build

> docker-compose -f docker-compose.9.yml -f docker-compose.override.yml up

> docker exec -it tms-janus_0.9.1 bash

自定义插件复制到/usr/src/janus-plugins

编译插件

./bootstrap && \
./configure --prefix=/opt/janus && \
make && make install

编译安装后要重启 janus

> docker-compose -f docker-compose.9.yml restart janus

## ssl

## coturn

为了在互联网上实现点对点通信，需要支持穿越 Nat，配置自己的 turn 服务器（coturn）。

参考：https://github.com/coturn/coturn

## janus

Janus-gateway 是开源的 WebRTC 服务器。

参考：https://janus.conf.meetecho.com

| 变量           | 说明 | 默认值 |
| -------------- | ---- | ------ |
| rtp_port_range |      |        |
| audiopt        |      |        |
| audiortpmap    |      |        |
| videopt        |      |        |
| videortpmap    |      |        |

## ffmpeg

用`ffmpeg`控制媒体文件播放。

在`tms-koa`框架中安装`tms-koa-ffmpeg`插件实现媒体播放。

| 变量                | 说明             | 默认值                        |
| ------------------- | ---------------- | ----------------------------- |
| ssl_certificate     | ssl 证书存放位置 | /usr/local/etc/ssl/server.crt |
| ssl_certificate_key | ssl 证书存放位置 | /usr/local/etc/ssl/server.key |

## 播放端（ue_player）

在 nginx 中运行控制媒体播放的前端代码。

默认`janus`服务和播放端在同一服务器上，使用 8088 作为`janus`服务的 http 端口，使用 8089 作为 https 端口。

可以通过环境变量 VUE_APP_JANUS_HTTP_SERVER 直接指定 janus。

环境变量

| 变量                      | 说明                                                        | 默认值 |
| ------------------------- | ----------------------------------------------------------- | ------ |
| VUE_APP_JANUS_ADDRESS     | `janus`服务的地址。如果不指定，认为和播放端部署在同一主机。 | 无     |
| VUE_APP_JANUS_HTTP_PORT   | `janus`服务 http 服务端口。与浏览器地址栏的协议一致。       | 8088   |
| VUE_APP_JANUS_HTTPS_PORT  | `janus`服务 https 服务端口。与浏览器地址栏的协议一致。      | 8089   |
|                           |                                                             |        |
| VUE_APP_FFMPEG_PROTOCOL   | `ffmpeg`服务协议（http/https）。                            | http   |
| VUE_APP_FFMPEG_HOSTNAME   | `ffmpeg`服务接口地址。                                      | 无     |
| VUE_APP_FFMPEG_API_PORT   | `ffmpeg`服务调用 api 端口。                                 | 3000   |
| VUE_APP_FFMPEG_API_PATH   | `ffmpeg`服务调用 api 入口路径。                             | ffmpeg |
| VUE_APP_FFMPEG_PUSH_PORT  | `ffmpeg`推送服务端口，接收`ffmpeg`推送事件。                | 3001   |
| VUE_APP_FFMPEG_RTP_TARGET | 接收`ffmpeg`发送的 RTP 流的地址                             | janus  |

默认情况下所有的模块都在 docker 中运行，`janus`服务用于接收`ffmpeg`服务发送的 RTP 包。

## ue-demo

janus-gateway 自带的演示程序。

# 环境准备

项目目录下新建`docker-compose.override.yml`文件。

```
version: '3.7'
services:
  coturn:
  # network_mode: 'host'

  janus:
    # network_mode: 'host'
    ports:
      - '8088:8088'
      - '8089:8089'
      - '5004:5004/udp'
      - '10000-10099:10000-10099/udp'
    volumes:
      - /Users/yangyue/ssl:/usr/local/etc/ssl
    env_file:
      - ./local.env

  ffmpeg:
    ports:
      - '3000:3000'
      - '3443:3443'
      - '3444:3444'
    volumes:
      - ./ffmpeg/files:/home/node/app/files
      - /Users/yangyue/ssl:/usr/local/etc/ssl
    env_file:
      - ./local.env

  ue_player:
    build:
      args:
        vue_app_janus_address: janus # 需要指定为janus服务的地址
        vue_app_ffmpeg_http_server: https://localhost:3443/ffmpeg
        vue_app_ffmpeg_push_server: https://localhost:3444
    volumes:
      - /Users/yangyue/ssl:/usr/local/etc/ssl
    env_file:
      - ./local.env
    ports:
      - '8080:80'
      - '8443:443'

  ue_demo:
    volumes:
      - /Users/yangyue/ssl:/usr/local/etc/ssl
    env_file:
      - ./local.env
    ports:
      - '8081:80'
      - '8444:443'
```

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

# 运行

> docker-compose up --build

## player

在浏览器中打开：https://yourdomain:8443/player

## demo

在浏览器中打开：https://yourdomain:8444
