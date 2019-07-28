#!/bin/sh

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" | tee -a output.txt
   exit 1
	else
    	mkdir Pimusic
    	cd Pimusic
    	wget https://raw.githubusercontent.com/AlexProgrammerDE/Pimusic/master/install-services.sh
    	sudo bash install-services.sh | tee -a output.txt
    	cd ..
    	rm install.sh
fi
