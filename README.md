Decode-O-Matic is a diagnostic tool that you can use to check midi signals. Sometimes especially in elaborate midi setups a synth may not respond to a midi signal you are sending to it and you want to find out why that is the case. In actual practise being able to see whether any midi signal is sent to the device at all often solves most problems. Furthermore, being able to see the channel number of the midi signal the synth is receiving will often help to remedy the problem. Only in more difficult cases it may be necessary to have a look at the midi data itself.

![Decode-O-Matic decoder][(https://github.com/josbouten/Decode-O-Matic/Decode-O-Matic-decoder.jpg)

![Decode-O-Matic viewer][(https://github.com/josbouten/Decode-O-Matic/Decode-O-Matic-viewer.jpg)

Decode-O-Matic consist of 2 parts. The decoder part is connected as a pass through between the synth and the midi interface. It is powered via USB. 

<foto van bovenkant van decode-o-matic>

It will show whether any midi data is received and it will show the data's channel. A second device is a midi viewer which can be connected via USB to a computer. 

Depending on the IDE you use, to compile the code you may need to install 2 libraries.

Dependencies for Decode-O-Matic-decoder when using platformio
	embeddedartistry/LibPrintf@^1.2.13
	khoih-prog/ESP8266TimerInterrupt@^1.6.0

Dependencies for Decode-O-Matic-viewer when using platformio
	embeddedartistry/LibPrintf@^1.2.13

Warning: Using D0 and D1 on the Wemos D1 R1 implies that you cannot upload code or use a serial terminal to look at any printed output as long as they are connected to the IO-devices. Only if you disconnect D0 you can upload code. If you disconnect D1 and do not redefine its pin setting only then will the serial output be visible in the serial out. Using the "#define DEBUG" So you could temporarily redefine Q9 to get rid of the problem.

The decoder and viewer are interconnected wirelessly via ESP_NOW and form a small private network. They do not require any WiFi router or similar to be around. Nor do they need internet access. The decoder however will need to know the mac address of the viewer. In de decoder code you need to fill the "broadcastAddress" with the proper address: 

uint8_t broadcastAddress[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
