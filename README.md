<html>
	<head>
		<link rel="stylesheet" type="text/css" href="mystyle.css">
	</head>
	<body>

<img src="https://raw.githubusercontent.com/AlexProgrammerDE/Pimusic/master/logo.png" alt="Pimusic" width="400">
[![HitCount](http://hits.dwyl.io/AlexProgrammerDE/Pimusic.svg)](http://hits.dwyl.io/AlexProgrammerDE/Pimusic)
This repository is created to give everyone the possibility to play Music on the Raspberry Pi.

This Repository installs multiple music service repositorys to let you play music by Spotify Connect, Airplay and Bluetooth.

This is my first real projekt so dont be too hard to me when somethig not works.

I am not a good programmer.

But report me the Issue and I will try to fix it.

I am using already the following repositorys:

Spotify Connect: https://github.com/dtcooper/raspotify

Airplay: https://github.com/mikebrady/shairport-sync

Bluetooth: https://github.com/AdityaTelange/rpi-bluetooth-audio-player

Mopidy: https://github.com/mopidy/mopidy

Mopidy-Spotify: https://github.com/mopidy/mopidy-spotify

Mopidy-Tunein: https://github.com/kingosticks/mopidy-tunein

Mopidy-Soundcloud: https://github.com/mopidy/mopidy-soundcloud

Mopidy-Scrobbler: https://github.com/mopidy/mopidy-scrobbler

Maybe can be found there help.

Now comes the fun part. 

First we update the system and install git.
```
sudo apt-get update && sudo apt-get install -y wget
```
This can take some time...

Now we clone my installation script.
```
wget https://raw.githubusercontent.com/AlexProgrammerDE/Pimusic/master/install.sh
```
Finally we run my installation script.
```
sudo bash install.sh
```
So youre finished good luck. :)

If you want to update the services you need to go only in the directory Pimusic with:
```
cd Pimusic
```
And then you need to start:
```
sudo bash update-services.sh | tee -a update-output.txt
```
