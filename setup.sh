export RTE_TARGET=x86_64-default-linuxapp-gcc
export RTE_SDK=../Intel_DPDK/DPDKCUR
curr=$(pwd)
cd $RTE_SDK
make config T=x86_64-default-linuxapp-gcc -j9
make clean -j9
make install T=x86_64-default-linuxapp-gcc -j9
cd x86_64-default-linuxapp-gcc
modprobe uio
make -j9
rmmod igb_uio
insmod kmod/igb_uio.ko
cd ../tools
./pci_unbind.py --force -b igb_uio 0000:04:00.0
./pci_unbind.py --force -b igb_uio 0000:04:00.1
./pci_unbind.py --status
cd $curr
