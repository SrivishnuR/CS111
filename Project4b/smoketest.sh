#!/bin/bash
rm -f smokelog.txt

./lab4b --period=5 --scale=F --log=smokelog.txt <<-EOF >> /dev/null
PERIOD=10
SCALE=F
SCALE=C
STOP
START
LOG testlog
OFF
EOF

if [ $? -ne 0 ]
then
	echo "Program exited with bad code"
fi

if [ ! -s smokelog.txt ]
then
	echo "Program did not create log file"
fi

grep "PERIOD=10" smokelog.txt >> /dev/null
if [ $? -ne 0 ]
then
	echo "Program did not log PERIOD=10"
fi

grep "SCALE=F" smokelog.txt >> /dev/null
if [ $? -ne 0 ]
then
	echo "Program did not log SCALE=F"
fi

grep "SCALE=C" smokelog.txt >> /dev/null
if [ $? -ne 0 ]
then
	echo "Program did not log SCALE=C"
fi

grep "STOP" smokelog.txt >> /dev/null
if [ $? -ne 0 ]
then
	echo "Program did not log STOP"
fi

grep "START" smokelog.txt >> /dev/null
if [ $? -ne 0 ]
then
	echo "Program did not log START"
fi

grep "LOG testlog" smokelog.txt >> /dev/null
if [ $? -ne 0 ]
then
	echo "Program did not log LOG testlog"
fi

grep "OFF" smokelog.txt >> /dev/null
if [ $? -ne 0 ]
then
	echo "Program did not log OFF"
fi

grep "SHUTDOWN" smokelog.txt >> /dev/null
if [ $? -ne 0 ]
then
	echo "Program did not log the SHUTDOWN line"
fi

rm -f smokelog.txt
