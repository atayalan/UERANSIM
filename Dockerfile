FROM ubuntu:20.04 
RUN apt update && apt install ethtool jq curl iproute2 -y
RUN mkdir -p /opt/ueransim
add build /opt/ueransim

ENTRYPOINT ["/bin/bash", "-ce", "sleep infinity"]
