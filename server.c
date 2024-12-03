#include <sys/socket.h>
#include <netinet/in.h>

#include <signal.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "duckchat.h"
#include <fcntl.h>
#include <netdb.h>
#include <sys/time.h>
#include <pthread.h>

// for error checking
#include <errno.h>

pthread_mutex_t lock;

// identifier struct
typedef struct {
	char recent_ids[MAX_IDENTIFIERS][IDENTIFY_MAX];
	int head;
	int size;
} identifierList;

// struct for subbed channel array
typedef struct {
	char name[CHANNEL_MAX];
	int timer;
	int last_join;
} channel_sub;

// server struct
typedef struct {
	struct sockaddr_in address;
	char host[INET_ADDRSTRLEN];
	int port;
	//char **channels;
	channel_sub *channels;
	int channel_size;
	int channel_cap;
} server;

// struct for server List
typedef struct {
	server *servers;
	int server_size;
	int server_cap;
} serverList;

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

// thread struct
typedef struct {
	serverList *l;
	server *s;
	int socket;
} threadInfo;

volatile sig_atomic_t timer_flag = 0;
volatile sig_atomic_t exiting = 0;

// helper functions
/*
void addUser(channelList *list, char *channelName, char *userName, struct sockaddr_in address) {
	// this function is for adding a user to an existing 
	// find channel in list
}
*/

//void signal_handler(int sig) {
	// sets shutdown
	//(void)sig;
	//exiting = 1;
//}

void timerHandler(int signum) {
	// update flag
	(void)signum;
	timer_flag = 1;
}

void setupTimer() {
	// setup timeout timer
	
	struct sigaction sa;
	struct itimerval timer;

	// clear space and init
	memset(&sa, 0, sizeof(sa));
	sa.sa_handler = timerHandler;
	sigaction(SIGALRM, &sa, NULL);

	timer.it_value.tv_sec = 1;
	timer.it_value.tv_usec = 0;
	timer.it_interval.tv_sec = 5;
	timer.it_interval.tv_usec = 0;

	// start
	setitimer(ITIMER_REAL, &timer, NULL);
}

void addIdentifier(identifierList *buff, char *id) {
	// adds an identifier to the identifier list
	
	// overwrite oldest entry if buff is full
	if (buff->size == MAX_IDENTIFIERS) {
		buff->head = (buff->head + 1) % MAX_IDENTIFIERS;
	} else {
		buff->size++;
	}
	int index = (buff->head + buff->size - 1) % MAX_IDENTIFIERS;
	strncpy(buff->recent_ids[index], id, IDENTIFY_MAX - 1);
	buff->recent_ids[index][IDENTIFY_MAX - 1] = '\0';
}

void initServerList(serverList *list) {
	// initializes the servers server list
	
	list->servers = (server *)malloc(2 * sizeof(server));

	// error checking
	if (!list->servers) {
		perror("Failed to allocate mem for serverList");
		exit(EXIT_FAILURE);
	}

	// set vals
	list->server_size = 0;
	list->server_cap = 2;
}

void initServer(server *s) {
	// initializes the servers channel list 
	//s->channels = (char **)malloc(2 * sizeof(char *));
	s->channels = (channel_sub *)malloc(2 * sizeof(channel_sub));

	// error checking
	if (!s->channels) {
		perror("Failed to allocate memory for channels");
		exit(EXIT_FAILURE);
	}
	s->channel_size = 0;
	s->channel_cap = 2;
}

void addServer(serverList *list, struct sockaddr_in address, char *host, int port) {
	// adds a server to the server list
	
	// resize if need capacity
	if (list->server_size >= list->server_cap) {
		list->server_cap *= 2;
		list->servers = (server *)realloc(list->servers, list->server_cap * sizeof(server));
		// error checking
		if (!list->servers) {
			perror("Failed to reallocate mem for serverList");
			exit(EXIT_FAILURE);
		}
	}

	// add the new server 
	server *s = &list->servers[list->server_size++];
	s->address = address;
	strncpy(s->host, host, INET_ADDRSTRLEN - 1);
	s->host[INET_ADDRSTRLEN - 1] = '\0';
	s->port = port;

	initServer(s);
	
}

