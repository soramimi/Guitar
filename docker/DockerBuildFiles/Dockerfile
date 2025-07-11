FROM ubuntu:22.04

RUN apt update
RUN DEBIAN_FRONTEND=noninteractive apt install -y apt-utils sudo locales vim git iputils-ping net-tools rename openssh-server
RUN apt update
RUN apt install -y software-properties-common
RUN apt install -y build-essential clang libc++-dev cmake
RUN apt install -y libncurses6
RUN apt install -y python3 python3-pip ruby zstd wget curl
RUN apt update

ARG UID
ARG GID
ARG UNAME
ARG GNAME
ARG HOMEDIR

RUN userdel ubuntu || true
RUN groupdel ubuntu || true
RUN groupadd -g ${GID} ${GNAME} && useradd -s /bin/bash -u ${UID} -g ${GID} ${UNAME} -M -d ${HOMEDIR}

COPY ./sudoers /etc/
COPY ./sshd_config /etc/ssh/
RUN mkdir -p /run/sshd

COPY ./locale.gen /etc
RUN locale-gen 
COPY ./default_locale /etc/default/locale
COPY ./environment /etc/

RUN apt install -y fuse
RUN apt install -y automake autoconf shtool libtool
RUN apt install -y doxygen graphviz
RUN apt install -y nkf universal-ctags

RUN apt install -y qtbase5-dev libqt5svg5-dev
RUN apt install -y qt6-base-dev libqt6svg6-dev libglu1-mesa-dev libbsd-dev libssl-dev

COPY ./cmd /tmp/
CMD ["/bin/sh", "/tmp/cmd"]

