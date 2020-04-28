# tms-janus-streaming

基于 janus-gateway 实现的流媒体服务器。

## coturn

为了解决点对点通信，需要支持穿越 Nat，配置自己的 turn 服务器（coturn）。

参考：https://github.com/coturn/coturn

## janus

Janus-gateway 是开源的 WebRTC 服务器。

参考：https://janus.conf.meetecho.com

## ffmpeg

用`ffmpeg`控制媒体文件播放。

使用了`tms-koa-ffmpeg`插件。

## ue_player

在 nginx 中运行前端代码。

## ue_demo

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
        vue_app_ffmpeg_api_base: https://localhost:3443/ffmpeg
        vue_app_ffmpeg_push: https://localhost:3444
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

在浏览器中打开：https://yourdomain:8080/player
