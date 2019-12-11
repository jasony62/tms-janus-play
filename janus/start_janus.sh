#!/bin/bash

echo "启动 Janus server"

if [ "$ssl_certificate" != "" -a "$ssl_certificate_key" = "" ]
then
    echo "启用 Janus ssl 端口"
    sed -i "s/https = false/https = true/g" /opt/janus/etc/janus/janus.transport.http.jcfg
    sed -i "s/#secure_port = 8089/secure_port = 8089/g" /opt/janus/etc/janus/janus.transport.http.jcfg
    sed -i "s/#cert_pem = \"\/path\/to\/cert.pem\"/cert_pem = $ssl_certificate/g" /opt/janus/etc/janus/janus.transport.http.jcfg
    sed -i "s/#cert_key = \"\/path\/to\/key.pem\"/cert_key = $ssl_certificate_key/g" /opt/janus/etc/janus/janus.transport.http.jcfg
fi

if [ "$stun_server" != "" ]
then
    p_stun_server="--stun-server=$stun_server"
fi

/opt/janus/bin/janus $p_stun_server 
