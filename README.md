# MetaHash InfrastructureTorrent node

This repository contains the InfrastructureTorrent node source code written in C++. There are two internal libraries: [libmicrohttpd2](https://github.com/metahashorg/libmicrohttpd2) and [libmhsupport](https://github.com/metahashorg/libmhsupport) used in this code. 

**Pre-built binary for Ubuntu 18 (20 is also suitable) available here: [releases](https://github.com/metahashorg/Node-InfrastructureTorrent/releases).**

## Requirements
```shell
cmake > 3.8
gcc > 8.0
libevent 2.1.8
```

## Build

Please be informed that you can build torrent in /tmp (temporary directory) as described below or you can build it in /opt directory.

Please follow these steps to build and run Torrent on Ubuntu:
1. Preparation
```shell

sudo apt install software-properties-common
sudo add-apt-repository ppa:ubuntu-toolchain-r/test
sudo apt update

sudo apt install -y libgoogle-perftools4 libgmp10 liburiparser1 libcurl4 gcc g++ liburiparser-dev libssl-dev libevent-dev git automake libtool make cmake libcurl4-openssl-dev libcrypto++-dev libgnutls28-dev libgcrypt20-dev libgoogle-perftools-dev g++-8 gcc-8 cpp-8 g++-8 gcc-8 gcc-8-locales g++-8-multilib gcc-8-doc gcc-8-multilib libstdc++-8-doc git curl wget automake libtool texinfo make libgmp-dev libcurl4-openssl-dev libgcrypt11-dev libgnutls28-dev libboost-dev libre2-dev libgoogle-perftools-dev libcurl4 libgoogle-perftools4 liburiparser1

sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-8 80 --slave /usr/bin/g++ g++ /usr/bin/g++-8 --slave /usr/bin/gcov gcov /usr/bin/gcov-8
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
make -j$(nproc)
sudo make install 
```
3. Get and compile libmicrohttpd2

Please note: you must use this libmicrohttpd2 library, because the original libmicrohttpd library has no all functions available which are necessary for running Torrent.
```shell
cd /tmp
git clone https://github.com/metahashorg/libmicrohttpd2
cd libmicrohttpd2
./bootstrap
./configure
make -j24
sudo make install
```
4. Get and compile libmhsupport
```shell
cd /tmp
git clone https://github.com/metahashorg/libmhsupport
cd libmhsupport/build
./build.sh
sudo make install
```
5. Submodule update and Build Torrent Node
```shell
cd /tmp
git clone https://github.com/metahashorg/Node-InfrastructureTorrent torrent_node
cd torrent_node/build
git submodule update --init
cmake ..
make
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
make -j$(nproc)
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

## Run torrent

To run torrent, you have to add the file containing your private key to the folder containing torrent (`/opt/torrent`). The file’s name must be as follows: `yourkey.raw.prv`. Сlick [here](https://metahash.readme.io/docs/server-setup-faq#section-where-to-get-the-private-keys-proxy_key) for information about where to get the private key. 

Example: `0x007ff2d508be12392be5a381ca07b55c8fd02d09192a9b5cf4.raw.prv`

Besides, you have to configure your config file (`/opt/torrent/config.conf`). To do it, add the row with the name of your file containing private key or, if the row `sign_key = ...` already exists, replace the private key with your key in it.

Example: `sign_key = "0x007ff2d508be12392be5a381ca07b55c8fd02d09192a9b5cf4"`.

