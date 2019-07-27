#!/bin/sh

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root" | tee -a output.txt
   exit 1
	else
    mkdir Musicpi
    cd Musicpi
    wget https://raw.githubusercontent.com/AlexProgrammerDE/Musicpi/master/install-services.sh
    sudo bash install-services.sh | tee -a output.txt
fi
