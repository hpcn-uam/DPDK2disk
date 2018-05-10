#Current directory
DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"

#DPDK (default) SDK
if [ -z "$RTE_SDK" ]
then
        export RTE_SDK=$DIR/../dpdk
fi

#DPDK (default) TARGET
if [ -z "$RTE_TARGET" ]
then
        export RTE_TARGET=x86_64-native-linuxapp-gcc
fi

# Default vaules
FOLDER=/mnt/raid
FILELIM=4

# Argument parse
if [ $# -le 0 ]
    then echo "Output folder required. Example: $0 /mnt/raid"
    exit 1
fi
if [ $# -ge 1 ]
    then FOLDER=$1
fi
if [ $# -ge 2 ]
    then FILELIM=$2
fi

#Compile
cd $DIR/../src
make -j9
if [ $? -eq 0 ] ; then

	# c = Core mask
	# n = Memory chanels
	# --rx "(PORT, QUEUE, LCORE), ..." : List of NIC RX ports and queues    
        # --tx "(PORT, LCORE), ..." : List of NIC TX ports handled by the I/O TX
        # --w "LCORE, ..." : List of the worker lcores                          
        # --folder "My Mounted Raid" Where de data would be storage             
        # --maxgiga "1" : Maximal filesize                                      
        # OPTIONAL:                                                             
	# --rsz "A, B, C, D" : Ring sizes                                       
        #   A = Size (in number of buffer descriptors) of each of the NIC RX    
        #       rings read by the I/O RX lcores (default value is 1024)         
        #   B = Size (in number of elements) of each of the SW rings used by the
        #       I/O RX lcores to send packets to worker lcores (default value is
        #       1024)                                                           
        #   C = Size (in number of elements) of each of the SW rings used by the
        #       worker lcores to send packets to I/O TX lcores (default value is
        #       1024)                                                           
        #   D = Size (in number of buffer descriptors) of each of the NIC TX    
        #       rings written by I/O TX lcores (default value is 1024)          
	# --bsz "(A, B), (C, D), (E, F)" :  Burst sizes                         
        #   A = I/O RX lcore read burst size from NIC RX (default value is 144) 
        #   B = I/O RX lcore write burst size to output SW rings (default value 
        #       is 144)                                                         
        #   C = Worker lcore read burst size from input SW rings (default value 
        #       is 144)                                                         
        #   D = Worker lcore write burst size to output SW rings (default value 
        #       is 144)                                                         
        #   E = I/O TX lcore read burst size from input SW rings (default value 
        #       is 144)                                                         
        #   F = I/O TX lcore write burst size to NIC TX (default value is 144)  

	sudo build/app/hpcn_n2d -c FFF -n 6 -- --rx "(0,0,1),(0,0,2)" --tx "(0,4)" --w "3" \
                --rsz "1024, 1024" \
                --bsz "144, 144" \
		--folder $FOLDER --maxgiga $FILELIM --

else
	echo ""
	echo "Compiling Error..."
fi
cd -