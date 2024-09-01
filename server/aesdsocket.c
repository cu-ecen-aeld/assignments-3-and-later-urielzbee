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


	printf("Waiting for connections\n");
	while(terminateProcces == false)
	{
		addr_size = sizeof(client_addr);
		client_sockfd = accept(socketfd,(struct sockaddr *)&client_addr, &addr_size);
		if(client_sockfd > 0)
		{
			inet_ntop(client_addr.ss_family, get_in_addr((struct sockaddr *)&client_addr), s_client_addr, sizeof(s_client_addr));
			syslog(LOG_DEBUG, "Accepted connection from %s", s_client_addr);
		
			if(!fork())// Child proccess 
			{
				close(socketfd); // Child proccess doesn't need the listener
			
				// Receive data
				char data_in_buff[100] = {0};
				char data_out;
				int numBytes = 0;
				FILE * file;

				file = fopen("/var/tmp/aesdsocketdata", "a+");
			
				while(1)
				{
					numBytes = recv(client_sockfd, data_in_buff, 99, 0);
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
                            if(data_out != EOF)
                            {
							    printf("%c", data_out);
							    send(client_sockfd, &data_out, 1, 0);
                            }
						}while(data_out != EOF);
						break;
					}
				}
				fclose(file);

				syslog(LOG_DEBUG, "Closed connection from %s", s_client_addr);
				close(client_sockfd);
				exit(0);
			}
			close(client_sockfd); // Parrent doesn't need this socket
		}
	}
    remove("/var/tmp/aesdsocketdata");
	syslog(LOG_DEBUG, "Caught signal, exiting");
	closelog();
	
	return 0;
}
