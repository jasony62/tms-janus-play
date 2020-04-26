# 标准基础镜像（构建阶段）
FROM node:alpine

RUN npm install cnpm -g

WORKDIR /home/node/app

COPY . .

# 生成.env文件
ARG vue_app_base_url
ARG vue_app_ffmpeg_api_base
ARG vue_app_ffmpeg_push
ARG vue_app_janus_address

RUN echo VUE_APP_BASE_URL=$vue_app_base_url > .env
RUN echo VUE_APP_FFMPEG_API_BASE=$vue_app_ffmpeg_api_base >> .env
RUN echo VUE_APP_FFMPEG_PUSH=$vue_app_ffmpeg_push >> .env
RUN echo VUE_APP_JANUS_ADDRESS=$vue_app_janus_address >> .env

# 安装依赖包，构建代码
RUN cnpm i && yarn build

# 标准基础镜像（部署阶段）
FROM nginx:alpine

# 设置时区
RUN sed -i 's?http://dl-cdn.alpinelinux.org/?https://mirrors.aliyun.com/?' /etc/apk/repositories && \
  apk add -U tzdata && \
  cp /usr/share/zoneinfo/Asia/Shanghai /etc/localtime && \
  apk del tzdata

# 修改配置文件
ADD ./nginx.conf.template /etc/nginx/nginx.conf.template

ADD ./start_nginx.sh /usr/local/bin/start_nginx.sh

RUN chmod +x /usr/local/bin/start_nginx.sh

# 将构建阶段代码放在指定位置
COPY --from=0 /home/node/app/dist /usr/share/nginx/html

CMD ["start_nginx.sh"]
