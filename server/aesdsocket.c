#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <stdbool.h>
#include <errno.h>

#include <pthread.h>
#include <time.h>
#include <sys/queue.h>

pthread_mutex_t lock; 

struct node
{
	pthread_t thread;
	TAILQ_ENTRY(node) nodes;
};

#define MAX_NUM_OF_CONNECTIONS 2

volatile bool terminateProcces = false;

// get sockaddr, IPv4 or IPv6:
void *get_in_addr(struct sockaddr *sa)
{
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in*)sa)->sin_addr);
    }

    return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

void interrupt_handler(int sig)
{
	if(sig == SIGTERM)
	{
		terminateProcces  = true;
	}
	else if(sig == SIGINT)
	{
		terminateProcces = true;
	}
}

void* send_receive_thread(void* param)
{
	int thread_client_sockfd = *((int*)param);
	// Receive data
	char data_in_buff[100] = {0};
	int data_out;
	int numBytes = 0;
	FILE * file;
	
	pthread_mutex_lock(&lock);
	file = fopen("/var/tmp/aesdsocketdata", "a+");
		
	while(1)
	{
		numBytes = recv(thread_client_sockfd, data_in_buff, 99, 0);
		data_in_buff[numBytes] = '\0';
		printf("Data length: %d, Data: %s \n", numBytes, data_in_buff);
		for(int x = 0; x < numBytes; x++)
		{
        	printf("0x%02X ", data_in_buff[x]);
		}
		printf("\n");
		fwrite(data_in_buff, sizeof(char), strlen(data_in_buff), file);
		if(data_in_buff[numBytes - 1] == '\n')
		{
			fseek(file, 0L, SEEK_SET);
			do
			{
				data_out = fgetc(file);
				if(data_out < 1)
				{
   					printf("fgetc error: %s\n", strerror(errno));
					break;
				}
				if(data_out != EOF)
				{
				    printf("%c", data_out);
				    send(thread_client_sockfd, &data_out, 1, 0);
				}
			}while(data_out != EOF);
			break;
		}
	}
	fclose(file);

	syslog(LOG_DEBUG, "Closed connection from %d", thread_client_sockfd);
	//syslog(LOG_DEBUG, "Closed connection from %s", s_client_addr);
	close(thread_client_sockfd);
	// exit(0); Should be replaced with the proper way to exit thread
	pthread_mutex_unlock(&lock);
	return 0;
}

void* time_thread(void* param)
{
	FILE * file;
	char outstr[256];
	time_t t;
	struct tm *tmp;	
	while(1)
	{
		pthread_mutex_lock(&lock);
		file = fopen("/var/tmp/aesdsocketdata", "a+");
		t = time(NULL);
       	tmp = localtime(&t);
		strftime(outstr, sizeof(outstr), "timestamp:%a, %d %b %Y %T %z \n", tmp);
		fwrite(outstr, sizeof(char), strlen(outstr), file);
		fclose(file);
		pthread_mutex_unlock(&lock);
		sleep(10);
	}
	syslog(LOG_DEBUG, "Closed timer thread\n");
	return NULL;
}

int main(int argc, char **argv)
{
    int status;
    int socketfd;
    struct addrinfo hints;
    struct addrinfo * servinfo;
    bool daemon = false;

    openlog(argv[0], LOG_PID, LOG_USER);
    if(argc == 2)
    {
        if(strcmp(argv[1], "-d") == 0)
        {
            daemon = true;
        }
    }

        
	struct sigaction logout;
	logout.sa_handler = interrupt_handler;
	//sigaction(SIGINT, &logout, NULL);
	//sigaction(SIGTERM, &logout, NULL); 
		
    // Creating the socket
    socketfd = socket(AF_INET, SOCK_STREAM, 0);
    if(socketfd == -1)
    {
        printf("Socket creation error");
        exit(-1);
    }

    // Getting server info
	memset(&hints, 0 ,sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	status = getaddrinfo(NULL, "9000", &hints, &servinfo);
	if( status != 0)
	{
	    printf("Get Addres info  error");
	    exit(-1);
	}
	
	int yes=1;
	// lose the pesky "Address already in use" error message
	setsockopt(socketfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

	//Bind the socket to the port
	status =  bind(socketfd, servinfo->ai_addr, servinfo->ai_addrlen);
	if(status == -1)
	{
	    printf("Binding error: %s\n", strerror(errno));
	    exit(-1);
	}

	freeaddrinfo(servinfo);
    
    if(daemon)
    {
        pid_t pid = fork();
        if(pid > 0)
        {
            //Parent thread
            exit(0);
        }
        else if(pid < 0)
        {
            exit(-1);
        }
    }
	
	sigaction(SIGINT, &logout, NULL);
	sigaction(SIGTERM, &logout, NULL); 

	// Listen for upcoming connections
	status = listen(socketfd, MAX_NUM_OF_CONNECTIONS);
	if(status == -1)
	{
	    printf("Listen error");
	    exit(-1);
	}
	
	// Accept incoming connection
	int client_sockfd;
	struct sockaddr_storage client_addr;
	socklen_t addr_size;
	char s_client_addr[INET6_ADDRSTRLEN];

	if (pthread_mutex_init(&lock, NULL) != 0) { 
        printf("\n mutex init has failed\n"); 
        return 1; 
    }
	
	// Init queue
	TAILQ_HEAD(head_s, node) head;
	TAILQ_INIT(&head);
	
	struct node * e = NULL;
	e = malloc(sizeof(struct node));
	if(e == NULL)
	{
		fprintf(stderr, "malloc failed ");
		exit(EXIT_FAILURE);
	}
	
	pthread_create(&e->thread, NULL, time_thread, NULL);
	TAILQ_INSERT_TAIL(&head, e, nodes);
	e = NULL;
	printf("Waiting for connections\n");
	while(terminateProcces == false)
	{
		printf("Waiting for connections\n");
		addr_size = sizeof(client_addr);
		client_sockfd = accept(socketfd,(struct sockaddr *)&client_addr, &addr_size);
		if(client_sockfd > 0)
		{
			inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s_client_addr, sizeof(s_client_addr));
			syslog(LOG_DEBUG, "Accepted connection from %s", s_client_addr);
		
			printf("client_sockfd:%d\n", client_sockfd);
			e = malloc(sizeof(struct node));
        	if (e == NULL)
        	{
            	fprintf(stderr, "malloc failed");
            	exit(EXIT_FAILURE);
        	}
			pthread_create(&e->thread, NULL, send_receive_thread, (void*)&client_sockfd);
			TAILQ_INSERT_TAIL(&head, e, nodes);
        	e = NULL;
			//if(!fork())// Child proccess 
			//{
			//	close(socketfd); // Child proccess doesn't need the listener
				
			//	send_receive_thread(&client_sockfd);				
	
			//	syslog(LOG_DEBUG, "Closed connection from %s", s_client_addr);
			//	exit(0);
			//}
			//close(client_sockfd); // Parrent doesn't need this socket
		}
	}
	while (!TAILQ_EMPTY(&head))
    {
        e = TAILQ_FIRST(&head);
        TAILQ_REMOVE(&head, e, nodes);
        free(e);
        e = NULL;
    }
    remove("/var/tmp/aesdsocketdata");
	syslog(LOG_DEBUG, "Caught signal, exiting");
	closelog();
	pthread_mutex_destroy(&lock); 
	return 0;
}
