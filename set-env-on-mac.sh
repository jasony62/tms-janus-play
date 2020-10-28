# 获取本机IP地址
HOST_IP=$(ifconfig en0 | awk '/inet / { print $2 }')

echo '获得IP地址：'$HOST_IP

# 修改/etc/hosts文件
sed -i "" "s/^.*host.tms.janus/${HOST_IP} host.tms.janus/g" /etc/hosts
