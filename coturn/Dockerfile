FROM ubuntu:16.04

RUN apt-get update && apt-get install -y coturn && apt-get clean && rm -rf /var/lib/apt/lists/* /tmp/* /var/tmp/*

ENV TURN_PORT 3478
ENV TURN_PORT_START 10000
ENV TURN_PORT_END 20000
ENV TURN_SECRET tmssecret
ENV TURN_SERVER_NAME coturn
ENV TURN_REALM north.gov

ADD start_coturn.sh start_coturn.sh
RUN chmod +x start_coturn.sh

CMD ["./start_coturn.sh"]