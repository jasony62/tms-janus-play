FROM centos:centos7

RUN yum -y update && yum -y install epel-release && \
    yum -y install gcc automake autoconf libtool make git wget \
    libmicrohttpd-devel jansson-devel \
    openssl-devel libsrtp-devel sofia-sip-devel glib2-devel \
    opus-devel libogg-devel libcurl-devel pkgconfig gengetopt \
    libconfig-devel \
    gtk-doc


# 更新curl>7.62
RUN rpm -Uvh  http://www.city-fan.org/ftp/contrib/yum-repo/rhel6/x86_64/city-fan.org-release-2-1.rhel6.noarch.rpm && \
    yum --showduplicates list curl --disablerepo="*" --enablerepo="city*" && \
    sed -i '5s/enabled=0/enabled=1/' /etc/yum.repos.d/city-fan.org.repo && \
    yum -y install curl

# 安装nice
RUN cd /root && git clone https://github.com/libnice/libnice.git && \
    cd libnice && \
    ./autogen.sh && \
    ./configure --prefix=/usr && \
    make && make install

# 安装srtp
RUN cd /root && wget https://github.com/cisco/libsrtp/archive/v2.2.0.tar.gz && \
    tar xfv v2.2.0.tar.gz && \
    cd libsrtp-2.2.0 && \
    ./configure --prefix=/usr --enable-openssl && \
    make shared_library && make install

# 找到nice包
ENV PKG_CONFIG_PATH /usr/lib/pkgconfig

# 安装janus
RUN cd /root && git clone https://github.com/meetecho/janus-gateway.git && \
    cd janus-gateway && \
    ./autogen.sh && \
    ./configure --prefix=/opt/janus && \
    make && make install && make configs 

RUN yum -y install gstreamer*

EXPOSE 8088 8089 5002 5004

ENV ssl_certificate="/etc/letsencrypt/live/back.ytxytx.cn/fullchain.pem" ssl_certificate_key="/etc/letsencrypt/live/back.ytxytx.cn/privkey.pem"

RUN sed -i "s/https = false/https = true/g" /opt/janus/etc/janus/janus.transport.http.jcfg
RUN sed -i "s/#secure_port = 8089/secure_port = 8089/g" /opt/janus/etc/janus/janus.transport.http.jcfg
RUN sed -i "s/#cert_pem = \"\/path\/to\/cert.pem\"/cert_pem = ${ssl_certificate}/g" /opt/janus/etc/janus/janus.transport.http.jcfg
RUN sed -i "s/#cert_key = \"\/path\/to\/key.pem\"/cert_key = ${ssl_certificate}/g" /opt/janus/etc/janus/janus.transport.http.jcfg

CMD ["--stun-server=stun.stunprotocol.org:3478"]

ENTRYPOINT ["/opt/janus/bin/janus"]