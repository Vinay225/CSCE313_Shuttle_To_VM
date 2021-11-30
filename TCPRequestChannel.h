#ifndef _TCPreqchannel_H_
#define _TCPreqchannel_H_

#include "common.h"
using namespace std;

class TCPRequestChannel{
private:
     /* Since a TCP socket is full-duplex, we need only one.
     This is unlike FIFO that needed one read fd and another
      for write from each side  */
    int sockfd;
public:
     /* Constructor takes 2 arguments: hostname and port not
     If the host name is an empty string, set up the channel for
      the server side. If the name is non-empty, the constructor
     works for the client side. Both constructors prepare the
     sockfd  in the respective way so that it can works as a    server or client communication endpoint*/
   TCPRequestChannel (const string host_name, const string port_no);
/* This is used by the server to create a channel out of a newly accepted client socket. Note that an accepted client socket is ready for communication */
   TCPRequestChannel (int);
   /* destructor */
   ~TCPRequestChannel();
   int cread (void* msgbuf, int buflen);
   int cwrite(void* msgbuf, int msglen);
    /* this is for adding the socket to the epoll watch list */
   int getfd();
};

#endif
