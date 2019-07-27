# Inverter Poller

This is a simple program designed to query the basic runtime parameters of Voltronic, Axpert, Mppsolar PIP, Voltacon, Effekta, and other branded OEM solar inverters.

- Please see this [blog post](https://skyboo.net/2017/03/monitoring-voltronic-power-axpert-mex-inverter-under-linux/) for the original findings, and inspiration for this project.

- [@ned-kelly](https://github.com/ned-kelly/) has also created a docker based [Home Assistant](https://www.home-assistant.io/) automation of this project and tested
it with both Serial & USB. It can be used as a fully-fledged integration to the inverter, with a user-interface and hooked up to a monitoring system such as Grafana - If you are interested, please head to:
[https://github.com/ned-kelly/docker-voltronic-homeassistant](https://github.com/ned-kelly/docker-voltronic-homeassistant)

------------------------------------------------------------

## Compilation / running

build/compilation procedure:

```
git clone git://github.com/manio/skymax-demo.git
cd skymax-demo
cmake .. && make
```

The code requires your inverter to be connected either via USB or RS323, and can be configured in the `inverter.conf` file... 


You can then run the inverter binary afterwards - By default, it will query the inverter every few seconds and return a JSON response to the console...


#### Basic command line arguments supported are:

```
USAGE:  ./inverter_poller <args> [-r <command>], [-h | --help], [-1 | --run-once]

SUPPORTED ARGUMENTS:
          -r <raw-command>      TX 'raw' command to the inverter
          -h | --help           This Help Message
          -1 | --run-once       Runs one iteration on the inverter, and then exits
          -d                    Additional debugging

```

Note:

- When using the `tx` command, your commands will need to follow the specification outlined [here](https://github.com/ned-kelly/docker-voltronic-homeassistant/blob/master/manual/HS_MS_MSX_RS232_Protocol_20140822_after_current_upgrade.pdf).
- TX commands will be executed directly on the inverter, then the process wil exit thereafter.

--------------------------------------------------------------------------------------
license
--------------------------------------------------------------------------------------
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
See the file COPYING for more information.
