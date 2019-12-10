#!/bin/sh

echo "启动 Nginx server"

if [ "$server_name" = "" ]
then
    echo "没有指定参数[server_name]"
    sed -i "s/server_name/#server_name/" /etc/nginx/nginx.conf.template
fi

if [ "$ssl_certificate" = "" ]
then
    echo "没有指定参数[ssl_certificate]"
    sed -i "s/ssl_certificate/#ssl_certificate/" /etc/nginx/nginx.conf.template
fi

if [ "$ssl_certificate_key" = "" ]
then
    echo "没有指定参数[ssl_certificate_key]"
    sed -i "s/ssl_certificate_key/#ssl_certificate_key/" /etc/nginx/nginx.conf.template
fi

envsubst < /etc/nginx/nginx.conf.template > /etc/nginx/nginx.conf && exec nginx -g 'daemon off;'