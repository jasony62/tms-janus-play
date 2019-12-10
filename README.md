# tms-janus-streaming

基于 janus-gateway 实现的流媒体服务器。

# nginx 支持 ssl

浏览器 webrtc 需要 https

Let's Encrypt

和 janus 共享证书

```
server {
    listen 443 ssl;
    server_name back.ytxytx.cn;
    ssl_certificate /etc/letsencrypt/live/back.ytxytx.cn/fullchain.pem;
    ssl_certificate_key /etc/letsencrypt/live/back.ytxytx.cn/privkey.pem;
    ssl_dhparam /etc/ssl/certs/dhparams.pem;
    ssl_ciphers 'ECDHE-ECDSA-CHACHA20-POLY1305:ECDHE-RSA-CHACHA20-POLY1305:ECDHE-ECDSA-AES128-GCM-SHA256:ECDHE-RSA-AES128-GCM-SHA256:ECDHE-ECDSA-AES256-GCM-SHA384:ECDHE-RSA-AES256-GCM-SHA384:DHE-RSA-AES128-GCM-SHA256:DHE-RSA-AES256-GCM-SHA384:ECDHE-ECDSA-AES128-SHA256:ECDHE-RSA-AES128-SHA256:ECDHE-ECDSA-AES128-SHA:ECDHE-RSA-AES256-SHA384:ECDHE-RSA-AES128-SHA:ECDHE-ECDSA-AES256-SHA384:ECDHE-ECDSA-AES256-SHA:ECDHE-RSA-AES256-SHA:DHE-RSA-AES128-SHA256:DHE-RSA-AES128-SHA:DHE-RSA-AES256-SHA256:DHE-RSA-AES256-SHA:ECDHE-ECDSA-DES-CBC3-SHA:ECDHE-RSA-DES-CBC3-SHA:EDH-RSA-DES-CBC3-SHA:AES128-GCM-SHA256:AES256-GCM-SHA384:AES128-SHA256:AES256-SHA256:AES128-SHA:AES256-SHA:DES-CBC3-SHA:!DSS';
    ssl_prefer_server_ciphers on;
    ssl_protocols TLSv1 TLSv1.1 TLSv1.2;
    location / {
        root /usr/share/nginx/html;
        index index.html index.htm;
    }
}
```

测试

参考：

https://letsencrypt.org

https://certbot.eff.org

# coturn 服务

测试

# janus
