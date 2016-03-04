# Use this dockerfile to build the qemu binary

FROM debian:jessie

RUN apt-get update \
	&& apt-get install -y \
		autoconf \
		build-essential \
		libglib2.0-dev \
		make \
		pkg-config \
		python \
		zlib1g-dev

COPY . /usr/src/qemu

RUN ./configure --target-list=arm-linux-user --static

RUN make -j $(nproc)
