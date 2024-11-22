#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "duckchat.h"
#include <fcntl.h>
#include <netdb.h>

// for error checking
#include <errno.h>

// MIGHT NEED TO NULL TERMINATE MY COPPIED STRINGS

// might not need but struct to keep users name, ip and port to send to
typedef struct {
	char username[USERNAME_MAX];
	struct sockaddr_in address;
} user;

// Channel struct
// keep track of the channels and their active subscribers
typedef struct {
	char name[CHANNEL_MAX];
	user *users;
	int capacity;
	int size;
} channel;

// struct to hold the Channel list and user list

typedef struct {
	// dont need seperate list of users?
	//user *users;
	channel *channels;
	int capacity;
	int size;
} channelList;

// struct to hold a user list
typedef struct {
	user *users;
	int capacity;
	int size;
} userList;

// helper functions
/*
void addUser(channelList *list, char *channelName, char *userName, struct sockaddr_in address) {
	// this function is for adding a user to an existing 
	// find channel in list
}
*/

void addToChannel(channelList *list, char *channelName, char *userName, struct sockaddr_in address) {
	// adds user to channel and if channel does not exist,
	// adds a new channel to the channel list, and adds the user to that list
	//if (channelName == NULL || strlen(channelName) == 0) {
	//	printf("error with channel name\n");
		//return;
	//}
	
	// search for channel
	for (int i = 0; i < list->size; i++) {
		if (strcmp(list->channels[i].name, channelName) == 0) {
			// resize if need capacity
			if (list->channels[i].size >= list->channels[i].capacity) {
				list->channels[i].capacity *= 2;
				list->channels[i].users = (user *)realloc(list->channels[i].users, list->channels[i].capacity * sizeof(user));

				if (list->channels[i].users == NULL) {
					printf("failed to resize array\n");
					return;
				}
			}

			// create user
			user newUser;
			strncpy(newUser.username, userName, USERNAME_MAX - 1);
			newUser.username[USERNAME_MAX - 1] = '\0';
			newUser.address = address;

			// add user to list
			//printf("SHOULD NOT PRINT\n");
			list->channels[i].users[list->channels[i].size] = newUser;
			list->channels[i].size++;
			return;

		}

	}
	
	// resize if need capacity
	if (list->size >= list->capacity) {
		list->capacity *= 2;
		list->channels = (channel *)realloc(list->channels, list->capacity * sizeof(channel));
		
		// error checking
		if (list->channels == NULL) {
			printf("Failed to resize array\n");
			return;
		}
	}

	// create new channel
	channel newChannel;
	strncpy(newChannel.name, channelName, CHANNEL_MAX - 1);
	newChannel.name[CHANNEL_MAX - 1] = '\0';
	newChannel.capacity = 2;
	newChannel.size = 1;

	// create a new user
	user newUser;
	strncpy(newUser.username, userName, USERNAME_MAX - 1);
	newUser.username[USERNAME_MAX - 1] = '\0';
	newUser.address = address;

	// add user to channel
	newChannel.users = (user *)malloc(newChannel.capacity * sizeof(user));
	
	// error checking
	if (newChannel.users == NULL) {
		printf("failed to resize array\n");
		return;
	}

	newChannel.users[0] = newUser;

	list->channels[list->size] = newChannel;
	list->size++;
	return;

}

void removeChannel(channelList *list, char *channelName);

void removeUserChannel(channelList *list, char *channelName, char* userName) {
	// this function is for removing users from a channel
	
	// find the channel user wants to leave
	for (int i = 0; i < list->size; i++) {
		if (strcmp(list->channels[i].name, channelName) == 0) {
			//printf("\n");

			//int found = 0;
			// find the user in the user list
			for (int j = 0; j < list->channels[i].size; j++) {
				if (strcmp(list->channels[i].users[j].username, userName) == 0) {
					// free memory
					
					// delete the user
					// fill the gap
					for (int k = j; k < list->channels[i].size - 1; k++) {
						list->channels[i].users[k] = list->channels[i].users[k + 1];

					}

					// decrease size
					list->channels[i].size--;
					
					// return
					return;
				}
			}
		}
	}

}

