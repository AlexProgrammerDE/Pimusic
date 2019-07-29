#!/bin/sh

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root"
   exit 1
	else
    	mkdir Pimusic
    	cd Pimusic
    	wget https://raw.githubusercontent.com/AlexProgrammerDE/Pimusic/master/install-services.sh
    	sudo bash install-services.sh | tee -a install-output.txt
	rm install-services.sh
	wget https://raw.githubusercontent.com/AlexProgrammerDE/Pimusic/master/update-services.sh
    	cd ..
    	rm install.sh
fi
