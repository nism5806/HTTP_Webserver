# HTTP_Webserver
By: Nick Smith

This webserver, coded in C++, handles HTTP/1.0 and HTTP/1.1 GET requests. It supports file types with extensions .htm, .html, .txt, .gif, .jpg, and .jpeg. The server sends the client the file it requests if the file is found in the root directory and the file type is supported. The server also handles multiple simultaneous requests.

Compile the server using the command "g++ -std=c++11 -o webserver webserver.cpp". This will create the executable file "webserver". Run this file using the command "./webserver". The server should now be running. Press enter in the terminal to end the server and close connections.
