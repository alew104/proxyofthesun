// AUTHORS: Alex Lew, Sunny Yeung, Daniel Pyon
// FILENAME: proxy.cpp
// CLASS: CPSC 5510, Fall 2017, Seattle University.
// INSTRUCTOR: Yingwu Zhu
// ASSIGNMENT NO: HW2
// DATE: 10/29/2017
// REFERENCES:

/*
 * This is a server program that runs on linux based machines.
 * It uses socket programming to forward client requests to a remote server and then 
 * sends the response back to client.
 * It can handle several clients at once.
 */


#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <iostream>
#include <pthread.h>
#include "proxy.h"
#include <semaphore.h>

using namespace std;

const int BACKLOG = 10;         //max threads
const string GET = "get";       //string comparison later

struct sockInfo {
    int sock;
    pthread_t tid;
};

struct twoSockInfo{
    int remotesock;
    int clientsock;
    pthread_t tid;
};

sem_t semaphore;

int main(int argc, char* argv[]){
    if (argc != 2){
        cerr << "Please input the portnumber" << endl;
        exit(-1);
    }

    string portnumber = argv[1];
    int sockfd, new_fd;  // listen on sock_fd, new connection on new_fd
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address information
    socklen_t sin_size;;
    int rv;
	sem_init(&semaphore, 0, 30);

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; // use current computer IP


    if ((rv = getaddrinfo(NULL, portnumber.c_str(), &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }


    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype,
                p->ai_protocol)) == -1) {
            perror("Socket error: ");
            continue;
        }

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("Bind error: ");
            continue;
        }

        break;
    }

    if (p == NULL)  {
        fprintf(stderr, "Failed to bind sockets \n");
        exit(1);
    }

    if (listen(sockfd, BACKLOG) == -1) {
        perror("Listen error: ");
        exit(1);
    }

    freeaddrinfo(servinfo); // No longer need list of addresses

    cout << "Listening for connections..." << endl;

        while(true) {  // continuously run
            sin_size = sizeof their_addr;
            sem_wait(&semaphore);
            new_fd = accept(sockfd, (struct sockaddr *)&their_addr, &sin_size);

            if (new_fd == -1) {
                perror("Accept error: ");
                continue;
            }

            // pthread instantiation here

            pthread_t tid;
            
            // struct to pass sock info
            sockInfo *info = new sockInfo;
            info->sock = new_fd;
            info->tid = tid;

            // create pthread
            int status = pthread_create(&tid, NULL, telnetDownload, (void*)info);
            if(status){
                cerr<<"Error creating threads." << endl;
                close(new_fd);
                exit(-1);
            }
            //cerr << "****Client thread started.****" << endl;


        }
    return 0;
}

//compares characters
bool charCompare(char a, char b){
    return tolower(a) == tolower(b);
}
//compares and returns the get request
bool stringCompare(string a, string b) {
    if (a.length()==b.length()) {
        return equal(b.begin(), b.end(),
                     a.begin(), charCompare);
    }
    else {
        return false;
    }
}

//writing info from remote to socket
void writeToSocket(int sockfd, const char* buffer, int bufferLen){
    int bytesSent = 0;
    int sentThisTime;
    while (bytesSent < bufferLen){
        if ((sentThisTime = send(sockfd, (void *) (buffer + bytesSent), bufferLen - bytesSent, 0)) < 0){
            perror ("Sending Error: ");
            //exit(-1);
            sem_post(&semaphore);
            pthread_exit(NULL);
        }
        bytesSent = bytesSent + sentThisTime;
    }
}
//this method connects server info back to the client
void* bridgeServerClient(void* i){
    twoSockInfo *info;
    info = (twoSockInfo*)i;
    const int bufferLen = 8192;
    int bytesRecv;
    char buf[bufferLen];
    while ((bytesRecv = recv(info->remotesock, buf, bufferLen, 0)) > 0){
        writeToSocket(info->clientsock, buf, bytesRecv); // write to client
        memset(buf, 0, sizeof(buf)); // update buffer size
    }
    close(info->remotesock);
    close(info->clientsock);
    pthread_detach(info->tid);
    pthread_exit(NULL);
}

//for loop control for receiving 
bool checkIfValid(string command){
    size_t found = command.find("\n");
    if (found == string::npos){
        return false;
    }
    return true;
}