void removeChannel(channelList *list, char *channelName) {
	// this function is for removing empty channels
	
	// find the empty channel in list
	for (int i = 0; i < list->size; i++) {
		if (strcmp(list->channels[i].name, channelName) == 0) {
			
			// free memory
			free(list->channels[i].users);

			// shift elements to fix gap
			for (int j = i; j < list->size - 1; j++) {
				list->channels[j] = list->channels[j + 1];
			}

			// null last swap
			//list->channels[list->size - 1] = NULL;
			memset(&list->channels[list->size - 1], 0, sizeof(channel));

			// decrease size
			list->size--;

			return;
		}
	}
	printf("could not find channel to remove\n");
}

void addUserList(userList *list, char *userName, struct sockaddr_in address) {
	// this function adds a user to the user list
	
	// resize array if cap needed
	if (list->size >= list->capacity) {
		list->capacity *= 2;
		list->users = (user *)realloc(list->users, list->capacity * sizeof(user));

		// error checking
		if (list->users == NULL) {
			printf("Failed to resize array\n");
			return;
		}
	
	}

	// create new user
	user newUser;
	strncpy(newUser.username, userName, USERNAME_MAX - 1);
	newUser.username[USERNAME_MAX - 1] = '\0';
	newUser.address = address;

	// add user to channel
	list->users[list->size] = newUser;
	list->size++;
}

void removeUserList(userList *list, char *userName) {
	// find user in list
	for (int i = 0; i < list->size; i++) {
		if (strcmp(list->users[i].username, userName) == 0) {
	
			// free memory
			//free(list->users[i]);

			// shift elements to fix gap
			for (int j = i; j < list->size - 1; j++) {
				list->users[j] = list->users[j+1];
			}

			
			// decrease size
			list->size--;

			return;
		}
	}

}

void freeUserList(userList *list) {
	free(list->users);
	free(list);
}

void freeChannelList(channelList *list) {
	for (int i = 0; i < list->size; i++) {
		free(list->channels[i].users);
	}
	free(list->channels);
	
	// might not need
	// list->channels = NULL
}

int compareSocket(struct sockaddr_in addr1, struct sockaddr_in addr2) {
	// function to compare sockets in order to determine the username of user
	
	return (addr1.sin_family == addr2.sin_family) && (addr1.sin_port == addr2.sin_port) && (addr1.sin_addr.s_addr == addr2.sin_addr.s_addr);
}

