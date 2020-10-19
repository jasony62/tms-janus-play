#!/bin/bash

echo "启动 Janus server"

# echo "设置janus.plugin.tms.rtprx.jcfg"
# envsubst '$rtp_port_range $audiopt $audiortpmap $videopt $videortpmap' < /opt/janus/etc/janus/janus.plugin.tms.rtprx.jcfg.template > /opt/janus/etc/janus/janus.plugin.tms.rtprx.jcfg

echo "修改Janus配置文件"
sed -i "s/#disable = \"libjanus_rabbitmq.so\"/disable = \"libjanus_rabbitmq.so,libjanus_pfunix.so\"/g" /opt/janus/etc/janus/janus.jcfg
# sed -i "s/#disable = \"libjanus_voicemail.so,libjanus_recordplay.so\"/disable = \"libjanus_voicemail.so,libjanus_recordplay.so,libjanus_audiobridge.so,libjanus_videoroom.so,libjanus_videocall.so,libjanus_echotest.so,libjanus_nosip.so\"/g" /opt/janus/etc/janus/janus.jcfg

if [ "$ssl_certificate" != "" -a "$ssl_certificate_key" != "" ]
then
    echo "启用 Janus ssl 端口"
    sed -i "s/https = false/https = true/g" /opt/janus/etc/janus/janus.transport.http.jcfg
    sed -i "s/#secure_port = 8089/secure_port = 8089/g" /opt/janus/etc/janus/janus.transport.http.jcfg
    sed -i "s?#cert_pem = \"\/path\/to\/cert.pem\"?cert_pem = \"$ssl_certificate\"?g" /opt/janus/etc/janus/janus.transport.http.jcfg
    sed -i "s?#cert_key = \"\/path\/to\/key.pem\"?cert_key = \"$ssl_certificate_key\"?g" /opt/janus/etc/janus/janus.transport.http.jcfg
fi

if [ "$stun_server" != "" ]
then
    p_stun_server="--stun-server=$stun_server"
fi

p_debug_level="--debug-level=$debug_level"

/opt/janus/bin/janus  -F /opt/janus/etc/janus $p_stun_server $p_debug_level