//for pthread and getting client requests
void* telnetDownload(void* i){
    sockInfo *info;
    info = (sockInfo*)i;
    char client_message[1024];  //the buffer to store the client message
    string command;             //the command to be parsed

    bool isValid = false; // used for string validation and loop breaking
    //control loop
    while (!isValid && recv(info->sock, client_message, 1024, 0) != 0){ 
        command += (client_message);
        isValid = checkIfValid(command);
    }
    //check for valid request   
    string cmd = command;//parseCommand(command);
    if (cmd.empty()){
        close(info->sock);
        sem_post(&semaphore);
        pthread_exit(NULL);
    }
    string firstLine = command.substr(0, command.find("\n"));
    //string validate = cmd.substr(0, cmd.find('/')-1); 
    //if not a get request
    bool turnOff = true;
	if (/*!stringCompare(validate, GET)*/ turnOff == false) {
		char request[1024];
        string errormsg = "500 'Internal error'\r\n";
        strcpy(request, errormsg.c_str());
        //write(info->sock, request, strlen(request));
		//close(info->sock);
        //pthread_exit(NULL);
    } else {
        //if no port
        string port = "80";
        string host;
        // cause need host for connection
        
        if (command.find("http://") == string::npos){
            // no http
            host = firstLine.substr(firstLine.find(" ") + 1, firstLine.length());
        } else {
            host = firstLine.erase(0, command.find("/") + 2);
        }
        
        
        // get port code
        if (host.find(':') != string::npos){
            // if there is a port nubmer
            port = host.substr(host.find(':') + 1, host.find(" ")-1);
            //cout << "port in if branch " << port << endl;
            host.erase(host.find(':'), host.length());
            //cout << "host in port branch " << host << endl;
        } else {
            host = host.substr(0, host.find('/'));
            //cout << "host in port branch with no port " << host << endl;
        }

        port = port.substr(0, port.length() - 10);
        bool has_only_digits = (port.find_first_not_of( "0123456789" ) == string::npos);
        if (!has_only_digits){
            port = "80";
            host = host.substr(0, host.find('/'));
        }

        /*
        cout << "host is " << host << endl;
        cout << "port is " << port << endl;
        cout << "command is " << endl << cmd << endl;
        */

        // connect to remote

		int remotesock = connectToRemote(host, "80");

		char request[1024];             //to not reuse buffer from earlier
		strcpy(request, cmd.c_str());

        // make http request
        int bytesSent = 0;
        int sentThisTime;
        while (bytesSent < strlen(request)){
            if ((sentThisTime = send(remotesock, (void *) (request + bytesSent), strlen(request) - bytesSent, 0)) < 0){
                perror ("Sending Error: ");
                //exit(-1);
                sem_post(&semaphore);
                pthread_exit(NULL);
            }
            bytesSent = bytesSent + sentThisTime;
        }


        
		//writeToSocket(remotesock, request, strlen(request));

		// client is info->sock
        // remote is remotesock
        pthread_t tid;
        twoSockInfo *bridgeInfo = new twoSockInfo;
        bridgeInfo->remotesock = remotesock;
        bridgeInfo->clientsock = info->sock;
        bridgeInfo->tid = tid;
        int status = pthread_create(&tid, NULL, bridgeServerClient, (void*)bridgeInfo);
        if(status){
            cerr<<"Error creating threads." << endl;
            close(remotesock);
            close(info->sock);
            pthread_exit(NULL);
            //exit(-1);
        }
        //bridgeServerClient(info->sock, remotesock);
		
		
        // // formatting client side
		// string newLine = "\r\n";
        // strcpy(request, newLine.c_str());
        // write(info->sock, request, strlen(request));
        
        //release resources
        //close(remotesock);
        //close(info->sock);
        //free(info);
        sem_post(&semaphore);
        pthread_detach(info->tid);
		}
	return NULL;
}


// connect to remote server
int connectToRemote(string hostname, string port){
    struct addrinfo remotehints;
    struct addrinfo *remoteInfo;
    struct addrinfo *remoteresult;
    int remotesockfd;
    int remoterv;
  
    // Get address information for stream socket on input port 
    memset(&remotehints, 0, sizeof(remotehints));
    remotehints.ai_family = AF_UNSPEC;
    remotehints.ai_socktype = SOCK_STREAM;

    if ((remoterv = getaddrinfo(hostname.c_str(), port.c_str(), &remotehints, &remoteInfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(remoterv));
        cout << "error on host: " << hostname << " and port " << port << endl;
        //exit(1);
        sem_post(&semaphore);
        pthread_exit(NULL);
    }

    for (remoteresult = remoteInfo; remoteresult != NULL; remoteresult = remoteresult->ai_next) {
		if ((remotesockfd = socket(remoteresult->ai_family, remoteresult->ai_socktype, 
			remoteresult->ai_protocol)) == -1) {
            perror("Socket error: ");
            sem_post(&semaphore);
            cout << "error on host: " << hostname << " and port " << port << endl;
			continue;
		}

		if (connect(remotesockfd, remoteresult->ai_addr, remoteresult->ai_addrlen) == -1) {
            perror("Connection error: ");
            cout << "error on host: " << hostname << " and port " << port << endl;
            close(remotesockfd);
            sem_post(&semaphore);
            pthread_exit(NULL);
			continue;
		}
		break;
    }
    //connecting to remote
    if (remoteresult == NULL){
        fprintf(stderr, "Proxy failed to connect to remote");
        sem_post(&semaphore);
        return 2;
	}
    // No longer need list of addresses
    freeaddrinfo(remoteInfo);
  
    return remotesockfd;
}


