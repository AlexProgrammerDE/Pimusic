#!/bin/sh
echo "Pi-Soundplayer"
# Get updates
echo "Updating System"
sudo apt-get update
sudo apt-get upgrade
# Install needed packages
echo "Getting needed Packages"
sudo apt-get -y install gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gir1.2-gstreamer-1.0 gir1.2-gst-plugins-base-1.0 python-gst-1.0 build-essential python-dev python-pip curl alsa-base alsa-utils bluealsa bluez bluez-firmware python-gobject python-dbus mpg123apt-transport-https uild-essential git xmltoman autoconf automake libtool libpopt-dev libconfig-dev libasound2-dev avahi-daemon libavahi-client-dev libssl-dev libsoxr-dev

# Add repo and its GPG key
curl -sSL https://dtcooper.github.io/raspotify/key.asc | sudo apt-key add -v -
echo 'deb https://dtcooper.github.io/raspotify raspotify main' | sudo tee /etc/apt/sources.list.d/raspotify.list

# Install package
echo "Installing Spotify Support"
sudo apt-get -y install raspotify

# Script for Airplay
echo "Installing Airplay Support"
git clone https://github.com/mikebrady/shairport-sync.git
cd shairport-sync

autoreconf -fi
./configure --sysconfdir=/etc --with-alsa --with-soxr --with-avahi --with-ssl=openssl --with-systemd
make
sudo make install
systemctl enable shairport-sync
systemctl start shairport-sync

#Mopidy
echo "Installing Mopidy Support"
sudo pip install -U mopidy

# Bluetooth script
echo "Installing Bluetooth Support"
git clone https://github.com/AdityaTelange/rpi-bluetooth-audio-player
cd rpi-bluetooth-audio-player

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" 
   exit 1
else
	echo "installing Bluetooth Music Player"
	export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/dbus/system_bus_socket
	export UDEV=1

	# Copy sounds
	echo "copying files"
	cp -r ./bluetooth-player/sounds/ /usr/src/sounds/

	# Setup udev rules - this lets us play the connect/disconnect sound
	# and also turn off discover/pairing when a client is connected
	cp -r ./bluetooth-player/bluetooth-udev /usr/src/
	chmod +x /usr/src/bluetooth-udev
	cp -r ./bluetooth-player/udev-rules/ /etc/udev/rules.d/

	# Bluetooth-agent handles the auth of devices
	cp -r ./bluetooth-player/bluetooth-agent /usr/src/
	chmod +x /usr/src/bluetooth-agent

	cp -r ./bluetooth-player/start.sh /usr/src/
	chmod +x /usr/src/start.sh

	echo "installing bluetooth-player service"
	sudo ln -sf /usr/src/start.sh /usr/local/bin/start_bt_player
	cp ./bluetooth-player/bluetooth-player.service /etc/systemd/system/

	echo "done !!!"
	echo "starting bt player"
	systemctl start bluetooth-player.service

fi
