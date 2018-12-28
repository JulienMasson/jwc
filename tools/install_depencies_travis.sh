#!/bin/bash

# set -e

# env
export LD_LIBRARY_PATH="/usr/lib/:$LD_LIBRARY_PATH"
export PKG_CONFIG_PATH="/usr/lib/pkgconfig/:$PKG_CONFIG_PATH"

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

# # glib version 2.56.1
# git clone https://gitlab.gnome.org/GNOME/glib.git
# pushd glib
# git checkout 2.56.1
# autotools_install
# popd

# gobject-introspection version 1.54.1
# git clone https://gitlab.gnome.org/GNOME/gobject-introspection.git
# pushd gobject-introspection
# git checkout 1.54.1
# autotools_install
# popd

# # harfbuzz version 2.3.0
# git clone https://github.com/harfbuzz/harfbuzz.git
# pushd harfbuzz
# git checkout 2.3.0
# autotools_install
# popd

# # pango version 1.41.0
# git clone https://gitlab.gnome.org/GNOME/pango.git
# pushd pango
# git checkout 1.41.0
# autotools_install
# popd

# # libepoxy version 1.5.3
# git clone https://github.com/anholt/libepoxy.git
# pushd libepoxy
# git checkout 1.5.3
# autotools_install
# popd

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
