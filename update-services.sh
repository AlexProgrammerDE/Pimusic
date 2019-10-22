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
	sudo apt-get install -y alsa-utils bluealsa bluez bluez-tools mplayer

	# Ask for Device name
	clear
	read -p “ Type in here the discoverable name of the Device: ” DEVICE_NAME

	echo "Deleting directory service_data"
	sudo rm -r service_data
	mkdir service_data
	cd service_data
	
	clear
	echo "Updating Spotify Connect"
	echo ""
	sudo apt-get -y install raspotify
	cat <<EOF >> /etc/default/raspotify
	DEVICE_NAME="$DEVICE_NAME"
	EOF
	
	clear
	echo "Updating Airplay"
	sudo apt-get -y shairport-sync
	cat <<EOF >> shairport-sync.sh
	!/bin/bash

	shairport-sync -a "$DEVICE_NAME"
	EOF
	
	cat <<EOF >> /etc/systemd/system/Pimusic-shairport-sync.service
	[Unit]
	Description=Shairport-Sync
	
	[Service]
	ExecStart=/home/pi/Pimusic/service_data/shairport-sync.sh
	
	[Install]
	WantedBy=multi-user.target
	EOF

	sudo systemctl enable Pimusic-shairport-sync
	sudo systemctl start Pimusic-shairport-sync

	
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
