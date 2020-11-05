# 插件 api

| 命令          | 功能                   |     |
| ------------- | ---------------------- | --- |
| request.offer | 请求服务端创建 offer。 |     |
| play.file     | 播放指定文件。         |     |
| pause.file    | 暂停播放。             |     |
| resume.file   | 恢复播放。             |     |
| stop.file     | 停止播放。             |     |

播放文件时会启动一个播放线程，结束或停止播放时释放线程。

## play.file

指定文件参数。

# 事件

|             |                    |     |
| ----------- | ------------------ | --- |
| launch.play | 已经启动播放线程。 |     |
| exit.play   | 已经启动播放线程。 |     |

# docker

> docker exec -it tms-janus_0.9.1 bash -c "cd ../janus-plugins/mp4; make; make install"
