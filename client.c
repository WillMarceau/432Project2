#include <sys/socket.h>
#include <netinet/in.h>

#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "duckchat.h"
#include "raw.h"
// need this for to go into non-cannonical mode
#include <termios.h>



// struct to keep track of channels for user
typedef struct {
	char **channels;
	int capacity;
	int size;
} channelList;


// helper functions

void addChannel(channelList *list, char *channelName) {
	// this function adds a channel name to the users channels
	// Will resize the array if more room is needed
	
	// resize array if need capacity
	if (list->size >= list->capacity) {
		list->capacity *= 2;
		list->channels = (char **)realloc(list->channels, list->capacity * sizeof(char *));

		// error checking
		if (list->channels == NULL) {
			printf("Failed to resize array\n");
		}
	}

	// add new channel to array
	list->channels[list->size] = (char *)malloc(CHANNEL_MAX * sizeof(char));

	// error checking
	if (list->channels[list->size] == NULL) {
		printf("failed to allocate space for channel name\n");
		return;
	}
	strncpy(list->channels[list->size], channelName, CHANNEL_MAX - 1);
	list->channels[list->size][CHANNEL_MAX - 1] = '\0';
	list->size++;
	return;
}

void removeChannel(channelList *list, char *channelName) {

	// find channel in list
	for (int i = 0; i < list->size; i++) {
		if (strcmp(list->channels[i], channelName) == 0) {

			// free memory
			free(list->channels[i]);

			// shift elements to fix gap
			for (int j = i; j < list->size - 1; j++) {
				list->channels[j] = list->channels[j + 1];
			}

			// null last swap
			list->channels[list->size - 1] = NULL;

			// decrease size
			list->size--;
			
			return;
		}
	}
}

void freeChannelList(channelList *list) {
	for (int i = 0; i < list->size; i++) {
		free(list->channels[i]);
	}
	free(list->channels);
	free(list);
}