void addChannel(server *s, char *name) {
	// adds a subscribed channel to a server
	
	// resize if need capacity
	if (s->channel_size >= s->channel_cap) {
		s->channel_cap *= 2;
		//s->channels = (char **)realloc(s->channels, s->channel_cap * sizeof(char *));
		s->channels = (channel_sub *)realloc(s->channels, s->channel_cap * sizeof(channel));
		// error checking
		if (!s->channels) {
			perror("Failed to reallocate mem for server channels");
			exit(EXIT_FAILURE);
		}
	}
	//s->channels[s->channel_size].name = (char *)malloc(CHANNEL_MAX * sizeof(char));

	// error checking
	//if (!s->channels[s->channel_size]) {
		//perror("Failed to allocate mem for channel name");
		//return;
	//}

	// add channel
	strncpy(s->channels[s->channel_size].name, name, CHANNEL_MAX - 1);
	//s->channels[s->channel_size][CHANNEL_MAX - 1] = '\0';
	s->channels[s->channel_size].name[CHANNEL_MAX - 1] = '\0';
	s->channels[s->channel_size].timer = 60;
	s->channels[s->channel_size].last_join = 120;
	s->channel_size++;
}

void removeChannel(server *s, char *name) {
	// removes a subscribed channel from a servers list
	
	// find channel
	for (int i = 0; i < s->channel_size; i++) {
		//if (strcmp(s->channels[i], name) == 0) {
		if (strcmp(s->channels[i].name, name) == 0) {
			// remove channel
			//free(s->channels[i]);

			// shift
			for (int j = i; j < s->channel_size - 1; j++) {
				s->channels[j] = s->channels[j + 1];
			}

			s->channel_size--;
			break;
		}
	}

}

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

void getIdentifier(char *buff) {
	// func to read random bytes fro /dev/urandom into a buff
	
	// open /dev/urandom
	int urandomfd = open("/dev/urandom", O_RDONLY);
	if (urandomfd < 0) {
		perror("error opening /dev/urandom");
		exit(EXIT_FAILURE);
	}

	// read bytes into buff
	ssize_t result = read(urandomfd, buff, IDENTIFY_MAX - 1);
	
	// error checking
	if (result < 0) {
		perror("error reading from /dev/urandom");
		close(urandomfd);
		exit(EXIT_FAILURE);
	}
	buff[IDENTIFY_MAX - 1] = '\0';
	close(urandomfd);	
}

/*
void handleTimers(server *myServ, serverList *others) {
	// func to handle channel timers
	
	// loop over my channels
	for (int i = 0; i < myServ->channel_size; i++) {
		if (myServ->channels[i].timer > 0) {
			myServ->channels[i].timer -= 5;
			printf("mine: %d\n", myServ->channels[i].timer);
		}

		// resend if needed
		if (myServ->channels[i].timer == 0) {
			myServ->channels[i].timer = 60;
			
			// send renew
			printf("send S2S renew %s\n", myServ->channels[i].name);
		}
	}

	// loop over other channels
	for (int i = 0; i < others->server_size; i++) {
		for (int j = 0; j < others->servers[i].channel_size; j++) {
			//printf("S: %d\n", others->servers[i].channels[j].last_join);
			if (others->servers[i].channels[j].last_join > 0) {
				others->servers[i].channels[j].last_join -= 5;
				printf("Others: %d\n", others->servers[i].channels[j].last_join);

			}

			// remove if needed
			if (others->servers[i].channels[j].last_join == 0) {
				// remove
				//printf("forcfully removing %s\n", others->servers[i].channels[j].name);
			}
		}
	}
	
}
*/

