FROM debian:stretch

RUN apt-get -q update \
        && apt-get -qqy install \
                build-essential \
                zlib1g-dev \
                libpixman-1-dev \
                python \
                libglib2.0-dev \
                pkg-config \
                curl \
                jq \
                git \
        && rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/qemu

COPY . /usr/src/qemu

CMD ./build.sh