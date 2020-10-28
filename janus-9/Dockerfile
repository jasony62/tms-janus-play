FROM jrottenberg/ffmpeg:4.2.3-centos7

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
RUN cd /root && wget https://github.com/libnice/libnice/archive/0.1.16.tar.gz && \
    tar xzfv 0.1.16.tar.gz && \
    cd libnice-0.1.16 && \
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

# 插件Streaming播放外部媒体
RUN yum -y install gstreamer*

# 安装Janus
WORKDIR /usr/src/janus-gateway

COPY ./janus-gateway-0.9.1 ./

RUN cd /usr/src/janus-gateway && \
    ./autogen.sh && \
    ./configure --prefix=/opt/janus && \
    make && make install && make configs 

# 安装janus(v0.9.1)
# 下载
# RUN cd /root && git clone https://github.com/meetecho/janus-gateway.git
# 编译和安装
# RUN cd /root/janus-gateway && \
#     git checkout v0.9.1 && \
#     ./autogen.sh && \
#     ./configure --prefix=/opt/janus && \
#     make && make install && make configs 
# RUN cd /root/janus-gateway && \
#     git checkout v0.8.2 && \
#     ./autogen.sh && \
#     ./configure --prefix=/opt/janus && \
#     make && make install && make configs 


# RUN cd /root && wget --tries=0 https://github.com/meetecho/janus-gateway/archive/v0.9.1.tar.gz && \
#     tar zxfv v0.9.1.tar.gz
# RUN cd /root/janus-gateway-0.9.1 && \
#     ./autogen.sh && \
#     ./configure --prefix=/opt/janus && \
#     make && make install && make configs 

# 安装mp4播放插件

# 安装Rtprx插件
# ADD plugins /root/janus/plugins
# RUN cd /root/janus/plugins/rtprx && \
#     ./bootstrap && \
#     ./configure --prefix=/opt/janus && \ 
#     make && make install 

# RUN cd /root/janus/plugins/rtprx && \
#     cp janus.plugin.tms.rtprx.jcfg.template /opt/janus/etc/janus/

EXPOSE 8088 8089

# janus服务环境变量
ENV ssl_certificate="" ssl_certificate_key="" stun_server="" debug_level=4 

ADD start_janus.sh /root/start_janus.sh
RUN chmod +x /root/start_janus.sh

ENTRYPOINT ["/root/start_janus.sh"]