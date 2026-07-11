FROM ubuntu:22.04

RUN apt-get update && DEBIAN_FRONTEND=noninteractive apt-get install -y apt-utils sudo locales vim git iputils-ping net-tools rename openssh-server \
	software-properties-common \
	build-essential clang libc++-dev cmake \
	libncurses6 \
	python3 python3-pip ruby zstd libzstd-dev wget curl \
	fuse \
	automake autoconf shtool libtool \
	doxygen graphviz \
	nkf universal-ctags \
	qtbase5-dev libqt5svg5-dev \
	qt6-base-dev libqt6svg6-dev libglu1-mesa-dev libbsd-dev libssl-dev \
	libcurl4-openssl-dev \
	mecab-utils \
	&& rm -rf /var/lib/apt/lists/*

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

COPY ./cmd /tmp/
CMD ["/bin/sh", "/tmp/cmd"]

