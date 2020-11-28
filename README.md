libosmo-modbus - Osmocom Modbus interface library
=================================================

This repository contains a C-Language library providing an implementation and interface to manage a Modbus node in a Modbus bus.

This library relies heavily on [libosmocore](https://osmocom.org/) library and it is aimed at being used by applications using that same library.

The Modbus specs can be found here: https://www.modbus.org/specs.php

Currently supported features include:
* Master and Slave roles
* RTU backend

TODO:
* Implement ASCII backend
* Implement TCP backend
* Implement missing unicast messages/responses
* Implement sending exceptions (both to protocol peer and to the upper layer)
* Implement broadcast messages
* Add a sniffer util to sniff traffic and store it in a pcap file using libpcap
* Add a register storage using a rb_tree?
* Add unit tests
