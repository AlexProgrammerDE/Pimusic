#!/bin/sh

SOURCE_REPO="deb https://dtcooper.github.io/raspotify raspotify main"

# Install script for Raspotify. Adds the Debian repo and installs.
set -e

run_on_pi_only() {
    echo "Raspotify installer only runs on a Raspberry Pi"
    exit 1
}

if ! which apt-get apt-key > /dev/null; then
    run_on_pi_only
fi

# You probably have these
PREREQ_PACKAGES="curl apt-transport-https"
PREREQ_PACKAGES_TO_INSTALL=
for package in $PREREQ_PACKAGES; do
    if ! dpkg-query --show --showformat='${db:Status-Status}\n' "$package" 2> /dev/null | grep -q '^installed$'; then
        PREREQ_PACKAGES_TO_INSTALL="$package $PREREQ_PACKAGES_TO_INSTALL"
    fi
done

if [ "$PREREQ_PACKAGES_TO_INSTALL" ]; then
    sudo apt-get update
    sudo apt-get -y install $PREREQ_PACKAGES_TO_INSTALL
fi

# By popular demand, do softer checking for other OS versions
if uname -a | fgrep -ivq arm; then
    run_on_pi_only
fi

# Add public key to apt
curl -sSL https://dtcooper.github.io/raspotify/key.asc | sudo apt-key add -v -
echo "$SOURCE_REPO" | sudo tee /etc/apt/sources.list.d/raspotify.list

sudo apt-get update
sudo apt-get upgrade
sudo apt-get -y install raspotify
# Script for Airplay
sudo apt-get -y install uild-essential git xmltoman autoconf automake libtool libpopt-dev libconfig-dev libasound2-dev avahi-daemon libavahi-client-dev libssl-dev libsoxr-dev
git clone https://github.com/mikebrady/shairport-sync.git
cd shairport-sync
autoreconf -fi
./configure --sysconfdir=/etc --with-alsa --with-soxr --with-avahi --with-ssl=openssl --with-systemd
make
sudo make install
systemctl enable shairport-sync
systemctl start shairport-sync
