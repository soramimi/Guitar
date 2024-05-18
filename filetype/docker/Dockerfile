FROM ubuntu:20.04
RUN apt-get update
RUN apt-get upgrade -y
RUN apt-get install -y --no-install-recommends apt-utils nano net-tools dnsutils ca-certificates
RUN apt-get update
RUN apt-get install -y --no-install-recommends build-essential git cmake ruby nasm
RUN apt-get update
RUN apt-get install -y --no-install-recommends automake autoconf shtool libtool

COPY cmd /root/
CMD /root/cmd
