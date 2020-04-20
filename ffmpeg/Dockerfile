### ffmpeg
FROM jrottenberg/ffmpeg:4.2-alpine311 AS ffmpeg

### Release Stage
FROM node:12.14.1-alpine3.11 AS release

COPY --from=ffmpeg /usr/local /usr/local

# 设置时区
RUN sed -i 's?http://dl-cdn.alpinelinux.org/?https://mirrors.aliyun.com/?' /etc/apk/repositories && \
  apk add -U tzdata && \
  cp /usr/share/zoneinfo/Asia/Shanghai /etc/localtime && \
  apk del tzdata

RUN apk add --no-cache --update libgomp expat

RUN npm install cnpm -g

WORKDIR /home/node/app

COPY . .

RUN cnpm i

CMD [ "node", "server" ]