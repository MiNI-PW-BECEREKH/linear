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
        int* fd;
        int playerNumber;
        int position;
        int boardsize;
        int move;
        //sending array of semaphores are not in the shared memory it does not unlock in parent
        sem_t semaphoreCurrentPosition;
        sem_t semaphoreNextPosition;
};

volatile sig_atomic_t do_work=1;
int clientNumber = 0;
char* board;
int gamestart= 0;
int gamestartpoint = 0;
int* connectedFileDescriptors;

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

int make_socket(int domain, int type)
{
    int sock;
    //create a socket in the given domain with given type
    sock = socket(domain,type,0);
    if(sock < 0 )ERR("socket");
    //return the created socket from this wrapper function.
    return sock;
}

int findPos(int x,int boardSize)
{
    int pos = -1;
    for (int i = 0; i < boardSize; i++)
    {
        if(board[i] == x)
        {
            pos = i;
        }
    }
    return pos;
    
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

void communicate(int connectedFileDescriptor,int* connectedFileDescriptors){
    
    char messageToClient[100] = {0};
    char errmsg[] = "Cannot serve to more than 5 client!!\n";
    if(clientNumber < 5)
    {
        snprintf(messageToClient,100,"you are player#%d,wait please..\n",clientNumber);
        ++clientNumber;
        if(bulk_write(connectedFileDescriptor, messageToClient, 100) < 0 && errno!=EPIPE) ERR("write:");
        //Connect to the empty one
        int i;
        for( i = 0 ; i < 5; ++i )
        {
            fprintf(stdout,"Checking %d\n",connectedFileDescriptors[i]);
            if(connectedFileDescriptors[i] < 0)//if the socket is not used
            {
                connectedFileDescriptors[i] = connectedFileDescriptor;
                fprintf(stdout,"%d = %d\n",connectedFileDescriptors[i],connectedFileDescriptor);
                break;
            }
        }
    }
    else{
        if(bulk_write(connectedFileDescriptor, errmsg, sizeof(errmsg)) < 0 && errno!=EPIPE) ERR("write:");
        if(TEMP_FAILURE_RETRY(close(connectedFileDescriptor))<0)ERR("close");
    }
}


void* communicateThread(void *arg){
        struct arguments  *args= (struct arguments*) arg;
        //int tt;
        //fprintf(stderr,"Will sleep for %d\n",ntohs(args->time));
        //for (tt = ntohs(args->time); tt > 0; tt = sleep(tt));
        char msg[100] = {0}; 
        strcpy(msg,"The Game has Started.\n");
        // for (int i = 0; i < args->boardsize; i++)
        // {
        //     strcpy(msg,"|");
        //     strcpy(msg,args->board[i]);
        //     strcpy(msg,"|");

        // }

        char *buf = malloc(100);
        char buf2[100];
        buf[0] = '\0';
        strncat(buf, "|", 100-strlen(buf)-1);
        for (int i = 0; i < args->boardsize; ++i) {
            if(board[i] == 32) strncpy(buf2, " ", sizeof(buf2));
            else if (board[i] >= 0) snprintf(buf2, sizeof(buf2), "%d", board[i]);
            strncat(buf, buf2, 100-strlen(buf)-1);
            strncat(buf, "|", 100-strlen(buf)-1);
        }
        strncat(buf, "\n", 100-strlen(buf)-1);
        

        if(gamestartpoint == 0)
        if(TEMP_FAILURE_RETRY(bulk_write(*(args->fd) ,msg,sizeof(msg)))<0) ERR("sendto");

        if(TEMP_FAILURE_RETRY(bulk_write(*(args->fd) ,buf,strlen(buf)))<0) ERR("sendto");
        //if (sem_post(args->semaphore) == -1) ERR("sem_post");
        free(args);
        return NULL;
}

void movethePlayer(struct arguments  *args)
{
    board[(args->position + args->move)] = board[args->position];
    board[args->position] = ' ';
}

// void movePlayerOrKick(void * arg)
// {
//     struct arguments  *args= (struct arguments*) arg;
//     if(args->position + args->move > args->boardsize ||args->position + args->move < 0 )
//         {
//             if(TEMP_FAILURE_RETRY(bulk_write(args->fd,"you lost:stepped out of board",sizeof("you lost:stepped out of board")))<0) ERR("sendto");
//             connectedFileDescriptors[args->playerNumber] = -1;
//             board[args->position] = ' ';
//         }
//         else{
//             if (TEMP_FAILURE_RETRY(sem_trywait(&args->semaphoreNextPosition) == -1)) {

//                         switch(errno){
//                         case EAGAIN:   movethePlayer(args);
//                         }
//                         ERR("sem_wait");
//                 }
//             movethePlayer(args);
//             if ((sem_post(&args->semaphoreNextPosition) == -1)) 
//             {ERR("sem_post");}
//         }
//             if ((sem_post(&args->semaphoreCurrentPosition) == -1)) 
//         {ERR("sem_post");}
// }


void* movePlayer(void *arg){
        struct arguments  *args= (struct arguments*) arg;
        //int tt;
        //fprintf(stderr,"Will sleep for %d\n",ntohs(args->time));
        //for (tt = ntohs(args->time); tt > 0; tt = sleep(tt));
        //printf("%d",args->position + args->move);
        //if(sem_trywait(&args->semaphore[args->position]) == -1)ERR("sem_wait");
        // if (TEMP_FAILURE_RETRY(sem_trywait(&args->semaphoreCurrentPosition) == -1)) {
        //                 switch(errno){
        //                 case EAGAIN:   movePlayerOrKick(arg);
        //                 }
        //                 ERR("sem_wait");
        //         }
        // movePlayerOrKick(arg);
        if (TEMP_FAILURE_RETRY(sem_trywait(&args->semaphoreCurrentPosition) == -1));//does nothing if resource is not available
        if(args->position + args->move > args->boardsize ||args->position + args->move < 0 )
        {
            if(TEMP_FAILURE_RETRY(bulk_write(*(args->fd) ,"you lost:stepped out of board\n",sizeof("you lost:stepped out of board\n")))<0) ERR("sendto");
            close(*(args->fd));
            *(args->fd) = -1;
            board[args->position] = ' ';
        }
        else{
            if (TEMP_FAILURE_RETRY(sem_trywait(&args->semaphoreNextPosition) == -1));
            movethePlayer(args);
            if ((sem_post(&args->semaphoreNextPosition) == -1)) 
            {ERR("sem_post");}
        }
            if ((sem_post(&args->semaphoreCurrentPosition) == -1)) 
        {ERR("sem_post");}

        
        free(args);
        return NULL;
}



void doServer(int fdT, int player_num,int boardsize){
        int connectedFileDescriptor;//,fileDescriptorMax;
        //int connectedFileDescriptors[5] = {-1,-1,-1,-1,-1};
        connectedFileDescriptors = (int*)malloc(sizeof(int)*player_num);
        for (int i = 0; i < player_num; i++)
        {
            connectedFileDescriptors[i] = -1;
        }

        //int* numbers = {}
        
        char buffer[100];
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


        //initialize board
        board = (char*)malloc(sizeof(char)*boardsize);
        memset(board,0,boardsize);
        for (int i = 0; i < boardsize; i++)
        {
            board[i] = ' ';
        }
        

        
        pthread_t thread;
        //struct sockaddr_in addr;
        struct arguments *args;
        socklen_t size=sizeof(struct sockaddr_in);
        sem_t* boardSemaphores = (sem_t*)malloc(sizeof(sem_t)*boardsize);
        for (int i = 0; i < boardsize; i++)
        {
            if(sem_init(&boardSemaphores[i],0,1)<0)
                ERR("sem_init");
        }
        
                
        struct timespec tv;
        tv.tv_sec =1;
        tv.tv_nsec = 0;

        while(do_work)
        {
            //enters here many times

            readyFileDescriptors=baseReadyFileDescriptors;
            
            int pret = 0;
            if((pret = pselect(fdT+1,&readyFileDescriptors,NULL,NULL,&tv,&oldmask))>0)
            {
                for(int i = 0 ; i < player_num ; ++i)
                {
                    fprintf(stdout,"Keep Alive to Client %d\n",i);
                    int ret = recv(connectedFileDescriptors[i],buffer,sizeof(buffer),MSG_PEEK| MSG_DONTWAIT);
                    fprintf(stdout,"recv() returns %d\n",ret);
                    if(ret == 0){
                        fprintf(stdout,"Client %d Closed Connection\n",i);
                        close(connectedFileDescriptors[i]);
                        connectedFileDescriptors[i]=-1;
                        --clientNumber;
                    }



                }

                    if((connectedFileDescriptor=add_new_client(fdT))>=0)
                    {
                        communicate(connectedFileDescriptor, connectedFileDescriptors);

                    }
                    

            }
            else if(pret == 0)
            {
                fflush(stdout);
                // fprintf(stdout,"Timeout--polling\n");
                for(int i = 0 ; i < player_num ; ++i)
                {
                    //fprintf(stdout,"Keep Alive to Client %d\n",i);
                    int ret = recv(connectedFileDescriptors[i],buffer,sizeof(buffer),MSG_PEEK| MSG_DONTWAIT);
                    //fprintf(stdout,"recv() returns %d\n",ret);
                    if(ret == 0){
                        //fprintf(stdout,"Client %d Closed Connection\n",i);
                        close(connectedFileDescriptors[i]);
                        connectedFileDescriptors[i]=-1;
                        --clientNumber;
                    }
                    else if(ret >0 && gamestart== 1)
                    {
                        char command[100] = {0};
                        //some rule given by client
                        strcpy(command,"");
                        if((size=read(connectedFileDescriptors[i],(char *)command,sizeof(char[100])))<0) ERR("read:");
                        //fprintf(stdout,"Rule from client:\n\t%s\n",(char*)command);
                        //here parse the command
                        if(strncmp("-2",command,strlen("-2"))==0)
                        {
                            //printf("read -2 from client %d\n",i);
                            int num = i;
                            if((args=(struct arguments*)malloc(sizeof(struct arguments)))==NULL) ERR("malloc:");
                            args->fd=&connectedFileDescriptors[i];
                            args->playerNumber=num;
                            args->boardsize = boardsize;
                            args->position = findPos(i,boardsize);
                            args->move = -2;
                            args->semaphoreCurrentPosition = boardSemaphores[args->position];
                            if(args->position + args->move > 0 && args->position + args->move < boardsize)
                            {
                                args->semaphoreNextPosition = boardSemaphores[args->position + args->move];

                            }
                            if (pthread_create(&thread, NULL,movePlayer, (void *)args) != 0) ERR("pthread_create");

                        }
                        else if(strncmp("-1",command,strlen("-1"))==0)
                        {
                            //printf("read -1 from client %d\n",i);
                                                        //printf("read -2 from client %d\n",i);
                                                                                                            int num = i;

                            if((args=(struct arguments*)malloc(sizeof(struct arguments)))==NULL) ERR("malloc:");
                            args->fd=&connectedFileDescriptors[i];
                            args->playerNumber=num;
                            args->boardsize = boardsize;
                            args->position = findPos(i,boardsize);
                            args->move = -1;
                            args->semaphoreCurrentPosition = boardSemaphores[args->position];
                            if(args->position + args->move > 0 && args->position + args->move < boardsize)
                            {
                                args->semaphoreNextPosition = boardSemaphores[args->position + args->move];

                            }
                            if (pthread_create(&thread, NULL,movePlayer, (void *)args) != 0) ERR("pthread_create");

                        }
                        else if(strncmp("0",command,strlen("0"))==0)
                        {
                            int num =i;
                            if((args=(struct arguments*)malloc(sizeof(struct arguments)))==NULL) ERR("malloc:");
                            args->fd=&connectedFileDescriptors[i];
                            args->playerNumber=num;
                            args->boardsize = boardsize;
                            if (pthread_create(&thread, NULL,communicateThread, (void *)args) != 0) ERR("pthread_create");
                            //if (pthread_detach(thread) != 0) ERR("pthread_detach");


                        }
                        else if(strncmp("1",command,strlen("1"))==0)
                        {
                            //printf("read 1 from client %d\n",i);
                                                        //printf("read -2 from client %d\n",i);
                            int num = i;

                            if((args=(struct arguments*)malloc(sizeof(struct arguments)))==NULL) ERR("malloc:");
                            args->fd=&connectedFileDescriptors[i];
                            args->playerNumber=num;
                            args->boardsize = boardsize;
                            args->position = findPos(i,boardsize);
                            args->move = 1;
                            args->semaphoreCurrentPosition = boardSemaphores[args->position];
                            if(args->position + args->move > 0 && args->position + args->move < boardsize)
                            {
                                args->semaphoreNextPosition = boardSemaphores[args->position + args->move];

                            }
                            if (pthread_create(&thread, NULL,movePlayer, (void *)args) != 0) ERR("pthread_create");

                        }
                        else if(strncmp("2",command,strlen("2"))==0)
                        {
                            //printf("read 2 from client %d\n",i);
                                                        //printf("read -2 from client %d\n",i);
                                                                                                            int num = i;

                            if((args=(struct arguments*)malloc(sizeof(struct arguments)))==NULL) ERR("malloc:");
                            args->fd=&connectedFileDescriptors[i];
                            args->playerNumber=num;
                            args->boardsize = boardsize;
                            args->position = findPos(i,boardsize);
                            args->move = 2;
                            args->semaphoreCurrentPosition = boardSemaphores[args->position];
                            if(args->position + args->move > 0 && args->position + args->move < boardsize)
                            {
                                args->semaphoreNextPosition = boardSemaphores[args->position + args->move];

                            }
                            if (pthread_create(&thread, NULL,movePlayer, (void *)args) != 0) ERR("pthread_create");

                        }

                    }

                }

                if(clientNumber == player_num && gamestartpoint == 0)
                {
                    gamestart = 1;
                    
                    printf("Enough players active\n");
                    //randomize positions
                    srand(time(NULL));

                    int randomNumber = -1;
                    for(int i =0 ; i < player_num; i++)
                    {

                        randomNumber = rand() % boardsize;
                        board[randomNumber] = i;

                    }


                    


                    for (int i = 0; i < player_num; i++)
                    {
                        if((args=(struct arguments*)malloc(sizeof(struct arguments)))==NULL) ERR("malloc:");
                        args->fd=&connectedFileDescriptors[i];
                        args->playerNumber=i;
                        args->boardsize = boardsize;
                        if (pthread_create(&thread, NULL,communicateThread, (void *)args) != 0) ERR("pthread_create");
                        //if (pthread_detach(thread) != 0) ERR("pthread_detach");
                    }
                    gamestartpoint =1;
                }


            }




                else
                {
                    if(EINTR==errno) continue;
                    ERR("pselect");
                }
            }
            //remove the signals in mask from block set.
            sigprocmask (SIG_UNBLOCK, &mask, NULL);
        }
        



int main(int argc, char** argv) {
        //int fdL;
        int fdT;
        int new_flags;
        if(argc!=4) {
                usage(argv[0]);
                return EXIT_FAILURE;
        }
        int num_players = atoi(argv[2]);
        if(num_players < 2 && num_players> 5)
        {
            usage(argv[0]);
            return EXIT_FAILURE;
        }
        int board_size = atoi(argv[3]);
        if(board_size < num_players && board_size> 5*num_players)
        {
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
        doServer(fdT,num_players,board_size);
        //if(TEMP_FAILURE_RETRY(close(fdL))<0)ERR("close");
        //if(unlink(argv[1])<0)ERR("unlink");
        if(TEMP_FAILURE_RETRY(close(fdT))<0)ERR("close");
        fprintf(stderr,"Server has terminated.\n");
        return EXIT_SUCCESS;
}