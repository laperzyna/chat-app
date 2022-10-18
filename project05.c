#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/poll.h>

#define BUF_SIZE 500
#define PRESENCE_PORT "8221"
#define LAB_NETWORK "10.10.13.255"

/*Project 5

-list of users as they broadcast to the UDP
-list of TCP users that we have connected with so far


-start a UDP server on 8221
-broadcast ourself to the UDP server (this happens every 10 seconds)
-For every user that broadcasts their presence we should add them to a list of users
 if they aren't already on the list
-Listen for messages through TCP and also the command line
-If you get a command line message, check to see if we have a tcp connection for that user
    if we do just message them through that address info
    if we do not have a connection with them try to initialize a connection

*/

typedef struct user {
	char *username;
    char *port;
    int isConnected;
    int sockfd;
	struct sockaddr_in data;
}User;

//TODO figure out how to remove a client after they go offline
User users[64];
struct pollfd userFDs[64];
int numUsers = 0;

//This sends a message to the UDP server
//We are using this specifically for the presence (online/offline) messasges
void sendMsg (int sfd, struct sockaddr_in *addr, char *status) {
    char buf[BUF_SIZE];
    strncpy(buf, status, BUF_SIZE);
    int len = strlen(buf) + 1;
    if (sendto(sfd, buf, len, 0, (struct sockaddr*) addr, sizeof(struct sockaddr_in)) != len) {
        perror("sendto");
    }
}

//This function gets called when another user sends a broadcast message to the UDP server
//This parses the message and adds the user to our user database if they have not been added already
//The user databse saves the username and port 
void parseBroadcast(int sfd) {
    socklen_t peer_addr_len;
    struct sockaddr_storage peer_addr;
    ssize_t nread;
    char buf[BUF_SIZE];
    peer_addr_len = sizeof(peer_addr);
    nread = recvfrom(sfd, buf, BUF_SIZE, 0,
    (struct sockaddr *) &peer_addr, &peer_addr_len);
    if (nread == -1)
        return;  // Ignore failed request 
    char host[NI_MAXHOST], service[NI_MAXSERV];

    int s = getnameinfo((struct sockaddr *) &peer_addr,
            peer_addr_len, host, NI_MAXHOST,
            service, NI_MAXSERV, NI_NUMERICSERV);
    if (s == 0) {
        //buf should be "online: bob 8000"
             //                  x
        //buf is the message which starts with online: , so we 
        //need to extract the username after that
        //find the first space
        char * firstLetter = &buf[0];
        //while we're not at the space yet keep increasing
        while (*firstLetter != ' ') {
            firstLetter++;
        }
        //at this point we found the space but we really want to 
        //start with the letter after that so increase one more 
        //time
        firstLetter++;
        //now count how many chars are in username
        char * curLetter = firstLetter;
        int numLetters = 0;
        while (*curLetter != ' ') {
            curLetter++;
            numLetters++;
        }
        
        char username[100];
        strncpy(username,firstLetter, numLetters);
        username[numLetters] = '\0';
        //check to see if this person is already in the database
        for(int i = 0; i < numUsers; i++) {
            if (strcmp(users[i].username, username) == 0) {
                //this person already exists in the databsae so 
                //do nothing
                return;
            }
        }
        //at this point co should hold how many letters are in the username so lets 
        //strncpy that many        
        //now we should be at the start of the username
        users[numUsers].username = malloc(sizeof(char) * (numLetters+1)); 
        //+1 because we need to add the null terminating char
        //strncpy copies N chars
        strncpy(users[numUsers].username,firstLetter, numLetters);
        //put in the null terminating
        users[numUsers].username[numLetters] = '\0';
        //now parse the incoming port
        //at this point curLetter should be on the space in between the username and port
        // so increase
        curLetter++;
        //allocate space for the port
        users[numUsers].port = malloc(sizeof(char) * (strlen(curLetter)+1) );
        //since we have a null terminating at the end of the port we can just use strcpy
        strcpy(users[numUsers].port,curLetter);
        users[numUsers].isConnected = 0;
        printf("%s\n",buf);
        numUsers++;
    } else {
        fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));
    }
}

