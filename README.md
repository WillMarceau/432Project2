# Chat Program for Unix

## Project Overview

This project impliments a chat service using a self defined protocol with a client and server architecture. A client can log into a server, join chat rooms, and chat with users in the room on any other server. The server topology is protected with loop detection and crash detection / recovery and each server passes along messages as needed to connected servers. The servers can be hosted on local host or given ip address. The protocol can be seen in the duckchat.h file. 

## Technical Requirements

Currently this program only works on Unix and will Segfault on Mac. I have not tested on windows.

## Installation

Just clone the repo and you are good to go!
There are executables in the repo, but you can clean and build your own with `<make clean>` and `<make all>` commands

## Running the Program
(ignore the `<>` when running the program, they are only included for readability)

1. To start a server manually you need to run `./server <ip address or localhost> <socket number>` then if you want to directly connect to other servers in the topology include their ip address and socket number after in the same format. ex: `./server <ip your ip> <your socket> <server_1 ip> <server_1 socket> <server_2 ip> <server_2 socket> ...`

2. If you want to avoid this, running the start_server.sh file will start a 3 x 3 server topology on localhost and socket numbers 4720 - 4728. You can edit this file to try other topologies, ip addresses or socket numbers of your choosing.

3. To start the client you need to run `./client <ip address of server> <socket number of server> <name>`. This will connect you to the server you entered and join you to the common room, your presence will be announced to all others servers in the topology if there is a path. From here you can send a message or run the `/join <name>` command to join or create a new room, `/leave <name>` command to leave a room, `/switch <name>` to switch to a different room you have joined, or `/exit` command to exit. A user can also run `/list` to list channels on their home server and `/who <name>` to see who on their home server is in the named channel, however these commands do not have cross server functionality.

## License
This project is for educational purposes as part of the CS 432 Intro to Networks course at the University of Oregon. All rights reserved.

## Acknowledgments
Intro to Networks Course, Univiersity of Oregon


