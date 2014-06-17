export RTE_TARGET=x86_64-default-linuxapp-gcc
export RTE_SDK=/home/dpdk/Intel_DPDK/DPDKCUR

#cp /home/pedro/dpi_engine/bin/config.cfg* .
cp -r /home/pedro/dpi_engine/bin/protocolsCP protocols


clear
make clean -j9
make -j9
if [ $? -eq 0 ] ; then

	# c = numero de procesadores
	# n = numero de canales de memoria
	# --rx "(PORT, QUEUE, LCORE), ..." : List of NIC RX ports and queues       
        # tx "(PORT, LCORE), ..." : List of NIC TX ports handled by the I/O TX   
        # w "LCORE, ..." : List of the worker lcores                             
        # OPTIONAL:                                                                     
	# rsz "A, B, C, D" : Ring sizes                                          
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
	# bsz "(A, B), (C, D), (E, F)" :  Burst sizes                            
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
	# pos-lb POS : Position of the 1-byte field within the input packet used by
        #   the I/O RX lcores to identify the worker lcore for the current      
        #   packet (default value is 29)    

	build/app/hpcn_flow -c 6 -n 2 -b "0000:04:00.1" -- --rx "(0,0,1)" --tx "(0,1)" --w "2" \
                --rsz "2048, 2048, 1024, 1024" \
                --bsz "(144, 144), (288, 144), (144, 144)"
#                --bsz "(8, 8), (8, 8), (8, 8)"
              #  | grep rat | grep -v Worker | sed -e 's/avg.*//gi' | sed -e 's/.*.:.//gi' | sed -e 's/.d.*.%//gi' | sed -e 's/.s.*.%//gi' | sed -e 's/(//gi' | sed -e 's/)//gi' | sed -e 's/\// /gi' | sed -e 's/ /\t/gi' | awk '$1 == "NIC" { nicok+=$2; nicerr+=$3 } $1 == "enq" { print nicerr/(nicok+nicerr), ($3-$2)/$3; nicok=0;nicerr=0 }' 


else
	echo ""
	echo "Error en la compilacion..."
fi
