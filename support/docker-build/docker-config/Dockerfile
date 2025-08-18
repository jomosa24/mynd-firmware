FROM alpine:3.20

RUN apk --update --no-cache add make cmake git git-lfs ninja openssl protobuf libc6-compat libstdc++ ca-certificates && \
    apk --update --no-cache add --virtual build-dependencies w3m wget curl bzip2 && \
    wget --no-check-certificate -O /tmp/gcc-arm-none-eabi.tar.bz2 https://developer.arm.com/-/media/Files/downloads/gnu/13.2.rel1/binrel/arm-gnu-toolchain-13.2.rel1-x86_64-arm-none-eabi.tar.bz2 && \
    tar xf /tmp/gcc-arm-none-eabi.tar.bz2 --strip-components=1 -C /usr/local && \
    rm -rf /tmp/gcc-arm-none-eabi.tar.bz2 && \
    rm -rf /usr/local/share/doc && \
    apk del build-dependencies
RUN apk add --no-cache python3 py3-pip py3-protobuf py3-crcmod py3-intelhex
