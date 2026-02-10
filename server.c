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
#include <pthread.h>

#define PORT "8080" // the port users will be connecting to
#define BACKLOG 10 // how many pending connections queue will hol

// it was used for fork
/*
void sigchld_handler(int s)
{
    (void)s;

    int saved_errno = errno;
    while(waitpid(-1,NULL,WNOHANG) > 0);
    errno = saved_errno;
} */

//get sockadder
void *get_in_addr(struct sockaddr *sa)
{
    if(sa->sa_family == AF_INET)
    {
        return &(((struct sockaddr_in*)sa) -> sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

//create a thread to handle the client connection, and detach the thread so that it can clean up after itself when it's done
static void* server_echo(void* arg)
{
	pthread_detach(pthread_self());
	
	int client_fd = *(int*) arg;
    free(arg);
    arg = NULL;
	
	char buf[1024];
	
	while(1)
	{
		ssize_t n = recv(client_fd,buf,sizeof(buf),0); //use ssize_t to avoid the problem of comparing signed and unsigned integers
		if(n>0)
		{
			ssize_t sent = 0;
            // make sure every byte is sent, since send() may not send all bytes in one call
			while(sent < n)
			{
				ssize_t m = send(client_fd, buf + sent, (size_t)(n-sent),0);
				if(m<0)
				{
					perror("send");
					close(client_fd);
					return NULL;
				}
				sent += m;
			}
		}
		else if(n == 0)
		{
			printf("server: connection closed by client\n");
			close(client_fd);
			return NULL;
		}
		else
		{
			if(errno == EINTR) //interrupted by signal
			continue;
			perror("recv");
			close(client_fd);
			return NULL;
		}
	}
}

int main(void)
{
    //listen on sock_fd, new connection on client_fd
    int sockfd, client_fd;
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage their_addr; // connector's address info

    socklen_t sin_size;
    struct sigaction sa;
    int yes = 1;
    char s [INET6_ADDRSTRLEN];
    int rv;

    memset(&hints,0,sizeof hints);
    hints.ai_family = AF_INET;  //IPV4 (127.0.0.1)
    hints.ai_socktype = SOCK_STREAM; //TCP
    hints.ai_flags = AI_PASSIVE;  // use my IP

    //get the linklist of addrinfo structures
    if((rv = getaddrinfo(NULL,PORT,&hints,&servinfo)) != 0)
    {
        fprintf(stderr,"getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    // loop through all the results and bind to the first we can
    for(p = servinfo; p != NULL; p = p->ai_next)
    {
        if((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
        {
            perror("server: socket");
            continue;
        }

        if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1)
        {
            perror("setsockopt");
            exit(1);
        }

        if(bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)
        {
            close(sockfd);
            perror("server: bind");
            continue;
        }

        break;
    }

    freeaddrinfo(servinfo);

    if(p == NULL)
    {
        fprintf(stderr,"server:failed to bind\n");
        exit(1);
    }

    if(listen(sockfd, BACKLOG) == -1)
    {
        perror("listen");
        exit(1);
    }

    /*
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }*/

    printf("server: waiting for connections...\n");

   while(1)
   {
        sin_size = sizeof their_addr;
        client_fd = accept(sockfd, (struct sockaddr *)&their_addr,&sin_size);
        if(client_fd < 0)
        {
            perror("accept");
            continue;
        }
        
        //everytime allocate memory for a new client connection, and free it in the thread function
        int* pfd = malloc(sizeof(int));
        if(!pfd)
        {
            perror("malloc");
            close(client_fd); //close the resource with associated client connection
            continue;
        }
        *pfd = client_fd;
        
        pthread_t tid;
        int rc = pthread_create(&tid,NULL,server_echo,pfd); 
        if(rc != 0) //create thread unsuccessfully
        {
            fprintf(stderr,"pthread_create: %s\n",strerror(rc));
            close(client_fd);
            free(pfd); 
            pfd = NULL;
            continue;
        }
                
        inet_ntop(their_addr.ss_family, get_in_addr((struct sockaddr *)&their_addr), s, sizeof s);
        printf("server: got connection from %s\n", s); 
   }
    return 0;
}


