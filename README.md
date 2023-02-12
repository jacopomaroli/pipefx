# Pipefx
This program allows you to apply a chain of effects to an input coming from a named pipe and output the result into another named pipe

## Installation
This project relies on https://github.com/cycfi/Q. We don't need to compile the whole thing; we'll just need a couple of headers:
```
mkdir -p /usr/local/include/q
sudo apt-get install libasound2-dev
git clone --recurse-submodules https://github.com/cycfi/Q.git
cp Q/infra/include/infra /usr/local/include/q/
cp Q/home/pi/Q/q_lib/include/q /usr/local/include/q/
```

After that we can just clone and compile this project with
```
git clone https://github.com/jacopomaroli/pipefx.git
cd pipefx
make
```

## Usage
Create a config file by copying the provided `config.example.cfg` and pass it as an argument like so:
```
./pipefx -c config.cfg
```
Define a chain of audio effects by specifying multiple `fx =` entries in the config file.

## Reload config
Config reload works by triggering a SIGUSR1 signal.
You can use
```
killall -SIGUSR1 pipefx
```
or
```
kill -SIGUSR1 $PIPEFX_PID
```
Note: Configuration reload works only for the fx chain.

## Limitations
For now it just supports a compressor and a lowpass filter.
Configuration reload works only for the fx chain.

## Thanks
This code was an adapted and inspired from https://github.com/voice-engine/ec and https://github.com/cycfi/Q
It was made to improve the user experience with https://rhasspy.readthedocs.io/en/latest/ and https://wiki.seeedstudio.com/ReSpeaker_6-Mic_Circular_Array_kit_for_Raspberry_Pi/

## Donations
If this project makes your life better in any way and you feel like showing appreciation you can buy me a coffee here  
[![paypal](https://www.paypalobjects.com/en_US/i/btn/btn_donateCC_LG.gif)](https://www.paypal.com/cgi-bin/webscr?cmd=_s-xclick&hosted_button_id=MNGDSC849AS6A)
