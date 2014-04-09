San-Trac
=======

San-Trac is a project to develop a remote monitoring system in the context of trying to improve sanitation conditions in schools in rural Uganda.

You can find a full description about it [in this page](http://www.sanitationhackathon.org/applications/san-trac).


This repository contains the code used in the prototype made in mid-2012 in the UNICEF Innovation Center located in Kampala, Uganda.

It was based on a Teensy 2.0 board (Arduino compatible), which was connected to a GPRS module and several switch sensors connected to toilet doors.
The prototype would send an SMS message every time a person used the toilet. The previous prototype supported sending messages daily. This feature is
still in the code, the functions are simply not being called.

This code can not only serve as an example of working with a GPRS module from switching it on and off, testing for connectivity with the network, and sending
SMS messages, but also managing real time, reading several sensors and working with EEPROM.


