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
	sem_init(&semaphore,0,1);

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

            // create pthread
            int status = pthread_create(&tid, NULL, telnetDownload, (void*)info);
            if(status){
                cerr<<"Error creating threads." <<endl;
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
//parses the client command into a relative uri from proxy to server
// string parseCommand(string command){
    // //cout << "parse command" << endl;
    // string method = command.substr(0, command.find(" "));
    // command.erase(0, command.find("w"));
    // string host;
    // size_t found = command.find(":");
    // //cout << command << endl;
    // if (found != string::npos){
        // host = command.substr(0, command.find(":"));
    // } else {
        // host = command.substr(0, command.find("/"));
    // }
    // //cout << command << endl;
    // //cout << host << endl;
    // command.erase(0, command.find("/"));
    // string file = command.substr(0, command.find("H"));
    // string proto = command.erase(0, command.find(" ")+1);

    // string cmd = method + " " + file + proto + 
    // "Host: " + host + "\r\n" + "Connection: close\r\n\r\n\r\n";
    // return cmd;
// }

//writing info from remote to socket
void writeToSocket(int sockfd, const char* buffer, int bufferLen){
    int bytesSent = 0;
    int sentThisTime;
    while (bytesSent < bufferLen){
        if ((sentThisTime = send(sockfd, (void *) (buffer + bytesSent), bufferLen - bytesSent, 0)) < 0){
            perror ("Sending Error: ");
            exit(-1);
        }
        bytesSent = bytesSent + sentThisTime;
    }
}
//this method connects server info back to the client
void bridgeServerClient(int clientSock, int remoteSock){
    const int bufferLen = 2048;
    int bytesRecv;
    char buf[bufferLen];
    while ((bytesRecv = recv(remoteSock, buf, bufferLen, 0)) > 0){
        writeToSocket(clientSock, buf, bytesRecv); // write to client
        memset(buf, 0, sizeof(buf)); // update buffer size
    }
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
    cout << "testing for calling telnet once" << endl;
    //control loop
    while (!isValid && recv(info->sock, client_message, 1024, 0) != 0){ 
        command += (client_message);
        isValid = checkIfValid(command);
		cout<< "looping" << endl;
    }
    //check for valid request   
    string cmd = command;//parseCommand(command);
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
        //firstLine.erase(0, command.find("/") + 2);
        
        if (command.find("http://") == string::npos){
            // no http
            host = firstLine.substr(firstLine.find(" ") + 1, firstLine.length());
        } else {
            host = firstLine.erase(0, command.find("/") + 2);
        }
        
        cout << host << endl;

        
        // get port code
        if (host.find(':') != string::npos){
            // if there is a port nubmer
            port = host.substr(host.find(':') + 1, host.find(" ")-1);
            cout << "port in if branch " << port << endl;
            host.erase(host.find(':'), host.length());
            cout << "host in port branch " << host << endl;
        } else {
            host = host.substr(0, host.find('/'));
            cout << "host in port branch with no port " << host << endl;
        }

        port = port.substr(0, port.length() - 10);
        bool has_only_digits = (port.find_first_not_of( "0123456789" ) == string::npos);
        if (!has_only_digits){
            port = "80";
            host = host.substr(0, host.find('/'));
        }

        /*
        if (firstLine.find(':') != string::npos){
            host = firstLine.substr(0, firstLine.find(":"));
            firstLine.erase(0, firstLine.find(":") + 1);
            port = firstLine.substr(0, firstLine.find("/"));
        } else {
            host = firstLine.substr(0, firstLine.find("/"));
        }
        */

        cout << "host is " << host << endl;
        cout << "port is " << port << endl;
        cout << "command is " << endl << cmd << endl;
        // connect to remote

		int remotesock = connectToRemote(host, port);

		char request[1024];             //to not reuse buffer from earlier
		strcpy(request, cmd.c_str());

		// make http request
		//sem_wait(&semaphore);
		writeToSocket(remotesock, request, strlen(request));

		// client is info->sock
		// remote is remotesock
        bridgeServerClient(info->sock, remotesock);
		
		
        // // formatting client side
		// string newLine = "\r\n";
        // strcpy(request, newLine.c_str());
        // write(info->sock, request, strlen(request));
        
        //release resources
		shutdown(remotesock, SHUT_WR);
        shutdown(info->sock, SHUT_WR);
        free(info);
        pthread_exit(NULL);
        
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
		exit(1);
    }

    for (remoteresult = remoteInfo; remoteresult != NULL; remoteresult = remoteresult->ai_next) {
		if ((remotesockfd = socket(remoteresult->ai_family, remoteresult->ai_socktype, 
			remoteresult->ai_protocol)) == -1) {
            perror("Socket error: ");
            cout << "error on host: " << hostname << " and port " << port << endl;
			continue;
		}

		if (connect(remotesockfd, remoteresult->ai_addr, remoteresult->ai_addrlen) == -1) {
            perror("Connection error: ");
            cout << "error on host: " << hostname << " and port " << port << endl;
			close(remotesockfd);
			continue;
		}
		break;
    }
    //connecting to remote
    if (remoteresult == NULL){
		fprintf(stderr, "Proxy failed to connect to remote");
        return 2;
	}
  
    // No longer need list of addresses
    freeaddrinfo(remoteInfo);
  
    return remotesockfd;
}

