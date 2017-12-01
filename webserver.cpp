#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string>
#include <unistd.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <sys/mman.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <atomic>

using namespace std;

#define MAXLINE 4096 /*max text line length*/
//#define SERV_PORT 3000
#define LISTENQ 32

struct ContentType{
  string extension;
  string tag;
};

static bool *keepLooping;

void stopLooping(int) { *keepLooping = false;}


int main(int argc, char ** argv)
{
    signal (SIGINT, stopLooping);
    int SERV_PORT;
    string DOC_ROOT;
    string DIR_INDEX;
    vector<ContentType> contentTypes;
    int KEEP_ALIVE;
    contentTypes.reserve(10);
    ifstream conf;
    conf.open("ws.conf");
    if(!conf.is_open()){
      printf("%s\n","Error opening file ws.conf");
      return -1;
    }
    string line;
    while(getline(conf, line)){
      int delimIndex = line.find(' ');
      string token = line.substr(0,delimIndex);
      if(token.compare("Listen")==0){
        int val = stoi(line.substr(delimIndex+1));
        SERV_PORT = val;
      }else if(token.compare("DocumentRoot")==0){
        string val = line.substr(delimIndex+2, line.substr(delimIndex+2).find('"'));
        DOC_ROOT = val;
      }else if(token.compare("DirectoryIndex")==0){
        string val = line.substr(delimIndex+1);
        DIR_INDEX = val;
      }else if(token.substr(0,1).compare(".")==0){
        string val = line.substr(delimIndex+1);
        struct ContentType ct;
        ct.extension = token;
        ct.tag = val;
        contentTypes.push_back(ct);
      }else if(token.compare("Keep-Alive")==0){
        int val = stoi(line.substr(delimIndex+6));
        KEEP_ALIVE = val;
      }
    }
    conf.close();
    // printf("%d %s %s %d\n", SERV_PORT, DOC_ROOT.c_str(), DIR_INDEX.c_str(), KEEP_ALIVE);
    // for(int i = 0; i<contentTypes.size();i++){
    //   printf("%s %s\n", contentTypes.at(i).extension.c_str(), contentTypes.at(i).tag.c_str());
    // }

    int listenfd, connfd, n;
    pid_t childpid;
    socklen_t clilen;
    char buf[MAXLINE];
    struct sockaddr_in cliaddr, servaddr;
    if ((listenfd = socket (AF_INET, SOCK_STREAM, 0)) <0){
      perror("Problem in creating the socket");
      exit(2);
    }else{
      cout << "Socket created" << endl;
    }

    //prep the socket address
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(SERV_PORT);

    int enable = 1;
    if(setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(int)) < 0){
      perror("couldn't allow socket reuse");
      exit(2);
    }


    //bind the socket
    bind (listenfd, (struct sockaddr *) &servaddr, sizeof(servaddr));

    //listen to the socket by creating a connection queue, then wait for clients
    listen (listenfd, LISTENQ);
    fcntl(listenfd, F_SETFL, fcntl(listenfd,F_GETFL, 0)|O_NONBLOCK);
    //cout << "Listening on: "<<listenfd <<endl;

    //printf("%s\n","Server running...waiting for connections.");
    keepLooping = (bool*)mmap(NULL, sizeof(*keepLooping), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    *keepLooping = true;
    while(*keepLooping) {
      clilen = sizeof(cliaddr);
      connfd = accept (listenfd, (struct sockaddr *) &cliaddr, &clilen);
      if(connfd != -1){
         printf("%s %d\n","Received request... ",connfd);
      }
      if(connfd < 0){
        continue;
      }
      if ((childpid = fork ()) == 0 ) {//if it’s 0, it’s child process
        //printf ("%s\n","Child created for dealing with client requests");

        char buf[MAXLINE];
        if ( (n = recv(connfd, buf, MAXLINE,0)) > 0)  {
          cout << "------- REQUEST---------" << endl;
          cout << buf << endl;
          char * sendBuff;
          int sendBuffSize;
          string reqLine{buf};
          //cout << buf << endl;
        //  string reqLine/**, hostLine, lengthLine**/;
        //  getline(n,reqLine);
        //  getline(n,hostLine);
        //  getline(n,lengthLine);
          string req, target, version;
          int start,end;
          end = reqLine.find(" ");
          req = reqLine.substr(0,end);
          start = end + 1;
          end = reqLine.find(" ",start);
          target = reqLine.substr(start,end-start);
          start = end + 1;
          version = reqLine.substr(start, 8);
          //cout << req << endl;
          //cout << target << endl;
          //cout << version << endl;

          ifstream targetFile;
          string sendit="";
          if(version.compare("HTTP/1.0")!=0 && version.compare("HTTP/1.1")!=0){
            sendit.append("HTTP/1.1 400 Bad request\n");
            string error = "<html><body>400 Bad Request Reason: Invalid HTTP-Version: "+version+"</body></html>\n";
            sendit.append("Content-Type: text/html\n");
            sendit.append("Content-Length: "+to_string(error.size())+"\n");
            sendit.append("\n");
            sendit.append(error);
            sendBuffSize = sendit.size();
            sendBuff = (char*)malloc(sendBuffSize*sizeof(char));
            sendit.copy(sendBuff,sendBuffSize,0);
          }else if(req.compare("GET")!=0 && req.compare("HEAD")!=0 && req.compare("POST")!=0 && req.compare("PUT")!=0 && req.compare("PATCH")!=0
                  && req.compare("DELETE")!=0 && req.compare("CONNECT")!=0 && req.compare("OPTIONS")!=0 && req.compare("TRACE")!=0){
            sendit.append(version + " 400 Bad request\n");
            string error = "<html><body>400 Bad Request Reason: Invalid Method :"+req+"</body></html>\n";
            sendit.append("Content-Type: text/html\n");
            sendit.append("Content-Length: "+to_string(error.size())+"\n");
            sendit.append("\n");
            sendit.append(error);
            sendBuffSize = sendit.size();
            sendBuff = (char*)malloc(sendBuffSize*sizeof(char));
            sendit.copy(sendBuff,sendBuffSize,0);
          }else if(req.compare("GET")!=0){
            sendit.append(version + " 501 Not Implemented\n");
            string error = "<html><body>501 Not Implemented Unsupported Request: "+req+"</body></html>\n";
            sendit.append("Content-Type: text/html\n");
            sendit.append("Content-Length: "+to_string(error.size())+"\n");
            sendit.append("\n");
            sendit.append(error);
            sendBuffSize = sendit.size();
            sendBuff = (char*)malloc(sendBuffSize*sizeof(char));
            sendit.copy(sendBuff,sendBuffSize,0);
          }else{
            if(target.compare("/")==0 || target.compare("/inside/")==0){
              target = DIR_INDEX;
            }
            targetFile.open(DOC_ROOT + target, ios::binary | ios::ate);

            if(!targetFile.is_open()){
              sendit.append(version + " 404 Not Found\n");
              string error = "<html><body>404 Not Found Reason URL does not exist :"+target+"</body></html>\n";
              sendit.append("Content-Type: text/html\n");
              sendit.append("Content-Length: "+to_string(error.size())+"\n");
              sendit.append("\n");
              sendit.append(error);
              sendBuffSize = sendit.size();
              sendBuff = (char*)malloc(sendBuffSize*sizeof(char));
              sendit.copy(sendBuff,sendBuffSize,0);
            }else{
              string targetExt = target.substr(target.find_last_of("."));
              string targetType;
              bool extSupported = false;
              for(int i = 0; i<contentTypes.size();i++){
                if(contentTypes.at(i).extension.compare(targetExt)==0){
                  extSupported = true;
                  targetType = contentTypes.at(i).tag;
                }
              }
              if(!extSupported){
                sendit.append(version + " 501 Not Implemented\n");
                string error = "<html><body>501 Not Implemented Unsupported File Type: "+targetExt+"</body></html>\n";
                sendit.append("Content-Type: text/html\n");
                sendit.append("Content-Length: "+to_string(error.size())+"\n");
                sendit.append("\n");
                sendit.append(error);
                sendBuffSize = sendit.size();
                sendBuff = (char*)malloc(sendBuffSize*sizeof(char));
                sendit.copy(sendBuff,sendBuffSize,0);
              }else{
                int fileLength;
                targetFile.seekg(0, targetFile.end);
                fileLength = targetFile.tellg();
                targetFile.seekg(0, targetFile.beg);
                sendit.append(version + " 200 OK\n");
                sendit.append("Content-Type: "+targetType+" \n");
                sendit.append("Content-Length: "+to_string(fileLength)+"\n");
                sendit.append("\n");
                sendBuffSize = sendit.size()+fileLength;
                sendBuff = (char*)malloc(sendBuffSize*sizeof(char));
                sendit.copy(sendBuff,sendit.size(),0);
                targetFile.read(sendBuff+sendit.size(),fileLength);
                //sendit.append(buff.str());
              }
            }
          }
        //  string hostName = hostLine.subtsr(hostLine.find(" ")+1);
        //  int contentLength = stoi(lengthLine.substr(lengthLine.find(" "+1)));
        //  printf("%s","String received from and resent to the client:");
        //  puts(buf);

          cout <<"Sending: "<<sendBuff <<endl;
          send(connfd, sendBuff, sendBuffSize, 0);
        }
        // if (n < 0){
        //   printf("%s\n", "Read error");
        // }
        shutdown(connfd, SHUT_RDWR);
        close(connfd);
        cout << "Closed connection: " << connfd << endl;
        exit(0);
      }
    }
    close(listenfd);
    //cout << "Closed listening: " << listenfd << endl;
    munmap(keepLooping, sizeof(*keepLooping));
    exit(0);



}
