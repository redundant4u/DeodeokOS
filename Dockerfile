FROM ubuntu:18.04

RUN apt-get update
RUN apt-get install -y \
    gcc-multilib \
    g++-multilib \
    binutils \
    bison \
    flex \
    libc6-dev \
    libtool \
    make \
    patchutils \
    libgmp-dev \
    libmpfr-dev \
    libmpc-dev
RUN apt-get -y install nasm
RUN apt-get -y install qemu-kvm
RUN apt-get -y install vim

COPY bashrc /root/.bashrc

WORKDIR /home