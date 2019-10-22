#!/bin/sh

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root"
   exit 1
	else
	clear
	echo "Pimusic Installation"
	echo ""
	echo "Preparing the System for Installation"
	echo ""
	echo "Adding needed Keys."
	# Spotify Key
	curl -sSL https://dtcooper.github.io/raspotify/key.asc | sudo apt-key add -v -
	echo 'deb https://dtcooper.github.io/raspotify raspotify main' | sudo tee /etc/apt/sources.list.d/raspotify.list
	
	# Get updates
	clear
	echo "Updating System"
	echo ""
	sudo apt-get update
	sudo apt-get -y upgrade
	sudo apt -y autoremove
	# Install needed packages 
	echo "Getting needed Packages"
	echo ""

	# Making directory for data.
	mkdir service_data
	cd service_data
	clear
	echo "Installing"
	echo ""
	# Install package
	clear
	echo "Installing Spotify Support"
	echo ""
	sudo apt-get -y install raspotify

	# Script for Airplay
	clear
	echo "Installing Airplay Support"
	echo ""
	sudo apt-get -y shairport-sync

	# Bluetooth script
	clear
	echo "Installing Bluetooth Support"
	echo ""
	cd ..
	git clone https://github.com/AlexProgrammerDE/rpi-bluetooth-audio-player.git
	cd rpi-bluetooth-audio-player
	sudo bash install.sh
	clear
fi