void *handleTimers(void *arg) {
	// function for timeout thread

	// sleep for second

	// get mutex
	threadInfo *info = (threadInfo *)arg;

	while (!exiting) {
		sleep(1);
		pthread_mutex_lock(&lock);
	//	printf("%s\n", info->s->host);
		for (int i = 0; i < info->l->server_size; i++) {
			//for (int j = 0; j < info->l->servers[i].channel_size; j++) {
			for (int j = info->l->servers[i].channel_size - 1; j >= 0; j--) {
				if (info->l->servers[i].channels[j].last_join > 0) {
					info->l->servers[i].channels[j].last_join--;
				}

				// remove if timout
				if (info->l->servers[i].channels[j].last_join == 0) {
					//printf("removing channel\n");
					printf("%s:%d %s:%d forcefully removing %s\n", info->s->host, info->s->port, info->l->servers[i].host, info->l->servers[i].port, info->l->servers[i].channels[j].name);
					removeChannel(&info->l->servers[i], info->l->servers[i].channels[j].name);
				}
			}	
		}
		for (int i = 0; i < info->s->channel_size; i++) {
			if (info->s->channels[i].timer > 0) {
				info->s->channels[i].timer--;
			}

			// renewal if needed
			if (info->s->channels[i].timer == 0) {
				info->s->channels[i].timer = 60;
				//printf("%s:%d sending renewal\n");

				// send renewal
				// create the s2s join message
				int bytes;
				struct s2s_join join_s2s;
				join_s2s.req_type = S2S_JOIN;
				strncpy(join_s2s.req_channel, info->s->channels[i].name, CHANNEL_MAX - 1);
				join_s2s.req_channel[CHANNEL_MAX - 1] = '\0';

				// send the s2s join message
				for (int k = 0; k < info->l->server_size; k++) {

					int search = 0; 
					// add channel to server if not in it already
					for (int j = 0; j < info->l->servers[k].channel_size; j++) {
						//if (strcmp(servList.servers[i].channels[j], inChannel) == 0) {
						if (strcmp(info->l->servers[k].channels[j].name, info->s->channels[i].name) == 0) {
							search = 1;
						}
					}

					if (!search) {
						addChannel(&info->l->servers[k], info->s->channels[i].name);
					}


					printf("%s:%d %s:%d send renew S2S Join %s\n", info->s->host, info->s->port, info->l->servers[k].host, info->l->servers[k].port, info->s->channels[i].name);
					bytes = sendto(info->socket, &join_s2s, sizeof(join_s2s), 0, (struct sockaddr *)&info->l->servers[k].address, sizeof(struct sockaddr_in));

					if (bytes < 0) {
						printf("error sending bytes\n");
					}

				}
			}
		}
		pthread_mutex_unlock(&lock);
		
	}
	return NULL;
}

/*
void *handleRenewal(void *arg) {
	// func for Join Renewal thread
	
	sleep(1);
	while (!exiting) {
		for (int i = 0; i < myServ->channel_size; i++) {
			if (myServ->channels[i].timer > 0) {
				myServ->channels[i].timer--;
			}

			// renewal if needed
			if (myServ->channels[i].timer == 0) {
				s->channels[i].timer = 60;
				printf("sending renewal\n");

				// send renewal
			}
		}
	}
}
*/



