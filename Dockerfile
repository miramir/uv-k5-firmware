FROM alpine:latest
WORKDIR /app
RUN apk add --no-cache --repository=https://dl-cdn.alpinelinux.org/alpine/edge/community gcc-arm-none-eabi newlib-arm-none-eabi \
   && apk add --no-cache git py3-pip py3-crcmod build-base
