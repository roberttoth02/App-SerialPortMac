# Overview

The program reads a byte stream from a given Serial port and emits it as an LSL stream. 
This program does not currently support sending a startup sequence to the device or any other kind of 2-way handshake.

# Mac compilation

Prerequisites

  * XCode and XCode Command Line Tools (https://developer.apple.com/xcode/)

  * Homebrew (https://brew.sh)

Install libraries available via Homebrew:

    brew install cmake boost qt6

Choose a working directory, and navigate to it in a terminal window, e.g.

    cd ~/Documents/LSL/

Get the latest version of liblsl and build it (currently no ready-made release for ARM Macs):

    git clone --depth=1 https://github.com/sccn/liblsl.git
    cd ./liblsl
    cmake -S . -B build -G "Xcode" -D LSL_UNIXFOLDERS=ON 
    sudo cmake --build build -j --config Release --target install
    cd ..

Then download and build the SerialPortLSL app:

    git clone --depth=1 https://github.com/roberttoth02/App-SerialPortMac.git    
    mkdir App-SerialPortMac-build
    cd App-SerialPortMac-build
    cmake -DCMAKE_BUILD_TYPE=Release ../App-SerialPortMac
    cmake --build .

You may move newly created application to a location of your choice, e.g.

    sudo mv SerialPortLSL.app /Applications/


# Usage
  * Start the SerialPort app. 
  
  * Make sure that your device is plugged in and that you know its /dev/tty entry. You may find this by running the following command in a terminal, before and after plugging in the serial device:<br />
    `/dev/tty.*`
  
  * Enter the /dev port and the BAUD rate (data rate). If you know the nominal sampling rate of your stream (in bytes per second), you can set this in the Stream Sampling Rate (0 means irregular/unknown).
  
  * Specify the data mode: "Passtrough" will forward anything read on serial to LSL and blindly convert to integer. "Read integer number" will try to convert the raw stream from serial port to an integer value. Same can be done for floating point values.

  * The output data will be batched into chunks of the given Chunk Size -- you can reduce or increase this setting according to your application's needs. The remaining settings can often be left untouched, except for devices with special requirements.

  * Click the "Link" button. If all goes well you should now have a stream on your lab network that has the name that you entered under Stream Name and type "Raw". Note that you cannot close the app while it is linked.
