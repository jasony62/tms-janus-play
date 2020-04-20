#!/bin/sh

echo "启动 Nginx server"

if [ "$ssl_certificate" != "" -a "$ssl_certificate_key" != "" ]
then
    echo "启用 Nginx ssl 端口"
    sed -i "s/#ssl_server//" /etc/nginx/nginx.conf.template
fi

envsubst '$ssl_certificate $ssl_certificate_key' < /etc/nginx/nginx.conf.template > /etc/nginx/nginx.conf && exec nginx -g 'daemon off;'