int main(int argc, char *argv[]) {
	if (argc != 4) {
		printf("Usage: ./client server_socket server_port username\n");
	}
	else {
		// start non-canonical mode
		if (raw_mode() == -1) {
			perror("error entering raw_mode\n");
			return -1;
		}

		// restore terminal mode at exit
		atexit(cooked_mode);

		// extract command line arguments
		const char *serverHost = argv[1];
		char *serverPortString = argv[2];
		
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
		
		

		// create socket
		int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
		if (socketfd == -1) {
			perror("socket Failure");
		}
		
		// create surver add struct for connection
		struct sockaddr_in clientAddr;
		socklen_t clientLength = sizeof(clientAddr);

		// create login request
		struct request_login login_request;
		login_request.req_type = REQ_LOGIN;

		// copy USERNAME_MAX chars from username pointer to request username
		strncpy(login_request.req_username, argv[3], USERNAME_MAX - 1);
		login_request.req_username[USERNAME_MAX - 1] = '\0';
		fflush(stdout);

		// send login request
		ssize_t bytes = sendto(socketfd, &login_request, sizeof(login_request), 0, resolution->ai_addr, resolution->ai_addrlen);
		
		if (bytes < 0) {
			perror("failed to send log in request");
			close(socketfd);
		}

		// connect to common channel 
		struct request_join join_common;
		join_common.req_type = REQ_JOIN;
		strncpy(join_common.req_channel, "Common", CHANNEL_MAX - 1);
		join_common.req_channel[CHANNEL_MAX - 1] = '\0';

		// send common request
		bytes = sendto(socketfd, &join_common, sizeof(join_common), 0, resolution->ai_addr, resolution->ai_addrlen);
		if (bytes < 0) {
			perror("failed to send log in request");
			close(socketfd);
		}

		
		char buff[SAY_MAX];
		char message[SAY_MAX];
		char copy[SAY_MAX];
		// each client needs own active channel list

		//struct request clientRequest;
		char *tokens[SAY_MAX];
	       	int count;
		char name[] = "Common";
		char activeChannel[CHANNEL_MAX];
		strcpy(activeChannel, "Common");


		// create channel list for client
		channelList *list = (channelList *)malloc(sizeof(channelList));
		if (list == NULL) {
			printf("error mallocing user channel list\n");
			return -1;
		}
		list->capacity = 2;
		list->size = 0;
		list->channels = (char **)malloc(2 * sizeof(char *));

		addChannel(list, name);
		int search;
		fd_set readfds;
		char inBuff[2048];
		char ch;
		char inChannelName[CHANNEL_MAX];
		char inUsername[USERNAME_MAX];
		char inMessage[SAY_MAX];
		int index = 0;

		printf(">");
		fflush(stdout);

		// start request loop
		while (true) {
			count = 0;
			
			// initialize fd set
			FD_ZERO(&readfds);

			// set stdin and socket
			FD_SET(0, &readfds);
			FD_SET(socketfd, &readfds);

			// get max file descriptor
			int max = socketfd > 0 ? socketfd : 0;
			if (select(max + 1, &readfds, NULL, NULL, NULL) < 0) {
				perror("problem with select\n");
				break;
			}

			// check for socket input from server
			if (FD_ISSET(socketfd, &readfds)) {
				ssize_t inbytes = recvfrom(socketfd, inBuff, sizeof(inBuff) - 1, 0, (struct sockaddr *)&clientAddr, &clientLength);	
				if (inbytes < 0) {
					perror("Error reciving from server\n");
				}
				
				else {
					// deal with when a client had started typing before message came in?
					if (index > 0) {
						fflush(stdout);

						// print backspaces
						for (int i = 0; i < index; i++) {
							printf("\b \b");
						}
						fflush(stdout);

					}

					// remove prompt
					printf("\b \b");
					fflush(stdout);

					// create base struct
					struct text *baseResponse = (struct text *)inBuff;

					// read id
					text_t type = baseResponse->txt_type;

					// message response
					if (type == TXT_SAY) {

						// check packet
						if (inbytes == 132) {

						// cast to say stuct
						struct text_say *say = (struct text_say*)baseResponse;

						// extract info
						strncpy(inChannelName, say->txt_channel, CHANNEL_MAX - 1);
						inChannelName[CHANNEL_MAX - 1] = '\0';
						strncpy(inUsername, say->txt_username, USERNAME_MAX - 1);
						inUsername[CHANNEL_MAX - 1] = '\0';
						strncpy(inMessage, say->txt_text, SAY_MAX - 1);
						inMessage[SAY_MAX - 1] = '\0';
						

						// print the message
						printf("[%s][%s]: %s\n", inChannelName, inUsername, inMessage);
						}

						else {
							printf("Error: expected packet length was 132, got %ld\n", inbytes);
						}



					}

					// channel list response
					else if (type == TXT_LIST) {

						struct text_list *inList = (struct text_list *)baseResponse;

						// extract info
						int numChannel =inList->txt_nchannels;
						
						// check packet
						long list_size  = 32 * numChannel;

						if (inbytes == (8 + list_size)) {

						// print setup
						printf("Existing channels:\n");

						// loop over channels
						for (int i = 0; i < numChannel; i++) {
							printf(" %s\n", inList->txt_channels[i].ch_channel);
						}
						}
						else {
							printf("Error: expected packet length was %ld, got %ld\n", (list_size + 8), inbytes);
						}

					}

					// who responsea
					else if (type == TXT_WHO) {
						// cast to who struct
						struct text_who *who = (struct text_who *)baseResponse;

						// extract info
						int numUsers = who->txt_nusernames;

						// check packet
						long list_size = 32 * numUsers;
						if (inbytes == (40 + list_size)) {
						
						// finish extracting info
						strncpy(inChannelName, who->txt_channel, CHANNEL_MAX - 1);
						inChannelName[CHANNEL_MAX - 1] = '\0';
						
						// print setup
						printf("Users on channel %s:\n", inChannelName);

						// loop over Users
						for (int i = 0; i < numUsers; i++) {
							printf(" %s\n", who->txt_users[i].us_username);
						}
						}
						else {
							printf("Error: expected packet length was %ld, got %ld\n", (list_size + 8), inbytes);
						}


					}
					
					// error response
					else if (type == TXT_ERROR) {

						// check packet size
						if (inbytes == 68) {
						// cast to error struct
						struct text_error *error = (struct text_error *)baseResponse;

						// extract info
						strncpy(inMessage, error->txt_error, SAY_MAX - 1);
						inMessage[SAY_MAX - 1] = '\0';
						
						// print error
						printf("%s\n", inMessage);
						}

						else {
							printf("Error: expected packet length was 68, got %ld\n", inbytes);
						}
					}
					// unkown packet
					else {
						printf("error: Recieved Unkown Packet\n");
					}
				}

				// print user input again
				printf(">");
				fflush(stdout);
				for (int i = 0; i < index; i++) {
					printf("%c", buff[i]);
					fflush(stdout);
				}
					
					

			}
			// check for user input
			if (FD_ISSET(0, &readfds)) {
	
				// read a char
				int del = 0;
				if (read(STDIN_FILENO, &ch, 1) > 0) {
					if (ch == '\n') {
						printf("\n");
					}
					// deal with backspace
					else if (ch == 127 || ch == 8) {
						del = 1;
						if (index > 0) {
							index--;
							buff[index] = '\0';
							printf("\b \b");
							fflush(stdout);
						}
							
					}
					else if (index < SAY_MAX - 1) {
						// store typed char
						buff[index++] = ch;
						printf("%c", ch);
						fflush(stdout);
					}
				}

				if (ch == '\n') {
				// clean-up
				buff[index] = '\0';
				ch = '\0';
				index = 0;

			// copy message
			strncpy(message, buff, SAY_MAX - 1);
			message[SAY_MAX] = '\0';
			strncpy(copy, buff, SAY_MAX - 1);
			copy[SAY_MAX] = '\0';

			// split the input into tokens
			char *token = strtok(copy, " ");
			count = 0;
			while (token != NULL) {

				// testing here to remove new line
				size_t len = strlen(token);
				if (len > 0 && token[len - 1] == '\n') {
					token[len - 1] = '\0';
				}

				tokens[count++] = token;
				token = strtok(NULL, " ");
			}

			buff[0] = '\0';

			if (count == 0) {
				char empty[] = "";
				tokens[0] = empty;
			}



			// check for / first so we can handle Unknown Commands
			if (tokens[0][0] == '/') {

				// logout
				if (strcmp(tokens[0], "/exit") == 0) {

					// check # of arguments
					if (count != 1) {
						printf("*Unknown command\n");
					}

					else {

						// populate the logout struct
						struct request_logout logout_request;
			       			logout_request.req_type = REQ_LOGOUT;	
				
						// send the logout command
						bytes = sendto(socketfd, &logout_request, sizeof(logout_request), 0, resolution->ai_addr, resolution->ai_addrlen);
						if (bytes < 0) {
							perror("Failed to send\n");
						}

						// free mem
						freeChannelList(list);
						freeaddrinfo(resolution);

			
						// exit the loop
						break;
					}
				}

				// join channel
				else if (strcmp(tokens[0], "/join") == 0) {
				
					// check # of arguments
					if (count != 2) {
						printf("*Unknown command\n");
					}

					else {

						//populate join struct
						struct request_join join_request;
						join_request.req_type = REQ_JOIN;
						strncpy(join_request.req_channel, tokens[1], CHANNEL_MAX - 1);
						join_request.req_channel[CHANNEL_MAX - 1] = '\0';


						// send join request
						bytes = sendto(socketfd, &join_request, sizeof(join_request), 0, resolution->ai_addr, resolution->ai_addrlen);
						if (bytes < 0) {
							perror("Failed to send\n");
						}

						addChannel(list, tokens[1]);
						strncpy(activeChannel, tokens[1], CHANNEL_MAX - 1);
						activeChannel[CHANNEL_MAX - 1] = '\0';
					}
				}


				// leave channel
				else if (strcmp(tokens[0], "/leave") == 0) {

					// check # of arguments
					if (count != 2) {
						printf("*Unknown command\n");
					}

					else {
					
						// populate leave struct
						struct request_leave leave_request;
						leave_request.req_type = REQ_LEAVE;
						strncpy(leave_request.req_channel, tokens[1], CHANNEL_MAX - 1);
						leave_request.req_channel[CHANNEL_MAX - 1] = '\0';
				
						// send leave request
						bytes = sendto(socketfd, &leave_request, sizeof(leave_request), 0, resolution->ai_addr, resolution->ai_addrlen);
						if (bytes < 0) {
							perror("Failed to send\n");
						}

						// if active leaving active channel make none
						if (strcmp(activeChannel, tokens[1]) == 0) {
							strcpy(activeChannel, "\0");
						}

						// might not have to check for the channel first
						// depending on error message being sent from server
						for (int i = 0; i < list->size; i++) {
							if (strcmp(list->channels[i], tokens[1]) == 0) {
								removeChannel(list, tokens[1]);
							}
						}
					}
				}

				// list all channels
				else if (strcmp(tokens[0], "/list") == 0) {
					// check # of args
					if (count != 1) {
						printf("*Unknown command\n");
					}

					else {

						// populate the lsit struct
						struct request_list list_request;
						list_request.req_type = REQ_LIST;
	
						// send list request
						bytes = sendto(socketfd, &list_request, sizeof(list_request), 0, resolution->ai_addr, resolution->ai_addrlen);
						if (bytes < 0) {
							perror("Failed to send\n");
						}
					}


				}

				// list users on specific channel
				else if (strcmp(tokens[0], "/who") == 0) {

					// check # of args
					if (count != 2) {
						printf("*Unknown command\n");
					}
					
					else {
						// populate the who struct
						struct request_who who_request;
						who_request.req_type = REQ_WHO;
						strncpy(who_request.req_channel, tokens[1], CHANNEL_MAX - 1);
						who_request.req_channel[CHANNEL_MAX - 1] = '\0';

						// send the who request
						bytes = sendto(socketfd, &who_request, sizeof(who_request), 0, resolution->ai_addr, resolution->ai_addrlen);
						if (bytes < 0) {
							perror("Failed to send\n");
						}

					}

				}


				// switch to existing channel
				else if (strcmp(tokens[0], "/switch") == 0) {

					// check # of args
					if (count != 2) {
						printf("*Unknown command\n");
					}
					
					else {
						// need to keep track of users active channels 
						// no server call for this one
						//
						// check if channel in active channels
						search = 0;
						for (int i = 0; i < list->size; i++) {
							if (strcmp(list->channels[i], tokens[1]) == 0) {

								// set new active channel
								strncpy(activeChannel, tokens[1], CHANNEL_MAX - 1);
								activeChannel[CHANNEL_MAX - 1] = '\0';
								search = 1;
							}
						}

						if (search == 0) {
							printf("You have not subscribed to channel %s\n", tokens[1]);
						}
						search = 0;


					}


				}
			
				// Unknown command
				else {
					printf("*Unknown Command\n");
					fflush(stdout);
				}
			}

			else {
				// send message
				
				// check if the user has an active channel
				if (strcmp(activeChannel, "\0") != 0) {
					// populate the say struct
					struct request_say say_request;
					say_request.req_type = REQ_SAY;
					strncpy(say_request.req_channel, activeChannel, CHANNEL_MAX - 1);
					say_request.req_channel[CHANNEL_MAX - 1] = '\0';
					strncpy(say_request.req_text, message, SAY_MAX - 1);
					say_request.req_text[SAY_MAX - 1] = '\0';

					// send the message
					
					bytes = sendto(socketfd, &say_request, sizeof(say_request), 0, resolution->ai_addr, resolution->ai_addrlen);
					if (bytes < 0) {
						perror("Failed to send\n");
					}
				}



			}
			}
			if (index == 0 && del != 1) {
			// print prompt
				printf(">");
				fflush(stdout);

			}
			}
		}
	}

}
