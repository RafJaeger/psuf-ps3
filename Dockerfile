FROM --platform=linux/amd64 ubuntu:22.04

ENV PS3DEV=/ps3dev
ENV PSL1GHT=/ps3dev
ENV PATH="${PS3DEV}/bin:${PS3DEV}/ppu/bin:${PS3DEV}/spu/bin:${PATH}"

RUN apt-get update && apt-get install -y \
    curl \
    git \
    make \
    python2.7 \
    libelf1 \
    && rm -rf /var/lib/apt/lists/*

RUN curl -sL http://archive.ubuntu.com/ubuntu/pool/main/o/openssl/libssl1.1_1.1.1f-1ubuntu2_amd64.deb -o /tmp/libssl.deb && \
    dpkg -i /tmp/libssl.deb && \
    rm /tmp/libssl.deb

RUN mkdir -p /ps3dev && \
    curl -sL https://github.com/bucanero/ps3toolchain/releases/download/ubuntu-latest-fad3b5fb/ps3dev-ubuntu-latest-2020-08-31.tar.gz | tar xz -C /

WORKDIR /src

