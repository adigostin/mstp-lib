### mstp-lib
A library that implements the Multiple / Rapid Spanning Tree
Protocol as defined in 802.1Q-2011.

It's written in C++03 and is callable from C.

I integrated the library myself in devices with RSTP used
in ship automation, military equipment, avionics --
bare-bones as well as Linux-based.
I'm aware of it being used in other products as well.

### Simulator
A Simulator application for Windows is provided - both
sources and binaries. The Simulator lets you create
networks and see the library in action.
See the screenshot below. This is a project for
Visual Studio 2017 version 15.3.

### Embedded Application Example
The repository includes sources for a sample RSTP implementation
on an embedded device: a project for IAR Embedded Workbench 6.50,
for a device with an LPC2000 microcontroller and an IP175C
switch chip.

This sample highlights the platform-specific
code needed by the library -- mostly code that writes to
the hardware registers of the switch chip. This is typically
a few dozen lines of code spread over several functions.

You're going to have to write the platform-specific code
for your particular switch IC. Drop me a message at
[adigostin@gmail.com](mailto:adigostin@gmail.com)
and I'll try to help.

### API Help
The repository also includes
[help files](https://github.com/adigostin/mstp-lib/tree/master/_help)
for most of the library APIs -
[some](http://htmlpreview.github.io/?https://github.com/adigostin/mstp-lib/blob/master/_help/STP_CreateBridge.html)
[quite](http://htmlpreview.github.io/?https://github.com/adigostin/mstp-lib/blob/master/_help/StpCallback_TransmitGetBuffer.html)
[extensive](http://htmlpreview.github.io/?https://github.com/adigostin/mstp-lib/blob/master/_help/STP_OnPortEnabled.html).

### Screenshot of the Windows Simulator
![screenshot](./Screenshot-v2.1.png "Logo Title Text 1")