//This function parses an incoming message through the TCP
//In our case we are sending the username first before the message so we are parsing
//username: message
void parseTCPMessage(int * fd) {

    int len;
    struct sockaddr_in cli;
    *fd = accept(*fd, (struct sockaddr *)&cli, &len);
    if (*fd < 0) {
        printf("server accept failed...\n");
        exit(0);
    }
    socklen_t peer_addr_len;
    struct sockaddr_storage peer_addr;
    ssize_t nread;
    char buf[BUF_SIZE];
    bzero(buf, sizeof(buf));
    peer_addr_len = sizeof(peer_addr);
    nread = recvfrom(*fd, buf, BUF_SIZE, 0,
    (struct sockaddr *) &peer_addr, &peer_addr_len);
    if (nread == -1)
        return;  // Ignore failed request 
    char host[NI_MAXHOST], service[NI_MAXSERV];
    //if we get the message pull out the username and say we're 
    //connected to this person
    //so that for future messages we dont try to connect again
    int charCo = 0;
    char * colon = &buf[0];
    while (*colon != ':') {
        colon++;
        charCo++;
    }
    //at this point we know how long the username is INCLUDING the @ symbol
    //the number of bytes to copy is one less than charCo and also we 
    //should not be starting
    //at the beginning of the line because then we're copying the @ symbol
    char username[100];
    //copy only the correct amount of chars
    strncpy(username, &buf[0], charCo);
    //since @bob is not the end of the string we don't have a null 
	//terminating to copy so we have to put it in ourselves
    username[charCo] = '\0';
    //at this point color should be on the colon, so increase and then 
    //use the rest of the message
    colon++;
    printf("%s says: %s\n", username, colon);
}
//This function sends a message to a user that is saved in the user database
//This takes in a user to send to, text to send, and also the name of THIS programs user
//so we can append it to the beginning of the message
//We send in this format
//myusername: message
void sendTCPMsg(User user, char * text, char * myName) {
    //put the username in front with colon
    char message[128];
    message[0] = '\0';
    strcat(message, myName);
    strcat(message, ":");
    strcat(message, text);
    write(user.sockfd, message, strlen(message) +1);
}
//This function tries to connect to another users TCP Server
//it is based on that users port which is saved in the user database
//We save the socket file descriptor in the user database so we can use it later
//to send a TCP message
int connectToTCPServer(User * user) {

    int sockfd;
    struct sockaddr_in servaddr;
   
    // socket create and verification
    sockfd = socket(PF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    bzero(&servaddr, sizeof(servaddr));   
   	int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0) {
        perror("setsockopt reausaddr");
        close(sockfd);
        exit(EXIT_FAILURE);
    }
    // assign IP, PORT
    servaddr.sin_family = PF_INET;
    //use the ip address of my machine rn
    inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
    servaddr.sin_port = htons( atoi((*user).port));
    // connect the client socket to server socket
    if (connect(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr)) != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    (*user).sockfd = sockfd;   //save socket file descriptor
}

//This function takes in information from the commandline in the format 
//@username: message
//It parses that out to figure out which user we should send the message to
//If we have not connected this user yet, it will make the connection,
//and then it sends a TCP message to the user
void parseCommandLine(char * line, User * users, char * myName) {
    //parse the message to get the send username and the message
    //@bob: hello whats up
    //go up to the first space and count how many chars
    int charCo = 0;
    char * colon = &line[0];
    while (*colon != ':') {
        colon++;
        charCo++;
    }
    //at this point we know how long the username is INCLUDING the @ symbol
    //the number of bytes to copy is one less than charCo and also we should not be starting
    //at the beginning of the line because then we're copying the @ symbol
    char searchName[100];
    //copy only the correct amount of chars
    strncpy(searchName, &line[1], charCo-1);
    //since @bob is not the end of the string we don't have a null terminating to copy so we have to put it in ourselves
    searchName[charCo-1] = '\0';
    //now get the message, the message starts two after the colon so increase
    colon += 2;
    char message[128];
    //strlen doesnt count the nullterminating but we do want to copy it so add 1
    strncpy(message,colon, strlen(colon)+1);            
    //look up the name in the list of users and use that socket address info
    for(int i = 0; i < numUsers; i++) {
        if (strcmp(users[i].username, searchName) == 0) {
            //if we haven't connected to user yet lets connect
            if (users[i].isConnected == 0){
                connectToTCPServer(&users[i]); //try to connect
                users[i].isConnected = 1;
            }
            sendTCPMsg(users[i], message, myName);
        }
    }
}

