#!/bin/sh

if [[ $EUID -ne 0 ]]; then
   echo "This script must be run as root"
   exit 1
	else
  	clear
  	echo "Pimusic update"
	sudo systemctl stop shairport-sync
	echo "removing bletooth support files"

	rm -rf /usr/src/sounds/

	rm -rf /usr/src/bluetooth-udev
	rm -rf /etc/udev/rules.d/udev-rules/
	rm -rf /usr/src/bluetooth-agent
	rm -rf /usr/src/start.sh
	rm -rf /usr/local/bin/start_bt_player

	systemctl stop bluetooth-player.service
	systemctl disable bluetooth-player.service
	rm -rf /etc/systemd/system/bluetooth-player.service


fi
