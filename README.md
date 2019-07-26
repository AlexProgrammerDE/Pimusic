# Pi-Music

This repository is created to give everyone the possibility to play Music on the Raspberry Pi.

This Repository installs multiple music service repositorys to let you play music by Spotify Connect, Airplay and Bluetooth.

I want to add Mopidy. 

So watch my project.

I am not a good programmer so I cant help very much.

I am using already the following repositorys:

Spotify Connect: https://github.com/dtcooper/raspotify

Airplay: https://github.com/mikebrady/shairport-sync

Bluetooth: https://github.com/AdityaTelange/rpi-bluetooth-audio-player

Maybe there can be found help.

Now comes the fun part. 

First we update the system and install git. (Copy this whole command)
```
sudo apt-get update \
sudo apt-get upgrade \
sudo apt-get install git
```
This can take some time...

Now we clone my repository.
```
git clone https://github.com/AlexProgrammerDE/Pi-Soundplayer.git
```
After the repository is cloned we go in it.
```
cd Pi-Soundplayer
```
Now we run my installation script.
```
sudo bash install.sh
```
So youre finished good luck. :)