//this setups the UDP server that listens for the presence messages on 10.10.13.255 port 8221
int setupUDP( struct addrinfo *result ) {
    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;    // Allow IPv4 or IPv6
    hints.ai_socktype = SOCK_DGRAM; // Datagram socket
    hints.ai_flags = AI_PASSIVE;    // Any IP address (DHCP)
    hints.ai_protocol = 0;          // Any protocol
    hints.ai_canonname = NULL;
    hints.ai_addr = NULL;
    hints.ai_next = NULL;
    int status = getaddrinfo(NULL, PRESENCE_PORT, &hints, &result);
    //getaddrinfo sends back a number that represents if it was successfull. 0 is sucess
    if (status != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
        exit(EXIT_FAILURE);
    }
    /* getaddrinfo() returns a list of address structures.
      Try each address until successfully bind().
      If socket() or bind() fails, close the socket
      and try the next address. 
    */
    struct addrinfo *rp;
    int presenceFD = -1;
    for (rp = result; rp != NULL; rp = rp->ai_next) {
        presenceFD = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (presenceFD == -1) {
            continue;
        }
       	int enable = 1;
        if (setsockopt(presenceFD, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0) {
    		perror("setsockopt reausaddr");
            close(presenceFD);
            exit(EXIT_FAILURE);
        }
        if (setsockopt(presenceFD, SOL_SOCKET, SO_BROADCAST, &enable, sizeof(int)) != 0) {
            perror("setsockopt broadcast");
            close(presenceFD);
            exit(EXIT_FAILURE);
        }
        if (bind(presenceFD, rp->ai_addr, rp->ai_addrlen) == 0){
            break;
        } else {
            perror("bind");
            exit(EXIT_FAILURE);
        }
        close(presenceFD);
    }
    freeaddrinfo(result);
    if (rp == NULL) {  // No address succeeded
        fprintf(stderr, "Could not bind\n");
        exit(EXIT_FAILURE);
    }
    return presenceFD;
}

//this sets up the TCP Server to listen for messages from other users
//This gets connected to the port that is passed into the program through the command line
// ./programName bob 8080  --> this would host the TCP server on port 8080
int setupTCPConnection(char * port) {
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;
    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    bzero(&servaddr, sizeof(servaddr));
    int enable = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) != 0) {
        perror("setsockopt reausaddr");
        close(sockfd);
        exit(EXIT_FAILURE);
    }   
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
   // servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
   	inet_pton(AF_INET, "127.0.0.1", &servaddr.sin_addr);
    servaddr.sin_port = htons(atoi(port));   
    // Binding newly created socket to given IP and verification
    if ((bind(sockfd, (struct sockaddr*) &servaddr, sizeof(servaddr))) != 0) {
        printf("socket bind failed...\n");
        exit(0);
    }
    // Now server is ready to listen and verification
    if ((listen(sockfd, 5)) != 0) {
        printf("Listen failed...\n");
        exit(0);
    }
    return sockfd;
}

