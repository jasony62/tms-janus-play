#!/bin/sh

echo "启动 Nginx server for tms-janus-player"

envsubst '$NGINX_WEB_BASE_URL' < /etc/nginx/nginx.conf.template > /etc/nginx/nginx.conf && exec nginx -g 'daemon off;'
