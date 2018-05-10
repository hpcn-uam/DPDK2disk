[Beta] High Speed Packet Dumper using DPDK 
=================

This application allows to dump the network's packets to a high speed disk array.

Tested up to **40Gbps**

*Current phase*: **In development**

Clonning
=================
To clone this project execute:

````
git clone https://github.com/hpcn-uam/DPDK2disk.git
git submodule update --init --recursive
````

if you want to update (pull) to a newer commit, you should execute:

````
git pull
git submodule update --init --recursive
````

DPDK-Compilation
=================
The latest tested DPDK-repository with this application is included in the `dpdk` folder.
Howerver, any other compatible-version could be used by exporting `RTE_SDK` variable.

To compile the included DPDK-release, it is recomended to execute and follow the basic `dpdk-setup.sh` script, example:

````
cd dpdk
./tools/dpdk-setup.sh
cd ..
````

APP-Compilation
=================
The application is compiled automatically when executing one of the provided scripts.
If you prefere to compile it manually, in the `src` folder there is a `Makefile` to do it.

Execution
=================
In `script` folder, there are some example scripts:

- `scripts/capture0.sh <outputFolder> [MaxFileSize]` Start dumping the packets into the `outputFolder`. Default pcap size is 4GB.
