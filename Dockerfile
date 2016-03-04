# Use this dockerfile to build the qemu binary

FROM debian:jessie

RUN apt-get update \
	&& apt-get install -y \
		autoconf \
		bison \
		build-essential \
		flex \
		libglib2.0-dev \
		libtool \
		make \
		pkg-config \
		python \
		zlib1g-dev

WORKDIR /usr/src/qemu

COPY . /usr/src/qemu

RUN ./configure --target-list=arm-linux-user --static

RUN make -j $(nproc)
