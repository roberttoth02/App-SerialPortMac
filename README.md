# Overview

The program reads a byte stream from a given Serial port and emits it as an LSL stream. 
This program does not currently support sending a startup sequence to the device or any other kind of 2-way handshake.

# Linux compilation

Prerequirements

    sudo apt install build-essential qt6-tools-dev libboost-thread-dev git cmake
    
Install liblsl from https://github.com/sccn/liblsl.
e.g. Under Ubuntu Jammy 22.04, download [this package](https://github.com/sccn/liblsl/releases/download/v1.16.2/liblsl-1.16.2-jammy_amd64.deb) and run:

    sudo apt install ./liblsl-1.16.2-jammy_amd64.deb

Then, execute the following commands in a terminal;

    git clone https://github.com/brunoherbelin/App-SerialPortLinux.git    
    mkdir App-SerialPortLinux-build
    cd App-SerialPortLinux-build
    cmake -DCMAKE_BUILD_TYPE=Release ../App-SerialPortLinux
    cmake --build .

NB: On Ubuntu, it might be required to authorize access of user to /dev/ttyS devices :

    sudo usermod -a -G tty $USER
    sudo usermod -a -G dialout $USER



# Usage
  * Start the SerialPort app. 
  
    ./SerialPortLSL
    

  * Make sure that your device is plugged in and that you know its /dev/tty entry 
  
  * Enter the /dev port and the BAUD rate (data rate). If you know the nominal sampling rate of your stream (in bytes per second), you can set this in the Stream Sampling Rate (0 means irregular/unknown).
  
  * Specify the data mode: "Pathtrough" will forward anything read on serial to LSL and blindly convert to integer. "Read integer number" will try to convert the raw stream from serial port to an integer value. Same can be done for floating point values.

  * The output data will be batched into chunks of the given Chunk Size -- you can reduce or increase this setting according to your application's needs. The remaining settings can often be left untouched, except for devices with special requirements.

  * Click the "Link" button. If all goes well you should now have a stream on your lab network that has the name that you entered under Stream Name and type "Raw". Note that you cannot close the app while it is linked.

  * For subsequent uses you can save the settings in the GUI via File / Save Configuration. If the app is frequently used with different settings you might can also make a shortcut on the desktop that points to the app and appends to the Target field the snippet `-c name_of_config.cfg`.
