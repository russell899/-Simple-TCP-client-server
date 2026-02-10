#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <string.h>

//#define PORT "8080" // the port clients will be connecting to

//#define MAXDATASIZE 100 // max number of bytes we can get at once

//get sockaddr, IPV4 or IPV6:
void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//print usage message
static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s -h <server_ip/host> -p <port>\n",prog);
}



int main(int argc, char *argv[])
{
    int sockfd;
    char sendbuf[128];
    char recvbuf[128];
    struct addrinfo hints, *servinfo, *p;
    int rv;
    char s[INET6_ADDRSTRLEN];
    int opt;
    int count = 1;
    char host[100] = {0};
    char port[20] = {0};

    //parse command line arguments
    while((opt = getopt(argc, argv, "h:p:")) != -1)
    {
        switch(opt)
        {
            case 'h':
                strncpy(host, optarg,sizeof(host)-1);
                break;
            case 'p':
                strncpy(port, optarg, sizeof(port)-1);
                break;
            default:
                usage(argv[0]);
                return 1;
        }
    }

    if(host[0] == '\0' || port[0] == '\0')
    {
        usage(argv[0]);
        return 1;
    }

    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if((rv = getaddrinfo(host,port,&hints,&servinfo)) != 0)
    {
        fprintf(stderr,"getaddrinfo:%s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and connect to the server
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("client: socket");
            continue;
        }

        inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof s);
        printf("client: attempting to connect to %s\n", s);

        if(connect(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            perror("client: connect");
            close(sockfd);
            continue;
        }
        break;
    }

    if(p == NULL)
    {
        fprintf(stderr,"client: failed to connect\n");
        return 2;
    }

    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr),s,sizeof s);
    printf("client: connecting to %s\n", s);

    freeaddrinfo(servinfo); // all done with this structure

   //send data to server and show echoed data from server
   while(1)
   {
        int len = snprintf(sendbuf, sizeof sendbuf, "count=%d\n", count);
        if(len < 0 || len >= (int)sizeof sendbuf)
        {
            fprintf(stderr,"client: message too long\n");
            break;
        }

        //send all bytes
        int sent = 0;
        while(sent < len)
        {
            int n = send(sockfd, sendbuf + sent, len - sent, 0);
            if(n == -1)
            {
                perror("client: send");
                goto end_connection;
            }
            sent += n;
        }

        //receive echoed data
        int r = recv(sockfd, recvbuf, sizeof recvbuf - 1, 0);
        if(r == -1)
        {
            perror("client: recv");
            goto end_connection;
        }
        else if(r == 0)
        {
            printf("client: server closed connection\n");
            goto end_connection;
        }
        recvbuf[r] = '\0';
        printf("client: sent: %s",sendbuf);
        printf("client: recv:%s", recvbuf);
        count++;
        sleep(1);
   }
end_connection:
close(sockfd); 

    close(sockfd);  

    return 0;
}