#!/bin/sh

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root"
   exit 1
	else
  	clear
  	echo "Pimusic update"
	echo ""
	echo "Stoping services"
	sudo systemctl stop shairport-sync
	sudo systemctl stop bluetooth-player.service
	sudo systemctl stop mopidy
	sudo systemctl disable bluetooth-player.service
	sudo systemctl disable shairport-sync
	sudo systemctl disable mopidy
	
	clear
	echo "Updating System"
	echo ""
	sudo apt-get update
	sudo apt-get -y upgrade
	sudo apt -y autoremove
	# Install needed packages 
	echo "Updatingg needed Packages"
	echo ""
	sudo apt-get -y install wget gstreamer1.0-plugins-bad gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gir1.2-gstreamer-1.0 gir1.2-gst-plugins-base-1.0 python-gst-1.0 python-dev python-pip curl alsa-base alsa-utils bluealsa bluez bluez-firmware python-gobject python-dbus mpg123 autotools-dev apt-transport-https dh-autoreconf git xmltoman autoconf automake libtool libpopt-dev libconfig-dev libasound2-dev avahi-daemon libavahi-client-dev libssl-dev libsoxr-dev
	clear
	echo "Deleting directory service_data"
	sudo rm service_data
	mkdir service_data
	cd service_data
	
	clear
	echo "Updating Spotify Connect"
	echo ""
	sudo apt-get -y install raspotify
	
	clear
	echo "Updating Airplay"
	sudo apt-get -y shairport-sync
	
	clear
	echo "Updating bletooth support"
	cd ..
        rm -rf /usr/src/sounds/
	rm -rf /usr/src/bluetooth-udev
	rm -rf /etc/udev/rules.d/udev-rules/
	rm -rf /usr/src/bluetooth-agent
	rm -rf /usr/src/start.sh
	rm -rf /usr/local/bin/start_bt_player
	rm -rf /etc/systemd/system/bluetooth-player.service
	git clone https://github.com/AlexProgrammerDE/rpi-bluetooth-audio-player.git
	cd rpi-bluetooth-audio-player
	sudo bash install.sh
	echo""
	echo "done !!!" 
	echo ""
	echo "Starting Bluetooth Support"
	echo ""
	sudo systemctl enable bluetooth-player.service
	sudo systemctl start bluetooth-player.service
	clear
fi
