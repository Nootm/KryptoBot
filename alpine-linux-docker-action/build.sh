#!/bin/sh -l

apk update
apk upgrade
apk add build-base clang llvm llvm13-dev websocket++ curl-dev jsoncpp-dev openssl-dev boost-dev glfw-wayland-dev pkgconfig make git wget
wget https://github.com/CrowCpp/Crow/releases/download/v1.0%2B5/crow_all.h
mv ./crow_all.h /usr/include/crow.h
git clone https://github.com/Nootm/KryptoBot.git
cd KryptoBot
wget https://github.com/ocornut/imgui/archive/refs/tags/v1.88.tar.gz
tar xf v1.88.tar.gz
mv imgui-1.88 imgui
make
cp ./*.out /github/workspace/
