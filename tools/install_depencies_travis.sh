#!/bin/bash

set -e

# dir
pushd /home/travis/build/
mkdir deps && cd deps

# system packages
sudo apt-get update -qq
sudo apt-get install git python3 python3-pip ninja-build doxygen xmlto graphviz libudev-dev libmtdev-dev libevdev-dev libwacom-dev libgtk2.0-dev libgtk-3-dev libegl1-mesa-dev libgles2-mesa-dev libpixman-1-dev libxkbcommon-dev libgbm-dev libmount-dev gobject-introspection libgirepository1.0-dev libfribidi-dev libthai-dev gtk-doc-tools xutils-dev libglib2.0-dev libcairo2-dev libgdk-pixbuf2.0-dev libharfbuzz-dev libpango1.0-dev libepoxy-dev check valgrind libxcb1-dev libxcb-composite0-dev libxcb-xfixes0-dev libxcb-image0-dev libxcb-render0-dev libx11-xcb-dev libxcb-icccm4-dev libxcb-xkb-dev -y
sudo pip3 install --upgrade pip
sudo pip3 install setuptools sphinx
sudo pip3 install recommonmark
sudo pip3 install sphinx_rtd_theme

function meson_install {
    meson --prefix=/usr "$@" build
    ninja -j4 -C build
    sudo ninja -C build install
}

function autotools_install {
    ./autogen.sh --prefix=/usr "$@"
    make -j4
    sudo make install
}

# install meson version 0.48.1
git clone https://github.com/mesonbuild/meson.git
pushd meson
git checkout 0.48.1
sudo python3 setup.py install
popd

# install wayland
git clone https://gitlab.freedesktop.org/wayland/wayland
pushd wayland
autotools_install
popd

# install wayland-protocols
git clone https://anongit.freedesktop.org/git/wayland/wayland-protocols.git
pushd wayland-protocols
autotools_install
popd

# gtk version 3.20.0
git clone https://gitlab.gnome.org/GNOME/gtk.git
pushd gtk
git checkout 3.20.0
autotools_install --enable-wayland-backend --enable-x11-backend
popd

# install libinput
git clone https://gitlab.freedesktop.org/libinput/libinput
pushd libinput
meson_install
popd

# libxkbcommon version 0.8.2
git clone https://github.com/xkbcommon/libxkbcommon.git
pushd libxkbcommon
git checkout xkbcommon-0.8.2
meson_install
popd

# install wlroots
git clone https://github.com/swaywm/wlroots.git
pushd wlroots
meson_install
popd

# go back
popd
