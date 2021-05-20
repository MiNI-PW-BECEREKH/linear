// I declare that this piece of work which is the basis for recognition of achieving learning outcomes in the OPS course wascompleted on my own. Suheyb Becerek, 295448


#define _GNU_SOURCE 
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <signal.h>
#include <netdb.h>
#include <fcntl.h>
#include <pthread.h>
#include <semaphore.h>

#define ERR(source)(perror(source),\
                    fprintf(stderr,"%s:%d\n",__FILE__,__LINE__),\
                    exit(EXIT_FAILURE))

#define BACKLOG 3

struct arguments {
        int fd;
        int16_t time;
        struct sockaddr_in addr;
        sem_t* semaphore;
};

volatile sig_atomic_t do_work=1;
int clientNumber = 0;

void usage(char * name){
        fprintf(stderr,"USAGE: %s socket port\n",name);
}

void sigint_handler(int sig)
{
    do_work = 0;
}

int sethandler(void (*f)(int),int sigNo)
{
    struct sigaction act;
    memset(&act,0,sizeof(struct sigaction));
    act.sa_handler = f;
    if(-1 == sigaction(sigNo,&act,NULL))
        return -1;
    return 0;
}

// int make_socket(char* name, struct sockaddr_un *addr)
// {
//     //declare 
//     int socketFileDescriptor;
//     //creates a local STREAM based socket
//     if((socketFileDescriptor = socket(PF_UNIX,SOCK_STREAM,0))<0)
//         ERR("socket");
//     //fills the address 0 bytes
//     memset(addr,0,sizeof(struct sockaddr_un));
//     //make the sun_family AF_UNIX since local socket
//     addr->sun_family = AF_UNIX;
//     //copy the name to the addr->sun_path 
//     strncpy(addr->sun_path,name,sizeof(addr->sun_path)-1);
//     return socketFileDescriptor;
// }

int make_socket(int domain, int type)
{
    int sock;
    //create a socket in the given domain with given type
    sock = socket(domain,type,0);
    if(sock < 0 )ERR("socket");
    //return the created socket from this wrapper function.
    return sock;
}


int bind_tcp_socket(uint16_t port)
{
    struct sockaddr_in addr;
    int socketFileDescriptor,t=1;
    //utilize make_socket for TCP socket
    socketFileDescriptor = make_socket(AF_INET,SOCK_STREAM);
    memset(&addr,0,sizeof(struct sockaddr_in));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    //why we use setsockopt here in this way?
    //sets the SO_REUSEADDR option of socket on socket level to t which is 1
    //allows new program to rebind to the IP/port
    if(setsockopt(socketFileDescriptor,SOL_SOCKET,SO_REUSEADDR,&t,sizeof(t)))
        ERR("setsockopt");
    if(bind(socketFileDescriptor,(struct sockaddr*)&addr,sizeof(addr))<0)
        ERR("bind");
    if(listen(socketFileDescriptor,BACKLOG)<0)
        ERR("listen");
    fprintf(stdout,"TCP socket bound\n");
    return socketFileDescriptor;

}

int add_new_client(int sfd)
{
    int nfd;
    //accept a new connection on socket - extract the first connection on the queue that is pending. Return the file descriptor of the accepted socket.d
    if((nfd = TEMP_FAILURE_RETRY(accept(sfd,NULL,NULL)))<0)
    {
        //O_NONBLOCK set for the socket file descriptor and no connections are present to be accepted then return directly -1 which is fail
        if(EAGAIN == errno || EWOULDBLOCK == errno)
            return -1;
        ERR("accept");
    }
    //return accepted socket
    return nfd;
}




ssize_t bulk_read(int fd,char *buf, size_t count)
{
    int c;
    size_t len  = 0;
    do
    {
        //read to the buffer as much as count from fd
        c = TEMP_FAILURE_RETRY(read(fd,buf,count));
        if(c<0)return c;
        if(0==c) return len;
        buf+=c;
        len+=c;
        count-=c;
    } while (count >0);
    return len;
    
}

ssize_t bulk_write(int fd,char *buf, size_t count)
{
    int c;
    size_t len = 0;
    do
    {
        c = TEMP_FAILURE_RETRY(write(fd,buf,count));
        if(c<0)return c;
        buf += c;
        len += c;
        count -=c;
    } while (count >0);
    return len;
    
}

void communicate(int connectedFileDescriptor){
    clientNumber++;
    char messageToClient[100] = {0};
    snprintf(messageToClient,100,"you are player#%d,wait please..",clientNumber);
    if(bulk_write(connectedFileDescriptor, messageToClient, 100) < 0 && errno!=EPIPE) ERR("write:");
    if(TEMP_FAILURE_RETRY(close(connectedFileDescriptor))<0)ERR("close");
}


void doServer(int fdT, int clientNumber){
        int connectedFileDescriptor;//,fileDescriptorMax;

        //ssize_t size;
        //Initialize 2 ready file descriptors 
        fd_set baseReadyFileDescriptors, readyFileDescriptors;
        sigset_t mask, oldmask;
        //Initialize the baseReadyFileDescriptors to null set.
        FD_ZERO(&baseReadyFileDescriptors);
        //Add the file descriptor fdL tot he baseReadyFileDescriptors
        //FD_SET(fdL, &baseReadyFileDescriptors);
        FD_SET(fdT,&baseReadyFileDescriptors);
        //fileDescriptorMax = fdT;
        //Initialize an empty signal set on mask
        sigemptyset (&mask);
        //Add SIGINT tot he mask 
        sigaddset (&mask, SIGINT);
        //Set the blocked signals union of current set and mask store old mask in the oldmask
        sigprocmask (SIG_BLOCK, &mask, &oldmask);
        while(do_work)
        {
		readyFileDescriptors=baseReadyFileDescriptors;
		
		if(pselect(fdT+1,&readyFileDescriptors,NULL,NULL,NULL,&oldmask)>0)
        {
			if((connectedFileDescriptor=add_new_client(fdT))>=0)
            {
				communicate(connectedFileDescriptor);
			}
        }
        else
        {
            if(EINTR==errno) continue;
            ERR("pselect");
        }
        
        //remove the signals in mask from block set.
        sigprocmask (SIG_UNBLOCK, &mask, NULL);
        }
}



int main(int argc, char** argv) {
        //int fdL;
        int fdT;
        int new_flags;
        if(argc!=3) {
                usage(argv[0]);
                return EXIT_FAILURE;
        }
        //IGNORES the SIGPIPE
        if(sethandler(SIG_IGN,SIGPIPE)) ERR("Seting SIGPIPE:");
        //sets do_work to 0 on every SIGINT
        if(sethandler(sigint_handler,SIGINT)) ERR("Seting SIGINT:");

        fdT = bind_tcp_socket(atoi(argv[1]));
        new_flags = fcntl(fdT,F_GETFL) | O_NONBLOCK;
        fcntl(fdT,F_SETFL,new_flags);
        //start server computations
        doServer(fdT,atoi(argv[2]));
        //if(TEMP_FAILURE_RETRY(close(fdL))<0)ERR("close");
        //if(unlink(argv[1])<0)ERR("unlink");
        if(TEMP_FAILURE_RETRY(close(fdT))<0)ERR("close");
        fprintf(stderr,"Server has terminated.\n");
        return EXIT_SUCCESS;
}