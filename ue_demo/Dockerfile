FROM nginx:alpine

ENV ssl_certificate="" ssl_certificate_key=""

ADD nginx.conf.template /etc/nginx/nginx.conf.template

ADD start_nginx.sh start_nginx.sh
RUN chmod +x start_nginx.sh

CMD ["./start_nginx.sh"]