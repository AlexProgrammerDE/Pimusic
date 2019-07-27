#!/bin/sh

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" | tee -a output.txt
   exit 1
	else
	echo "Pi-Soundplayer" | tee -a output.txt
	echo "Adding needed Keys." | tee -a output.txt
	# Spotify Key
	curl -sSL https://dtcooper.github.io/raspotify/key.asc | sudo apt-key add -v - | tee -a output.txt
	echo 'deb https://dtcooper.github.io/raspotify raspotify main' | sudo tee /etc/apt/sources.list.d/raspotify.list | tee -a output.txt
	
	# Mopidy Keys
	
	wget -q -O - https://apt.mopidy.com/mopidy.gpg | sudo apt-key add - | tee -a output.txt
	sudo wget -q -O /etc/apt/sources.list.d/mopidy.list https://apt.mopidy.com/stretch.list | tee -a output.txt
	
	# Get updates
	echo "Updating System" | tee -a output.txt
	sudo apt-get update | tee -a output.txt
	sudo apt-get -y upgrade | tee -a output.txt
	sudo apt -y autoremove | tee -a output.txt
	# Install needed packages 
	echo "Getting needed Packages" | tee -a output.txt
	sudo apt-get -y install wget gstreamer1.0-plugins-bad gstreamer1.0-tools gstreamer1.0-plugins-good gstreamer1.0-plugins-ugly gir1.2-gstreamer-1.0 gir1.2-gst-plugins-base-1.0 python-gst-1.0 python-dev python-pip curl alsa-base alsa-utils bluealsa bluez bluez-firmware python-gobject python-dbus mpg123 autotools-dev apt-transport-https dh-autoreconf git xmltoman autoconf automake libtool libpopt-dev libconfig-dev libasound2-dev avahi-daemon libavahi-client-dev libssl-dev libsoxr-dev | tee -a output.txt

	# Making directory for data.
	mkdir service_data | tee -a output.txt
	cd service_data | tee -a output.txt
	
	# Install package
	echo "Installing Spotify Support" | tee -a output.txt
	sudo apt-get -y install raspotify | tee -a output.txt

	# Script for Airplay
	echo "Installing Airplay Support" | tee -a output.txt
	git clone https://github.com/mikebrady/shairport-sync.git | tee -a output.txt
	cd shairport-sync | tee -a output.txt
	echo "Wait please. This can take some time." | tee -a output.txt
	autoreconf -fi | tee -a output.txt
	./configure --sysconfdir=/etc --with-alsa --with-soxr --with-avahi --with-ssl=openssl --with-systemd | tee -a output.txt
	make | tee -a output.txt
	sudo make install | tee -a output.txt
	systemctl enable shairport-sync | tee -a output.txt
	systemctl start shairport-sync | tee -a output.txt

	# Mopidy script 
	echo "Installing Mopidy Support" | tee -a output.txt
	sudo apt-get -y install mopidy | tee -a output.txt
	sudo systemctl enable mopidy | tee -a output.txt
	sudo systemctl start mopidy | tee -a output.txt
	# Mopidy Spotify
	sudo apt-get install -y mopidy-spotify | tee -a output.txt
	
	# Mopidy Soundcloud
	sudo apt-get install mopidy-soundcloud | tee -a output.txt
	
	# Mopidy Scrobbler
	pip install Mopidy-Scrobbler | tee -a output.txt
	
	# Mopidy Tunein
	pip install Mopidy-TuneIn | tee -a output.txt
	
	# Bluetooth script
	echo "Installing Bluetooth Support" | tee -a output.txt
	cd .. | tee -a output.txt
	git clone https://github.com/AlexProgrammerDE/rpi-bluetooth-audio-player.git | tee -a output.txt
	cd rpi-bluetooth-audio-player | tee -a output.txt
	export DBUS_SYSTEM_BUS_ADDRESS=unix:path=/run/dbus/system_bus_socket | tee -a output.txt
	export UDEV=1

	# Copy sounds
	echo "copying files" | tee -a output.txt
	cp -r ./bluetooth-player/sounds/ /usr/src/sounds/ | tee -a output.txt

	# Setup udev rules - this lets us play the connect/disconnect sound
	# and also turn off discover/pairing when a client is connected
	cp -r ./bluetooth-player/bluetooth-udev /usr/src/ | tee -a output.txt
	chmod +x /usr/src/bluetooth-udev | tee -a output.txt
	cp -r ./bluetooth-player/udev-rules/ /etc/udev/rules.d/ | tee -a output.txt

	# Bluetooth-agent handles the auth of devices
	cp -r ./bluetooth-player/bluetooth-agent /usr/src/ | tee -a output.txt
	chmod +x /usr/src/bluetooth-agent | tee -a output.txt

	cp -r ./bluetooth-player/start.sh /usr/src/ | tee -a output.txt
	chmod +x /usr/src/start.sh | tee -a output.txt

	echo "installing bluetooth-player service" | tee -a output.txt
	sudo ln -sf /usr/src/start.sh /usr/local/bin/start_bt_player | tee -a output.txt
	cp ./bluetooth-player/bluetooth-player.service /etc/systemd/system/ | tee -a output.txt

	echo "done !!!" | tee -a output.txt
	echo "starting bt player" | tee -a output.txt
	systemctl start bluetooth-player.service | tee -a output.txt

fi