int main(int argc, char *argv[]) {

	if (argc < 3) {
		printf("Usage: ./server <i>_domain_name <i>_ port_num\n");
		return -1;
	}
	else if ((argc - 1) % 2 != 0) {
		printf("Usage: each server must have a both a domain name and port num\n");
		return -1;
	}

	// create lock
	if (pthread_mutex_init(&lock, NULL) != 0) {
		perror("Mutex init failed");
		return EXIT_FAILURE;
	}

	//signal(SIGINT, signal_handler);
	
	pthread_t timer_thread;

	// extract command line arguments
	const char *serverHost = argv[1];
	char *serverPortString = argv[2];

	// deal with local host input
	//if (strcmp(serverHost, "localhost") == 0) {
		//serverHost = "127.0.0.1";
	//}
	
	// set up server list

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
	//int serverPort = atoi(serverPortString);

	// create server socket
	int socketfd = socket(AF_INET, SOCK_DGRAM, 0);
	if (socketfd == -1) {
		perror("Socket Failure");
	}

	// set up serverList
	serverList servList;
	initServerList(&servList);

	// loop through the inputed arguments
	for (int i = 3; i < argc; i += 2) {
		//printf("%s\n", argv[i]);
		const char *comHost = argv[i];
		//printf("%s\n", argv[i + 1]);
		char *comPortString = argv[i + 1];

		// resolve hostname
		struct addrinfo h, *res;
		memset(&h, 0, sizeof(h));
		h.ai_family = AF_INET;
		h.ai_socktype = SOCK_DGRAM;

		int comStatus = getaddrinfo(comHost, comPortString, &h, &res);

		// error checking
		if (comStatus != 0) {
			perror("Failed to resolve host name");
			return -1;
		}
		
		// get addr
		struct sockaddr_in *addr = (struct sockaddr_in *)res->ai_addr;

		// get host string
		char comHostString[INET_ADDRSTRLEN];
		strncpy(comHostString, inet_ntoa(addr->sin_addr), INET_ADDRSTRLEN - 1);
		comHostString[INET_ADDRSTRLEN - 1] = '\0';

		// get port num
		int comSocketString = ntohs(addr->sin_port);

		//printf("%s:%d\n", comHostString, comSocketString);
		fflush(stdout);



		// add the server
		addServer(&servList, *addr, comHostString, comSocketString);
		freeaddrinfo(res);
		
	}

	// set non blocking mode
	int flags = fcntl(socketfd, F_GETFL, 0);
	fcntl(socketfd, F_SETFL, flags | O_NONBLOCK);

	// create server addr and client addr struct
	struct sockaddr_in clientAddr;
	socklen_t clientLength = sizeof(clientAddr);

	// clean struct
	//memset(&serverAddr, 0, sizeof(serverAddr));
	//serverAddr.sin_family = AF_INET;
	//serverAddr.sin_port = htons(serverPort);
	//serverAddr.sin_addr.s_addr = INADDR_ANY;

	// bind socket
	//if (bind(socketfd, (struct sockaddr *)&serverAddr, sizeof(serverAddr)) < 0) {
	if (bind(socketfd, resolution->ai_addr, resolution->ai_addrlen) == -1) {
		perror("bind failure\n");
		close(socketfd);
		return -1;
	}

	// free the resolution 
	freeaddrinfo(resolution);

	// cast for name extraction
	struct sockaddr_in *serverAddr = (struct sockaddr_in *)resolution->ai_addr;

	// get host name and socket for log
	char serverHostString[INET_ADDRSTRLEN];
	strncpy(serverHostString, inet_ntoa(serverAddr->sin_addr), INET_ADDRSTRLEN - 1);
	serverHostString[INET_ADDRSTRLEN - 1] = '\0';
	
	int serverSocketString = ntohs(serverAddr->sin_port);

	// set up own server
	server myServer;
	initServer(&myServer);
	
	//myServer.host = serverHostString;
	strncpy(myServer.host, serverHostString, INET_ADDRSTRLEN - 1);
	myServer.host[INET_ADDRSTRLEN- 1] = '\0';
	myServer.port = serverSocketString;


	/*printf("%s:%d\n", serverHostString, serverSocketString);
	fflush(stdout);

	for (int i = 0; i < servList.server_size; i++) {
		printf("%d\n", servList.servers[i].port);
		fflush(stdout);
	}

	printf("\n"); */

	// init variables
	fd_set readfds;
	struct timeval timeout;
	char inBuff[1024];
	char inUsername[USERNAME_MAX];
	char inChannel[CHANNEL_MAX];
	char inMessage[SAY_MAX];
	char inIdentifier[IDENTIFY_MAX];
	ssize_t bytes;
	identifierList identifiers;
	identifiers.size = 0;
	identifiers.head = 0;

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
	
	threadInfo tInfo;
	tInfo.l = &servList;
	tInfo.s = &myServer;
	tInfo.socket = socketfd;

	//setupTimer();
	//setup timer thread
	pthread_create(&timer_thread, NULL, handleTimers, (void*)&tInfo);

	// listen loop
	while(true) {

		//if (timer_flag) {
			//handleTimers(&myServer, &servList);
		//}
		// init fd set
		FD_ZERO(&readfds);

		// set socket listening
		FD_SET(socketfd, &readfds);

		// set timeout
		timeout.tv_sec = 5;
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
			pthread_mutex_lock(&lock);
			ssize_t inbytes = recvfrom(socketfd, inBuff, sizeof(inBuff) - 1, 0, (struct sockaddr *)&clientAddr, &clientLength);
		        //printf("%ld\n", inbytes);	
			inBuff[inbytes] = '\0';
			//printf("recieved from client %s:%d %s\n", inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port), inBuff);
			//fflush(stdout);
			if (inbytes < 0) {
					
				perror("error reading bytes sent to socket\n");
			}
			else {
		//	printf("Received from client %s:%d\n",
       		//	inet_ntoa(clientAddr.sin_addr), ntohs(clientAddr.sin_port));
	
		//	inet_ntop is thread save, if needed
		//
		//	get Client Host string and socket for log
				char clientHostString[INET_ADDRSTRLEN]; 
				strncpy(clientHostString, inet_ntoa(clientAddr.sin_addr), INET_ADDRSTRLEN - 1);   
				clientHostString[INET_ADDRSTRLEN - 1] = '\0'; 


				int clientSocketString = ntohs(clientAddr.sin_port);
				//printf("Recieved from %s:%d\n", clientHostString, clientSocketString);
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

					printf("%s:%d %s:%d recv Request login %s\n", serverHostString, serverSocketString, clientHostString, clientSocketString, inUsername);

					//printf("%s\n", chanList->channels[0].users[0].username);
					}
					else {
						printf("%s:%d Bad packet, expecting packet of size 36, got %ld\n", serverHostString, serverSocketString, inbytes);


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
					//printf("server: %s logs out\n", inUsername);
					printf("%s:%d %s logs out\n", serverHostString, serverSocketString, inUsername);
					int index = 0;
					//char currentName[CHANNEL_MAX];
					while(index < chanList->size) {
						//printf("index %d\n", index);
						//printf("checking %s\n", chanList->channels[index].name);
						removeUserChannel(chanList, chanList->channels[index].name, inUsername);
						

						// if channel is empty, remove channel
					      	if (chanList->channels[index].size == 0) {
							printf("%s:%d removing empty channel %s locally\n", serverHostString, serverSocketString, chanList->channels[index].name);
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
						printf("%s:%d Bad packet, expecting packet of size 4, got %ld\n", serverHostString, serverSocketString, inbytes);


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

						// user joins the channel
						printf("%s:%d %s:%d recv Request join %s %s\n", serverHostString, serverSocketString, clientHostString, clientSocketString, inUsername, inChannel);

						// add user to channel
						addToChannel(chanList, inChannel, inUsername, clientAddr);

						// check if subbed
						int subbed = 0;
						for (int i = 0; i < myServer.channel_size; i++) {
							//if (strcmp(inChannel, myServer.channels[i]) == 0) {
							if (strcmp(inChannel, myServer.channels[i].name) == 0) {
								subbed = 1;
								break;
							}

						}

						if (!subbed) {

							// add to subbed channels
							addChannel(&myServer, inChannel);

							// create the s2s join message
							struct s2s_join join_s2s;
							join_s2s.req_type = S2S_JOIN;
							strncpy(join_s2s.req_channel, inChannel, CHANNEL_MAX - 1);
							join_s2s.req_channel[CHANNEL_MAX - 1] = '\0';

							// send the s2s join message
							for (int i = 0; i < servList.server_size; i++) {

								int search = 0; 
								// add channel to server if not in it already
								for (int j = 0; j < servList.servers[i].channel_size; j++) {
									//if (strcmp(servList.servers[i].channels[j], inChannel) == 0) {
									if (strcmp(servList.servers[i].channels[j].name, inChannel) == 0) {
										search = 1;
									}
								}

								if (!search) {
									addChannel(&servList.servers[i], inChannel);
								}


								printf("%s:%d %s:%d send S2S Join %s\n", serverHostString, serverSocketString, servList.servers[i].host, servList.servers[i].port, inChannel);
								bytes = sendto(socketfd, &join_s2s, sizeof(join_s2s), 0, (struct sockaddr *)&servList.servers[i].address, sizeof(struct sockaddr_in));

								if (bytes < 0) {
									printf("error sending bytes\n");
								}

							}
						}

					}
					}
					else {
						//printf("server: user not logged in, dropping packet\n");
					}
					}
					
					// bad packet
					else {
						printf("%s:%d Bad packet, expecting packet of size 36, got %ld\n", serverHostString, serverSocketString, inbytes);


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
						printf("%s:%d %s leaves channel %s\n", serverHostString, serverSocketString, inUsername, inChannel);
						removeUserChannel(chanList, inChannel, inUsername);

						// if channel is empty, remove channel
						for (int i = 0; i < chanList->size; i++) {
							if (strcmp(inChannel, chanList->channels[i].name) == 0) {
								if (chanList->channels[i].size == 0) {
									printf("%s:%d %s channel empty removing locally\n", serverHostString, serverSocketString, chanList->channels[i].name);
									fflush(stdout);
									removeChannel(chanList, chanList->channels[i].name);
								}
							}
						}
					}

					// send error if not in channel
					else if (exists && !found) {
						printf("%s:%d %s is trying to leave channel %s where he/she is not a member\n", serverHostString, serverSocketString, inUsername, inChannel);
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
						printf("%s:%d %s trying to leave a non-existent channel %s\n", serverHostString, serverSocketString, inUsername, inChannel);
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
						printf("%s:%d Bad packet, expecting packet of size 36, got %ld\n", serverHostString, serverSocketString, inbytes);
						//printf("leave\n");
						fflush(stdout);


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
					printf("%s:%d %s lists channels\n", serverHostString, serverSocketString, inUsername);
					}

					else {
						//printf("server: user not logged in, dropping packet\n");
					}
					}

					// bad packet
					else {
						printf("%s:%d Bad packet, expecting packet of size 4, got %ld\n", serverHostString, serverSocketString, inbytes);


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

						printf("%s:%d %s lists users in channel %s\n", serverHostString, serverSocketString, inUsername, inChannel);
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
						printf("%s:%d %s trying to list users in non-existing channel %s\n", serverHostString, serverSocketString, inUsername, inChannel);

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

						printf("%s:%d Bad packet, expecting packet of size 36, got %ld\n", serverHostString, serverSocketString, inbytes);


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
					//printf("server: %s sends say message in %s\n", inUsername, inChannel);
					printf("%s:%d %s:%d recv Request say %s %s \"%s\"\n", serverHostString, serverSocketString, clientHostString, clientSocketString, inUsername, inChannel, inMessage);

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

					// create s2s say message
					struct s2s_say say_s2s;
					say_s2s.req_type = S2S_SAY;
					getIdentifier(say_s2s.unique);
					strncpy(say_s2s.req_username, inUsername, USERNAME_MAX - 1);
					say_s2s.req_username[USERNAME_MAX - 1] = '\0';
					strncpy(say_s2s.req_channel, inChannel, CHANNEL_MAX - 1);
					say_s2s.req_channel[USERNAME_MAX - 1] = '\0';
					strncpy(say_s2s.req_text, inMessage, SAY_MAX - 1);
					say_s2s.req_text[SAY_MAX - 1] = '\0';

					// find subbed servers
					for (int i = 0; i < servList.server_size; i++) {
						for (int j = 0; j < servList.servers[i].channel_size; j++) {
							//if (strcmp(servList.servers[i].channels[j], inChannel) == 0) {
							if (strcmp(servList.servers[i].channels[j].name, inChannel) == 0) {
								// log
								printf("%s:%d %s:%d send S2S say %s %s \"%s\"\n", serverHostString, serverSocketString, servList.servers[i].host, servList.servers[i].port, inUsername, inChannel, inMessage);
								// send message
								bytes = sendto(socketfd, &say_s2s, sizeof(say_s2s), 0, (struct sockaddr *)&servList.servers[i].address, sizeof(struct sockaddr_in));

								if (bytes < 0) {
									printf("error sending bytes \n");
								}
							}
						}

					}



					}
					else {
					//	printf("server: user not logged in, dropping packet\n");
					}
					}
					// bad packet
					else {
						printf("%s:%d Bad packet, expecting packet of size 100, got %ld\n", serverHostString, serverSocketString, inbytes);


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
				
				// handle s2s join message
				else if (type == S2S_JOIN) {
					if (inbytes == 36) {
					// extract info 
					struct s2s_join *join_s2s = (s2s_join *)baseRequest;

					strncpy(inChannel, join_s2s->req_channel, CHANNEL_MAX - 1);
					inChannel[CHANNEL_MAX - 1] = '\0';

					printf("%s:%d %s:%d recv S2S Join %s\n", serverHostString, serverSocketString, clientHostString, clientSocketString, inChannel);

					// add subscription to corresponding server record and update the last_join
					for (int i = 0; i < servList.server_size; i++) {
						if (compareSocket(clientAddr, servList.servers[i].address)) {
							int exists = 0;
							// check if channel already exists
							for (int j = 0; j < servList.servers[i].channel_size; j++) {
								if (strcmp(servList.servers[i].channels[j].name, inChannel) == 0) {
									servList.servers[i].channels[j].last_join = 120;
									exists = 1;
									break;
								}
							}
							if (!exists) {
								addChannel(&servList.servers[i], inChannel);
							}
							break;
						}
					}
					int subbed = 0;
					// check if server is subscribed
					for (int i = 0; i < myServer.channel_size; i++) {
						// if subbed
						//if (strcmp(inChannel, myServer.channels[i]) == 0) {
						if (strcmp(inChannel, myServer.channels[i].name) == 0) {
							subbed = 1;
						}
					}

					// sub and send join further if not subbed
					if (!subbed) {
						// add to subbed channels
						addChannel(&myServer, inChannel);

						// create the s2s join message
						struct s2s_join out_s2s;
						out_s2s.req_type = S2S_JOIN;
						strncpy(out_s2s.req_channel, inChannel, CHANNEL_MAX - 1);
						out_s2s.req_channel[CHANNEL_MAX - 1] = '\0';

						// send the s2s join message down the treed
						for (int i = 0; i < servList.server_size; i++) {
							if (!compareSocket(clientAddr, servList.servers[i].address)) {
								int search = 0; 
								// add channel to server if not in it already
								for (int j = 0; j < servList.servers[i].channel_size; j++) {
									//if (strcmp(servList.servers[i].channels[j], inChannel) == 0) {
									if (strcmp(servList.servers[i].channels[j].name, inChannel) == 0) {
										search = 1;
									}
								}

								if (!search) {
									addChannel(&servList.servers[i], inChannel);
								}

								printf("%s:%d %s:%d send S2S Join %s\n", serverHostString, serverSocketString, servList.servers[i].host, servList.servers[i].port, inChannel);
								bytes = sendto(socketfd, &out_s2s, sizeof(out_s2s), 0, (struct sockaddr *)&servList.servers[i].address, sizeof(struct sockaddr_in));

								if (bytes < 0) {
									printf("error sending bytes\n");
								}
							}

						}
						
					}
					}
					else {
						printf("%s:%d bad packet, got %ld, when expecting 36 bytes\n", serverHostString, serverSocketString, inbytes);
						/*char errorBuff[SAY_MAX];
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
						*/
					
					}


				}

				else if (type == S2S_SAY) {
					if (inbytes == 140) {
						// extract info
						struct s2s_say *say_s2s = (s2s_say *)baseRequest;
						strncpy(inIdentifier, say_s2s->unique, IDENTIFY_MAX - 1);
						inIdentifier[IDENTIFY_MAX - 1] = '\0';
						
						strncpy(inUsername, say_s2s->req_username, USERNAME_MAX - 1);
						inUsername[USERNAME_MAX - 1] = '\0';

						strncpy(inChannel, say_s2s->req_channel, CHANNEL_MAX - 1);
						inChannel[CHANNEL_MAX - 1] = '\0';

						strncpy(inMessage, say_s2s->req_text, SAY_MAX - 1);
						inMessage[SAY_MAX - 1] = '\0';

						int found = 0;
						// check for identifier in list
						for (int i = 0; i < identifiers.size; i++) {
						
							// if exists drop packet and send leave to prevent loop
							if (strcmp(identifiers.recent_ids[i], inIdentifier) == 0) {
								found = 1;
								printf("%s:%d %s:%d Loop detected, send S2S Leave %s\n", serverHostString, serverSocketString, clientHostString, clientSocketString, inChannel);

								removeChannel(&myServer, inChannel);

								// create s2s leave message
								struct s2s_leave leave_s2s;
								leave_s2s.req_type = S2S_LEAVE;
								strncpy(leave_s2s.req_channel, inChannel, CHANNEL_MAX - 1);
								leave_s2s.req_channel[CHANNEL_MAX - 1] = '\0';

								// log
								//printf("%s:%d %s:%d send S2S Leave %s\n", serverHostString, serverSocketString, clientHostString, clientSocketString, inChannel);

								// send s2s leave message
								bytes = sendto(socketfd, &leave_s2s, sizeof(leave_s2s), 0, (struct sockaddr*)&clientAddr, sizeof(struct sockaddr_in));
								if (bytes < 0) {
									printf("error sending bytes\n");
								}

								break;
							}
						}

						if (!found) {

						// add identifer to recent list
						addIdentifier(&identifiers, inIdentifier);

						// message to all clients of this server
						struct text_say say_text;
						say_text.txt_type = TXT_SAY;
						strncpy(say_text.txt_channel, inChannel, CHANNEL_MAX - 1);
						say_text.txt_channel[CHANNEL_MAX - 1] = '\0';
						strncpy(say_text.txt_username, inUsername, USERNAME_MAX - 1);
						say_text.txt_username[USERNAME_MAX - 1] = '\0';
						strncpy(say_text.txt_text, inMessage, SAY_MAX - 1);
						say_text.txt_text[SAY_MAX - 1] = '\0';

						// print to server
						//printf("server: %s sends say message in %s\n", inUsername, inChannel);
						printf("%s:%d %s:%d recv S2S say %s %s \"%s\"\n", serverHostString, serverSocketString, clientHostString, clientSocketString, inUsername, inChannel, inMessage);

						int client = 0;
						int first = 1;
						// find channel
						for (int i = 0; i < chanList->size; i++) {
							if (strcmp(inChannel, chanList->channels[i].name) == 0) {
								// for each user on channel
								client = 1;
								for (int j = 0; j < chanList->channels[i].size; j++) {
									if (first) {
										printf("%s:%d %s:%d send say message \"%s\" in %s from %s\n", serverHostString, serverSocketString, inet_ntoa(chanList->channels[i].users[j].address.sin_addr), ntohs(chanList->channels[i].users[j].address.sin_port), inMessage, inChannel, inUsername);
										first = 0;
									}
									//if (compareSocket(chanList->channels[i].users[j].address, clientAddr)) {
										//}
								// send message to user
									bytes = sendto(socketfd, &say_text, sizeof(say_text), 0, (struct sockaddr *)&chanList->channels[i].users[j].address, sizeof(struct sockaddr_in));

									if (bytes < 0) {
										printf("error sending bytes\n");
									}
								}
							break;
							}
						}
						int servs = 0;
						// forward message to all subsribed neighbors
						for (int i = 0; i < servList.server_size; i++) {
							//printf("%d\n", servList.servers[i].port);
							if (!compareSocket(clientAddr, servList.servers[i].address)) {
								for (int j = 0; j < servList.servers[i].channel_size; j++) {
									//if (strcmp(servList.servers[i].channels[j], inChannel) == 0) {
									if (strcmp(servList.servers[i].channels[j].name, inChannel) == 0) {
										servs = 1;
										printf("%s:%d %s:%d send S2S say %s %s \"%s\"\n", serverHostString, serverSocketString, servList.servers[i].host, servList.servers[i].port, inUsername, inChannel, inMessage);
										bytes = sendto(socketfd, say_s2s, sizeof(*say_s2s), 0, (struct sockaddr *)&servList.servers[i].address, sizeof(struct sockaddr_in));
									
										if (bytes < 0) {
											printf("error sending bytes\n");
										}
										break;
									}
								}
							}
						}
						// no clients or servers to forward to 
						if (!servs && !client) {
							// remove channel from your subs
							removeChannel(&myServer, inChannel);

							// create s2s leave message
							struct s2s_leave leave_s2s;
							leave_s2s.req_type = S2S_LEAVE;
							strncpy(leave_s2s.req_channel, inChannel, CHANNEL_MAX - 1);
							leave_s2s.req_channel[CHANNEL_MAX - 1] = '\0';

							// log
							printf("%s:%d %s:%d send S2S Leave %s\n", serverHostString, serverSocketString, clientHostString, clientSocketString, inChannel);

							// send s2s leave message
							bytes = sendto(socketfd, &leave_s2s, sizeof(leave_s2s), 0, (struct sockaddr*)&clientAddr, sizeof(struct sockaddr_in));
							if (bytes < 0) {
								printf("error sending bytes\n");
							}

						
						}

						}
					}
					else {
						printf("%s:%d bad packet, got %ld, when expecting 140 bytes", serverHostString, serverSocketString, inbytes);/*
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
						*/
					
					}

				}

				else if (type == S2S_LEAVE) {
					if (inbytes == 36) {
						// extract info
						struct s2s_leave *l_s2s = (s2s_leave *)baseRequest;
						strncpy(inChannel, l_s2s->req_channel, CHANNEL_MAX - 1);
						inChannel[CHANNEL_MAX - 1] = '\0';

						printf("%s:%d %s:%d recv 2S2 Leave %s\n", serverHostString, serverSocketString, clientHostString, clientSocketString, inChannel);
						
						// find server
						for (int i = 0; i < servList.server_size; i++) {
							if (compareSocket(servList.servers[i].address, clientAddr)) {
								
								// remove channel
								removeChannel(&servList.servers[i], inChannel);
								break;
							}	
						}
					}

					else {
						printf("%s:%d bad packet, got %ld, when expecting 36 bytes", serverHostString, serverSocketString, inbytes);
						/*char errorBuff[SAY_MAX];
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
						*/
			
					}
				}

				// unknown request
				else {
						printf("%s:%d recieved Unknown packet\n", serverHostString, serverSocketString);
						/*

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
						*/

				}
		}
		pthread_mutex_unlock(&lock);

		}
		//printf("here\n");
	//	fflush(stdout);
		//handleTimers(&myServer, &servList);


	}

}
