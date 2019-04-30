# MetaHash torrent node

This repository contains the torrent node source code written in C++. There are two internal libraries: [libmicrohttpd2](https://github.com/metahashorg/libmicrohttpd2) and [libmhsupport](https://github.com/metahashorg/libmhsupport) used in this code. 

**Pre-built binary for ubuntu 14/16/18 available here: [releases](https://github.com).**

## Requirements
```shell
cmake > 3.8
gcc > 8.0
libevent 2.1.8
```

## Build

Please follow these steps to build and run Torrent on Ubuntu 14.04 x64:
1. Preparation
```shell
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update

sudo apt install gcc-8 g++-8 liburiparser-dev libssl-dev libevent-dev git automake libtool texinfo make libgmp-dev
    
sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 60 --slave /usr/bin/g++ g++ /usr/bin/g++-8
sudo update-alternatives --config gcc
```
2. Get and compile latest cmake
```shell
cd /tmp
wget https://github.com/Kitware/CMake/releases/download/v3.13.0/cmake-3.13.0.tar.gz
tar zxfv cmake-3.13.0.tar.gz
cd cmake-3.13.0
./bootstrap
./configure
make
sudo make install 
```
3. Get and compile libevent
```shell
cd /tmp
wget https://github.com/libevent/libevent/releases/download/release-2.1.8-stable/libevent-2.1.8-stable.tar.gz
tar zxfv libevent-2.1.8-stable.tar.gz
cd libevent-2.1.8-stable
./configure
make
sudo make install
```
4. Get and compile libmicrohttpd2

Please note: you must use this libmicrohttpd2 library, because the original libmicrohttpd library has no all functions available which are necessary for running Torrent.
```shell
cd /tmp
git clone https://github.com/metahashorg/libmicrohttpd2
cd libmicrohttpd2
./bootstrap
./configure
make
sudo make install
```
5. Get and compile libmhsupport
```shell
cd /tmp
git clone https://github.com/metahashorg/libmhsupport
cd libmhsupport/build
./build.sh
sudo make install
```
6. Build Torrent Node
```shell
cd /tmp
git clone https://github.com/metahashorg/Node-InfrastructureTorrent
cd Node-InfrastructureTorrent/build
./build.sh
```

## Update torrent to recent version

1. Stop running torrent:
```shell
kill `ps axuwf|grep torrent_node|grep -v grep|awk '{print $2}'`
```
2. Go to directory where you've cloned torrent_node, for example

```shell
cd /tmp/torrent_node

git pull
cd build
rm -rf
cmake ..
make -j
```
3. Start torrent:
```shell
./torrent_node torrent_config
```

## Script for starting, stopping, restarting.
Script [torrent.sh](https://github.com/metahashorg/Node-InfrastructureTorrent/blob/master/torrent.sh) for the following operations with Metahash torrent application:
* starting, 
* stopping, 
* restarting,
* getting status.

Note: default workdir is `/opt/torrent`. If you’ve install torrent to another location please change workdir in script.

#### Usage script
RUN script as follows:
```shell
./torrent.sh status
```
