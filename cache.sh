#!/bin/bash

INPUT=cache
OUTPUT=/captura
OUTPUT=/captura/catch

while [ 1 ]
do

#	dnotify -Mso src $INPUT

	FILES=$(ls $INPUT -rt | wc -l)
	while [ $FILES -gt 1 ]
	do
		FILE=$(ls -rt $INPUT | head -n 1)

		dd bs=2M count=10000 if=$INPUT/$FILE of=$OUTPUT/$FILE oflag=nonblock
		rm -rf $INPUT/$FILE

		echo $INPUT/$FILE saved

		FILES=$(ls $INPUT -rt | wc -l)
	done

done
