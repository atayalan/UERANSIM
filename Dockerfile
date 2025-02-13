FROM ubuntu:20.04 
RUN apt update && apt install ethtool jq curl iproute2 libsctp1 -y
RUN mkdir -p /opt/ueransim
ADD build /opt/ueransim

ENTRYPOINT ["/bin/bash", "-ce", "sleep infinity"]
