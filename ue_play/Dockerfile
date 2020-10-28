# 标准基础镜像（构建阶段）
FROM node:alpine  AS build

RUN npm install cnpm -g

WORKDIR /home/node/app

# 安装依赖包
COPY package.json package.json

RUN cnpm i

# 生成.env文件
ARG vue_app_title
ARG vue_app_base_url
ARG vue_app_janus_address
ARG vue_app_janus_http_port
ARG vue_app_janus_https_port
ARG vue_app_ffmpeg_protocol
ARG vue_app_ffmpeg_hostname
ARG vue_app_ffmpeg_api_port
ARG vue_app_ffmpeg_api_path
ARG vue_app_ffmpeg_push_port
ARG vue_app_ffmpeg_rtp_target
ARG vue_app_ffmpeg_vcodec

RUN echo VUE_APP_TITLE=$vue_app_title > .env && \
  echo VUE_APP_BASE_URL=$vue_app_base_url >> .env && \
  echo VUE_APP_JANUS_ADDRESS=$vue_app_janus_address >> .env && \
  echo VUE_APP_JANUS_HTTP_PORT=$vue_app_janus_http_port >> .env && \
  echo VUE_APP_JANUS_HTTPS_PORT=$vue_app_janus_https_port >> .env && \
  echo VUE_APP_FFMPEG_PROTOCOL=$vue_app_ffmpeg_protocol >> .env && \
  echo VUE_APP_FFMPEG_HOSTNAME=$vue_app_ffmpeg_hostname >> .env && \
  echo VUE_APP_FFMPEG_API_PORT=$vue_app_ffmpeg_api_port >> .env && \
  echo VUE_APP_FFMPEG_API_PATH=$vue_app_ffmpeg_api_path >> .env && \
  echo VUE_APP_FFMPEG_PUSH_PORT=$vue_app_ffmpeg_push_port >> .env && \
  echo VUE_APP_FFMPEG_RTP_TARGET=$vue_app_ffmpeg_rtp_target >> .env && \
  echo VUE_APP_FFMPEG_VCODEC=$vue_app_ffmpeg_vcodec >> .env 

# 构建代码
COPY . . 

RUN yarn build

# 标准基础镜像（部署阶段）
FROM nginx:alpine as release

# 将构建阶段代码放在指定位置
COPY --from=build /home/node/app/dist /usr/share/nginx/html

# 设置时区
RUN sed -i 's?http://dl-cdn.alpinelinux.org/?https://mirrors.aliyun.com/?' /etc/apk/repositories && \
  apk add -U tzdata && \
  cp /usr/share/zoneinfo/Asia/Shanghai /etc/localtime && \
  apk del tzdata

ENV ssl_certificate="" ssl_certificate_key=""

# 修改配置文件
ADD ./nginx.conf.template /etc/nginx/nginx.conf.template

ADD ./start_nginx.sh /usr/local/bin/start_nginx.sh

RUN chmod +x /usr/local/bin/start_nginx.sh

CMD ["start_nginx.sh"]
