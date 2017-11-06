
#ifndef PROXY_H_
#define PROXY_H_

void writeToSocket(int sockfd, const char* buffer, int bufferLen);  //writing to socket
void* bridgeServerClient(void* i);//(int clientSock, int serverSock);             //server output to client
int connectToRemote(std::string, std::string port);                 //connecting to remote server
std::string parseCommand(std::string command);                           //parse commands
void* telnetDownload(void* i);                                      // for pthread
bool charCompare(char a, char b);
bool stringCompare(std::string a, std::string b);


#endif /*PROXY_H_*/