int main(int argc, char *argv[]) {
    if (argc < 3){
        printf("Usage  ./project5 USERNAME PORT\n");
        exit(1);
    }    
    //Pointer is a piece of paper with an address on it
    //When you pass a pointer into a function you are copying that original paper
    //and using it in the function.
    //Original Paper -> 123 john lane
    //pass that to a function
    //Copy of Original Paper ->123 john lane
    //lets say the function changes that address
    //Copy of Original Paper -> 150 bill lane
    //well that did not change the ORIGINAL piece of paper
    struct addrinfo *result, *rp;
    int presenceFD, messagesFD, addressInfo;
    struct sockaddr_storage peer_addr;
    socklen_t peer_addr_len;
    ssize_t nread;
    char buf[BUF_SIZE];
     //setup UDP presence server
    presenceFD = setupUDP(result);
    if (presenceFD == -1) {
        perror("Could not setup UDP server");
        exit(EXIT_FAILURE);
    }
   //setup listener for tcp on port passed in through command line
    messagesFD = setupTCPConnection(argv[2]);
     //poll 3 different sockets (socket file descriptors)
    // for UDP broadcasts, STDIN messages, and TCP messages
    int numFDs = 3;
    struct pollfd pfds[numFDs];
    //all of these get set to listen for POLLIN - if we made it far enough for user's leaving the server
    //we might have to listen for an additional EVENT TYPE (maybe POLLOUT or something)
    pfds[0].fd = presenceFD; //UDP server listening for broadcasts
    pfds[0].events = POLLIN; //POLLIN means we got data in
    pfds[1].fd = STDIN_FILENO; //this is to listen for messages we should be sending
    //they come from the terminal in "@username: messsage"   format
    //@bob2: hey whats up
    pfds[1].events = POLLIN;
    pfds[2].fd = messagesFD;
    pfds[2].events = POLLIN;
    //bob  8080
    //bill 8081
    //for bob to send a message to bill, bob opens TCP connection on 8081
    //setup the OUTPUT address to send the UDP presence message on
    struct sockaddr_in baddr;    
    memset(&baddr, 0, sizeof(baddr));
    baddr.sin_family = AF_INET;
    inet_pton(AF_INET, LAB_NETWORK, &baddr.sin_addr);
    baddr.sin_port = htons( atoi(PRESENCE_PORT));
    //populate the presence message  online: bob 8080
    char presenceMsg[100];
    strcpy(presenceMsg,"online: ");
    strcat(presenceMsg, argv[1]);
    strcat(presenceMsg, " ");
    strcat(presenceMsg, argv[2]);
    char line[100];
    int timeoutCo = 0;
    //Normally taking in information from the user will BLOCK other code. scanf() or 
    //gets() will stop and wait for the user to enter
    //a value. We can't do that here because if we stop and wait for a user input value
    // we are not checking to see if we got UDP broadcast
    //or TCP message. To solve that, we should add the STDIN as a socket in our pollFd 
    //array so that we can monitor all three inputs in a 
    //NON-BLOCKING fashion.
    //runs forever
    while(1) {
     	//poll all 3 sockets that we are listening on (UDP, STDIN, TCP)
        int res = poll(pfds, numFDs, 100); //100ms timeout
        //the third parameter is how long the poll() function should wait if it tries 
        //to read and there is no data there to read      
        //if there is no data to read when we call poll() it sends back a 0 (poll 
        //returns the number of socket fd's that had data)
        //if there was no data to read in count towards the timeout
        if (res == 0) {
            timeoutCo++;
        }
        //if you get no data in 100 times then you should broadcast your presence
     	//this works out to be 10 seconds because every poll waits 100ms and we do that 100 
     	//times before we broadcast
       	//our presence again so 100ms * 100 --> 10000ms..... which is 10 seconds
        if (timeoutCo == 100) {
        	//send out presence message
         	sendMsg(presenceFD, &baddr, presenceMsg);
         	//since we just broadcasted reset the timeoutCo so we will wait another 
         	//10 seconds before broadcasting
         	timeoutCo = 0;
        }
        //go through all socket FD's (UDP,STDIN,TCP) and check if they received a 
        //POLLIN event
        for(int i = 0 ; i < numFDs; i++) {
        	//if there was a POLLIN even received
            if (pfds[i].revents & POLLIN) {
            	//we have to handle the receive event different depending on if it is UDP, STDIN or TCP
            	//if the user typed something in the command line
                if (pfds[i].fd == STDIN_FILENO){
                    fgets(line, 100, stdin);
                    //parse and send the message they typed
                    parseCommandLine(line, users, argv[1]);  
                    //if we received a POLLIN event for TCP messages                  
                } else if (pfds[i].fd == messagesFD) {
                    //this fires off when we get a connect request 
                    //from another user
                   parseTCPMessage(&pfds[i].fd);                   
                } else {
                	//if you get a POLLIN on the UDP server 
                    parseBroadcast(pfds[i].fd);
                }
            }
        }  
    }
}