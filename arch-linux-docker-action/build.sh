#!/bin/sh -l

pacman --noconfirm -Sy
pacman --noconfirm -S base-devel git boost glfw-wayland cmake clang wget
git clone https://aur.archlinux.org/yay.git
cd yay
useradd -m -g wheel noot
cd /home/noot
sudo -u noot git clone https://aur.archlinux.org/crow.git
cd crow
sudo -u noot makepkg
pacman --noconfirm -U ./crow-*.pkg.tar.zst
cd ..
sudo -u noot git clone https://aur.archlinux.org/imgui.git
cd imgui
sudo -u noot makepkg
pacman --noconfirm -U ./imgui-*.pkg.tar.zst
cd ..
sudo -u noot git clone https://aur.archlinux.org/websocketpp-git.git
cd websocketpp-git
sudo -u noot makepkg
pacman --noconfirm -U ./websocketpp-*.pkg.tar.zst
cd ~
git clone https://github.com/Nootm/KryptoBot.git
cd KryptoBot
wget https://github.com/ocornut/imgui/archive/refs/tags/v1.88.tar.gz
tar xf v1.88.tar.gz
mv imgui-1.88 imgui
make
