FROM debian:latest
RUN dpkg --add-architecture i386 && \
    apt-get update && \
    apt-get install -y gcc-multilib make autoconf automake xsltproc docbook-xsl \
        libxpm-dev libxpm-dev:i386 libx11-dev libx11-dev:i386 git
