#!/usr/bin/bash

#uncomment the topolgy you want. The simple two-server topology is uncommented here.

# Change the SERVER variable below to point your server executable.
SERVER=~/cs432/432Project2/test_server

SERVER_NAME=`echo $SERVER | sed 's#.*/\(.*\)#\1#g'`

# Generate a simple two-server topology
#$SERVER localhost 4720 localhost 4721 &
#$SERVER localhost 4721 localhost 4720 & 

# Generate a capital-H shaped topology
$SERVER localhost 4720 localhost 4721 &
$SERVER localhost 4721 localhost 4720 localhost 4722 localhost 4723 &
$SERVER localhost 4722 localhost 4721 & 
$SERVER localhost 4723 localhost 4721 localhost 4725 &
$SERVER localhost 4724 localhost 4725 &
$SERVER localhost 4725 localhost 4724 localhost 4723 localhost 4726 &
$SERVER localhost 4726 localhost 4725 &

# Generate a 3x3 grid topology
#$SERVER localhost 4720 localhost 4721 localhost 4723 &
#$SERVER localhost 4721 localhost 4720 localhost 4722 localhost 4724 &
#$SERVER localhost 4722 localhost 4721 localhost 4725 &
#$SERVER localhost 4723 localhost 4720 localhost 4724 localhost 4726 &
#$SERVER localhost 4724 localhost 4721 localhost 4723 localhost 4725 localhost 4007 &
#$SERVER localhost 4725 localhost 4722 localhost 4724 localhost 4728 &
#$SERVER localhost 4726 localhost 4723 localhost 4727 &
#$SERVER localhost 4727 localhost 4726 localhost 4724 localhost 4728 &
#$SERVER localhost 4728 localhost 4725 localhost 4727 &


echo "Press ENTER to quit"
read
pkill $SERVER_NAME