int main(int argc, char *argv[]) {

	if (argc != 3) {
		printf("Usage: ./server server_socket server_port");
		return -1;
	}

	// extract command line arguments
	const char *serverHost = argv[1];
	char *serverPortString = argv[2];

	// deal with local host input
	//if (strcmp(serverHost, "localhost") == 0) {
		//serverHost = "127.0.0.1";
	//}

	// resolve hostname
	struct addrinfo hints, *resolution;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_DGRAM;

	int status = getaddrinfo(serverHost, serverPortString, &hints, &resolution);
	if (status != 0) {
		printf("error resolving host name. .\n");
		return -1;
	}

	// convert port to int for socket addr struct and binding
	int serverPort = atoi(serverPortString);

	// create server socket
	int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketfd == -1) {
		perror("Socket Failure");
	}

	// set non blocking mode
	int flags = fcntl(socketfd, F_GETFL, 0);
	fcntl(socketfd, F_SETFL, flags | O_NONBLOCK);

	// create server addr and client addr struct
	struct sockaddr_in serverAddr, clientAddr;
	socklen_t clientLength = sizeof(clientAddr);

	// clean struct
	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(serverPort);
	serverAddr.sin_addr.s_addr = INADDR_ANY;

	// bind socket
	//if (bind(socketfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
	if (bind(socketfd, resolution->ai_addr, resolution->ai_addrlen) == -1) {
		perror("bind failure\n");
		close(socketfd);
		return -1;
	}

	// free the resolution 
	freeaddrinfo(resolution);

	// init variables
	fd_set readfds;
	struct timeval timeout;
	char inBuff[1024];
	char inUsername[USERNAME_MAX];
	char inChannel[CHANNEL_MAX];
	char inMessage[SAY_MAX];
	ssize_t bytes;

	// create the channelList
	channelList *chanList = (channelList *)malloc(sizeof(channelList));
	

	chanList->channels = (channel *)malloc(2 * sizeof(channel));

	if (chanList == NULL || chanList->channels == NULL) {
		printf("error mallocing channel List\n");
		free(chanList);
		return -1;
	}

	chanList->capacity = 2;
	chanList->size = 0;

	// create the userList
	userList *uList = (userList *)malloc(sizeof(userList));

	uList->users = (user *)malloc(2 * sizeof(user));

	if (uList == NULL || uList->users == NULL) {
		printf("error mallocing user list\n");
		free(uList);
		return -1;
	}

	uList->capacity = 2;
	uList->size = 0;

	
	// listen loop
	while(true) {
		// init fd set
		FD_ZERO(&readfds);

		// set socket listening
		FD_SET(socketfd, &readfds);

		// set timeout
		timeout.tv_sec = 1;
		timeout.tv_usec = 0;

		// wait for connection
		int activity = select(socketfd + 1, &readfds, NULL, NULL, &timeout);

		// check for errors
		if (activity < 0 && errno != EINTR) {
			perror("select error\n");
			break;
		}

		// connection initiated
		else if (activity > 0) {
			ssize_t inbytes = recvfrom(socketfd, inBuff, sizeof(inBuff) - 1, 0, (struct sockaddr *)&clientAddr, &clientLength);
		        //printf("%ld\n", inbytes);	
			inBuff[inbytes] = '\0';
			//printf("recieved from client %s:%d %s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), inBuff);
			//fflush(stdout);
			if (inbytes < 0) {
					
				perror("error reading bytes sent to socket\n");
			}
			
			else {
				// create base request struct
				struct request *baseRequest = (struct request *)inBuff;

				// read the req type
				request_t type = baseRequest->req_type;

				// login request
				if (type == REQ_LOGIN) {

					// check the packet size
					if (inbytes == 36) {

					// cast to login struct
					struct request_login *login_request = (struct request_login*)baseRequest;

					// extract info
					strncpy(inUsername, login_request->req_username, USERNAME_MAX - 1);
					inUsername[USERNAME_MAX - 1] = '\0';
					strncpy(inChannel, "Common", CHANNEL_MAX - 1);
					inChannel[CHANNEL_MAX - 1] = '\0';

					// add user to Common and userList
					//addToChannel(chanList, inChannel, inUsername, clientAddr);
					addUserList(uList, inUsername, clientAddr);

					printf("server: %s logs in\n", inUsername);

					//printf("%s\n", chanList->channels[0].users[0].username);
					}
					else {
						printf("server: Bad packet, expecting packet of size 36, got %ld\n", inbytes);


						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;
				
						snprintf(errorBuff, SAY_MAX, "Error: Sent bad Login Packet");
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';

					
						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("Failed to send\n");
						}

					}


				}

				// logout request
				else if (type == REQ_LOGOUT) {

					// check size
					if (inbytes == 4) {

					// get username
					int search = 0;
					for (int i = 0; i < uList->size; i++) {
						if (compareSocket(uList->users[i].address,clientAddr)) {
							//printf("%s\n", uList->users[i].username);
							strncpy(inUsername, uList->users[i].username, USERNAME_MAX - 1);
							inUsername[USERNAME_MAX - 1] = '\0';
							search = 1;
							break;
						}
					}

					if (search) {

					// remove user from all channels
					printf("server: %s logs out\n", inUsername);
					int index = 0;
					//char currentName[CHANNEL_MAX];
					while(index < chanList->size) {
						//printf("index %d\n", index);
						//printf("checking %s\n", chanList->channels[index].name);
						removeUserChannel(chanList, chanList->channels[index].name, inUsername);
						

						// if channel is empty, remove channel
					      	if (chanList->channels[index].size == 0) {
							printf("server: removing empty channel %s\n", chanList->channels[index].name);
							fflush(stdout);
							removeChannel(chanList, chanList->channels[index].name);
						}
						else {
							index++;
						}
					}


					// remove uesr from userlist
					removeUserList(uList, inUsername);
					}

					else {
						//printf("server: user is not logged in, dropping packet\n");

					}
					}

					// bad packet
					else {
						printf("server: Bad packet, expecting packet of size 4, got %ld\n", inbytes);


						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;
				
						snprintf(errorBuff, SAY_MAX, "Error: Sent bad Logout Packet");
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';

					
						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("Failed to send\n");
						}

					}
					

				}

				// Join request
				else if (type == REQ_JOIN) {

					// check size of packet
					if (inbytes == 36) {

					// cast to join struct
					struct request_join *join_request = (struct request_join*)baseRequest;

					// extract info
					strncpy(inChannel, join_request->req_channel, CHANNEL_MAX - 1);
					inChannel[CHANNEL_MAX - 1] = '\0';

					// get username
					int found = 0;
					for (int i = 0; i < uList->size; i++) {
						if (compareSocket(uList->users[i].address, clientAddr)) {
							//printf("%s\n", uList->users[i].username);
							strncpy(inUsername, uList->users[i].username, USERNAME_MAX - 1);
							inUsername[USERNAME_MAX - 1] = '\0';
							found = 1;
							break;
						}
					}

					if (found) {

					int search = 0;
					// check if user in channel
					for (int i = 0; i < chanList->size; i++) {
						if (strcmp(chanList->channels[i].name, inChannel) == 0) {
							for (int j = 0; j < chanList->channels[i].size; j++) {
								if (strcmp(chanList->channels[i].users[j].username, inUsername) == 0) {
									search = 1;
									break;
								}
							}
						}
						if (search) {
							break;
						}
					}

					if (!search) {

						// add user to channel
						addToChannel(chanList, inChannel, inUsername, clientAddr);
					}
					printf("server: %s joins channel %s\n", inUsername, inChannel);
					}
					else {
						//printf("server: user not logged in, dropping packet\n");
					}
					}
					
					// bad packet
					else {
						printf("server: Bad packet, expecting packet of size 36, got %ld\n", inbytes);


						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;
				
						snprintf(errorBuff, SAY_MAX, "Error: Sent bad Join Packet");
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';
						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("Failed to send\n");
						}

					

					}

				}

				// leave request
				else if (type == REQ_LEAVE) {

					// check packet size
					if (inbytes == 36) {

					// cast to leave struct
					struct request_leave* leave_request = (struct request_leave*)baseRequest;

					// extract info
					strncpy(inChannel, leave_request->req_channel, CHANNEL_MAX - 1);
					inChannel[CHANNEL_MAX - 1] = '\0';

					// get user
					int search  = 0;
					for (int i = 0; i < uList->size; i++) {
						if (compareSocket(uList->users[i].address,clientAddr)) {
							//printf("%s\n", uList->users[i].username);
							strncpy(inUsername, uList->users[i].username, USERNAME_MAX - 1);
							inUsername[USERNAME_MAX - 1] = '\0';
							search = 1;
							break;
						}
					}

					if (search) {

					int exists = 0;
					int found = 0;
					// check if channel exists
					for (int i = 0; i < chanList->size; i++) {
						if (strcmp(inChannel, chanList->channels[i].name) == 0) {
							exists = 1;
							for (int j = 0; j < chanList->channels[i].size; j++) {

								// check if user in channel
								if (strcmp(chanList->channels[i].users[j].username, inUsername) == 0) {
									found = 1;
								}
							}

						}


					}

					if (exists && found) {

						// remove user from channel
						printf("server: %s leaves channel %s\n", inUsername, inChannel);
						removeUserChannel(chanList, inChannel, inUsername);

						// if channel is empty, remove channel
						for (int i = 0; i < chanList->size; i++) {
							if (strcmp(inChannel, chanList->channels[i].name) == 0) {
								if (chanList->channels[i].size == 0) {
									printf("server: %s channel empty removing\n", chanList->channels[i].name);
									fflush(stdout);
									removeChannel(chanList, chanList->channels[i].name);
								}
							}
						}
					}

					// send error if not in channel
					else if (exists && !found) {
						printf("server: %s is trying to leave channel %s where he/she is not a member\n", inUsername, inChannel);
						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;
				
						snprintf(errorBuff, SAY_MAX, "Error: You are not in channel %s", inChannel);
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';

					
						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("Failed to send\n");
						}
					}

					// send error if channel does not exist
					else if (!exists) {
						printf("server: %s trying to leave a non-existent channel %s\n", inUsername, inChannel);
						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;

						snprintf(errorBuff, SAY_MAX, "Error: No channel by the name %s", inChannel);
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';

						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("Failed to send\n");
						}

					}
					}
					// user does not exist
					else {
						//printf("server: user is not logged in, dropping packet\n");
					}
					}

					// bad packet
					else {
						printf("server: Bad packet, expecting packet of size 36, got %ld\n", inbytes);


						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;
				
						snprintf(errorBuff, SAY_MAX, "Error: Sent bad Leave Packet");
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';
						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("Failed to send\n");
						}

					}


				}


				// list request
				else if (type == REQ_LIST) {

					// check size
					if (inbytes == 4) {
					// extract info
					//
					// create channel_info array
					//struct text_list list_text;
					struct text_list *list_text = (text_list *)malloc(sizeof(struct text_list) + sizeof(struct channel_info) * (chanList->size));

					// get user
					int search = 0;
					for (int i = 0; i < uList->size; i++) {
						if (compareSocket(uList->users[i].address,clientAddr)) {
							//printf("%s\n", uList->users[i].username);
							strncpy(inUsername, uList->users[i].username, USERNAME_MAX - 1);
							inUsername[USERNAME_MAX - 1] = '\0';
							search = 1;
							break;
						}
					}
					if (search) {

					// get list of all active channels
					for (int i = 0; i < chanList->size; i++) {
						strncpy(list_text->txt_channels[i].ch_channel, chanList->channels[i].name, CHANNEL_MAX - 1);
						list_text->txt_channels[i].ch_channel[CHANNEL_MAX - 1] = '\0';
					}

					// pack text_list
					list_text->txt_type = TXT_LIST;
					list_text->txt_nchannels = chanList->size;

					// get size 
					size_t total_size = sizeof(struct text_list) + sizeof(struct channel_info) * chanList->size;

					// send list to user
					bytes = sendto(socketfd, list_text, total_size, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
					if (bytes < 0) {
						printf("error sending info\n");
					}

					free(list_text);
					printf("server: %s lists channels\n", inUsername);
					}

					else {
						//printf("server: user not logged in, dropping packet\n");
					}
					}

					// bad packet
					else {
						printf("server: Bad packet, expecting packet of size 4, got %ld\n", inbytes);


						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;
				
						snprintf(errorBuff, SAY_MAX, "Error: Sent bad List Packet");
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';
						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("Failed to send\n");
						}

					}	

				}

				// who request
				else if (type == REQ_WHO) {

					// check packet size
					if (inbytes == 36) {

					// cast to who struct
					struct request_who *who_request = (request_who *)baseRequest;

					// extract info
					strncpy(inChannel, who_request->req_channel, CHANNEL_MAX - 1);
					inChannel[CHANNEL_MAX - 1] = '\0';
					struct text_who *who_text;
					size_t whoSize;

					int found = 0;

					// find channel
					for (int i = 0; i < chanList->size; i++) {
						if (strcmp(chanList->channels[i].name, inChannel) == 0) {
						found = 1;
					
							// create who_text
							who_text = (text_who *)malloc(sizeof(struct text_who) + sizeof(struct user_info) * (chanList->channels[i].size));

							// for user in channel
							for (int j = 0; j < chanList->channels[i].size; j++) {

								// fill the user list
								strncpy(who_text->txt_users[j].us_username, chanList->channels[i].users[j].username, USERNAME_MAX - 1);
								who_text->txt_users[j].us_username[USERNAME_MAX - 1] = '\0';
							}

						// add number of users to struct
						who_text->txt_nusernames = chanList->channels[i].size;

						
						// get size
						whoSize = sizeof(struct text_who) + sizeof(struct user_info) * chanList->channels[i].size;

						// break
						break;
						}
					}


					// if channel exists
					if (found) {

						// pack text who
						who_text->txt_type = TXT_WHO;
						strncpy(who_text->txt_channel, inChannel, CHANNEL_MAX - 1);
						who_text->txt_channel[CHANNEL_MAX - 1] = '\0';

						// send list to user
						//bytes = sendto(socketfd, who_text, whoSize, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						
						// get user
						int search = 0;
						for (int i = 0; i < uList->size; i++) {
							if (compareSocket(uList->users[i].address,clientAddr)) {
								//printf("%s\n", uList->users[i].username);
								strncpy(inUsername, uList->users[i].username, USERNAME_MAX - 1);
								inUsername[USERNAME_MAX - 1] = '\0';
								search = 1;
								break;
							}
						}

						if (search) {

						printf("server: %s lists users in channel %s\n", inUsername, inChannel);
						bytes = sendto(socketfd, who_text, whoSize, 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							printf("error sending info\n");
						}
						}

						else {
							//printf("server: user not logged in, dropping packet\n");
						}

						//free(who_text);
					}

					// send error if not found
					else {
						printf("Server: %s trying to list users in non-existing channel %s\n", inUsername, inChannel);

						// fill struct
						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;

						snprintf(errorBuff, SAY_MAX, "Error: No channel by the name %s", inChannel);
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';

						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("Failed to send\n");
						}
						//free(who_text);

					}
					}

					// bad packet
					else {

						printf("server: Bad packet, expecting packet of size 36, got %ld\n", inbytes);


						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;
				
						snprintf(errorBuff, SAY_MAX, "Error: Sent bad Who Packet");
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';
						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("Failed to send\n");
						}
					}

				}

				// message request
				else if (type == REQ_SAY) {

					// check packet size
					if (inbytes == 100) {

					// cast to say struct
					struct request_say *say_request = (request_say *)baseRequest;
					
					// extract info
					strncpy(inChannel, say_request->req_channel, CHANNEL_MAX - 1);
					inChannel[CHANNEL_MAX - 1] = '\0';
					strncpy(inMessage, say_request->req_text, SAY_MAX - 1);
					inMessage[SAY_MAX - 1] = '\0';

					// get username
					int search = 0;
					for (int i = 0; i < uList->size; i++) {
						if (compareSocket(uList->users[i].address,clientAddr)) {
							//printf("%s\n", uList->users[i].username);
							strncpy(inUsername, uList->users[i].username, USERNAME_MAX - 1);
							inUsername[USERNAME_MAX - 1] = '\0';
							search = 1;
							break;
						}
					}


					if (search) {
					// fill text say struct
					struct text_say say_text;
					say_text.txt_type = TXT_SAY;
					strncpy(say_text.txt_channel, inChannel, CHANNEL_MAX - 1);
					say_text.txt_channel[CHANNEL_MAX - 1] = '\0';
					strncpy(say_text.txt_username, inUsername, USERNAME_MAX - 1);
					say_text.txt_username[USERNAME_MAX - 1] = '\0';
					strncpy(say_text.txt_text, inMessage, SAY_MAX - 1);
					say_text.txt_text[SAY_MAX - 1] = '\0';

					// print to server
					printf("server: %s sends say message in %s\n", inUsername, inChannel);

					// find channel
					for (int i = 0; i < chanList->size; i++) {
						if (strcmp(inChannel, chanList->channels[i].name) == 0) {
							// for each user on channel
							for (int j = 0; j < chanList->channels[i].size; j++) {
								if (compareSocket(chanList->channels[i].users[j].address, clientAddr)) {
										}
								// send message to user
								bytes = sendto(socketfd, &say_text, sizeof(say_text), 0, (struct sockaddr *)&chanList->channels[i].users[j].address, sizeof(struct sockaddr_in));

								if (bytes < 0) {
									printf("error sending bytes\n");
								}
							}
							break;
						}
					}
					}

					else {
					//	printf("server: user not logged in, dropping packet\n");
					}
					}
					// bad packet
					else {
						printf("server: Bad packet, expecting packet of size 100, got %ld\n", inbytes);


						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;
				
						snprintf(errorBuff, SAY_MAX, "Error: Sent bad Say Packet");
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';
						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("error sending bytes\n");
						}
					
					}


				}

				// unknown request
				else {
						printf("Server: recieved Unknown packet\n");

						// fill struct
						char errorBuff[SAY_MAX];
						// pack error struct
						struct text_error error_text;
						error_text.txt_type = TXT_ERROR;

						snprintf(errorBuff, SAY_MAX, "Error: sent Unknown Packet");
						strncpy(error_text.txt_error, errorBuff, SAY_MAX - 1);
						error_text.txt_error[SAY_MAX - 1] = '\0';

						// send error to client
						bytes = sendto(socketfd, &error_text, sizeof(error_text), 0, (struct sockaddr *)&clientAddr, sizeof(clientAddr));
						if (bytes < 0) {
							perror("error sending bytes\n");
						}

				}

		}


		}


	}

}
