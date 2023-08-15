#include <sys/socket.h>
#include <pwd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <pthread.h>
#include <sys/types.h>
#include <grp.h>

#define DATA_BLOCK_LENGTH 512
#define MESSAGE_LENGTH 500
#define NUM_THREADS 100

//Function signatures
void *connection_handler(void *);
int is_user_in_group(char* , gid_t* , int);

//Global mutex lock to be used for all threads on server
pthread_mutex_t lock_x;

int main() {
    //Initiate global variables
	int socket_desc;
	int client_sock;
	int con_size;
	int thread_count = 0;
	pthread_t client_conn[NUM_THREADS];
	struct sockaddr_in server, client;

	//Create socket to be used by server
	socket_desc = socket(AF_INET, SOCK_STREAM, 0);

    //Check if socket was created successfully
	if( socket_desc == -1)
		perror("Could not create socket:\n");
	else
		printf("Socket successfully created.\n");

	//Set socket variables
	server.sin_port = htons(8082);
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;

	//Bind socket to server address and check if it fails
	if (bind(socket_desc, (struct sockaddr *) &server, sizeof(server)) < 0 )
	{
		perror("Issues binding socket to server:\n");
		return 1;
	} else
	{
		printf("Socket successfully bound.\n");
	}

	//Set socket to listen
	listen(socket_desc, 3);

	//Allow for any connections to server
	printf("Waiting for incoming connection from client\n");
	con_size = sizeof(struct sockaddr_in);

	//Initialise the mutex lock to be used to synchronise the threads
	pthread_mutex_init(&lock_x, NULL);

    //Server will continually loop through as long as it's running waiting for any clinet connections
	while ( (client_sock = accept(socket_desc, (struct sockaddr *) &client, (socklen_t *) &con_size )))
    {
		//Creates thread to handle client connection
		if ( pthread_create( &client_conn[thread_count], NULL, connection_handler, (void*) &client_sock ) < 0)
		{
			perror("Error creating thread:");
			return 1;
		}
		pthread_join(client_conn[thread_count], NULL);
		thread_count++;
	}

	return 0;
}

void *connection_handler(void *socket_desc)
{
	//Gets the socket descriptor
	int sock = *(int *) socket_desc;
	char message[MESSAGE_LENGTH];
	uid_t client_user_id;
	char directory[20];
	int READSIZE;
	
	//Receives user ID from the client
	READSIZE = recv(sock, &client_user_id, sizeof(client_user_id), 0);

    //Check to see if the connection occurred correctly
	if (READSIZE == -1)
	{
		printf("Error occurred in recv() call\n");
		exit(EXIT_FAILURE);
	}

    //Converts the user ID
    client_user_id = ntohl(client_user_id);

	//Mutex locked to prevent multiple clients transferring at the same time
	pthread_mutex_lock(&lock_x);
	printf("\nThread has been locked.\n");


    //Sets up variables for the user and user groups
	gid_t *groups;
	struct passwd *user_info;
	user_info = getpwuid(client_user_id);

	//Gets the client's user name
	char *user_name = user_info -> pw_name;
    printf( "Client's username is: %s\n", user_name );

    //Sets up how many groups to check
	int ngroups = 20;
	groups = malloc(ngroups * sizeof(gid_t));

    //Retrieves list of groups user is in
	if (getgrouplist(user_name, client_user_id, groups, &ngroups) == -1 )
	{
	    //If else if block to handle any errors
        if (errno == EFAULT)
        {
            perror("Memory allocation error:\n");
            exit(EXIT_FAILURE);
        } else if (errno == EINVAL)
        {
            perror("Invalid arguments:\n");
            exit(EXIT_FAILURE);
        } else if (errno == EPERM)
        {
            perror("Insufficient permissions:\n");
            exit(EXIT_FAILURE);
        } else if (errno == -1)
        {
            perror("Error retrieving group list:\n");
            exit(EXIT_FAILURE);
        }
	}

	printf("Client's groups:");
    for(int i = 0; i < ngroups; i++) {

        printf(" -%d", groups[i]);
    }

    //Sets effective user ID
	seteuid(client_user_id);

    //Clears the buffer of the message
    memset(message,'\0',sizeof(message));
	
	//Receives the desired directory from client
	READSIZE = recv(sock, message, MESSAGE_LENGTH, 0);

	//Check to see if the data was received correctly
	if (READSIZE == -1)
	{
		printf("Error occurred in recv() call\n");
		exit(1);
	}

    strcpy(directory, message);

    //Checks if user is apart of the group
    if (!is_user_in_group(directory, groups, ngroups))
    {
        write( sock, "notInGroup", strlen("notInGroup") );
        sleep(10);
        close(sock);
        pthread_mutex_unlock(&lock_x);
        pthread_exit(NULL);
		printf("\nTransfer cancelled user not in group.\n");
    }

    printf("User is in group. Sending directory received message.\n");
    write( sock, "directoryReceived", strlen("directoryReceived") );

    //Clears the buffer again
    memset(message,'\0',sizeof(message));
	
	//Reads message from client
	READSIZE = recv(sock, message, MESSAGE_LENGTH, 0);

	//Checks if the client is ready to transfer
	if (strcmp(message, "initTransfer") == 0)
	{
		printf("Init Transfer\n");
		write(sock, "fileRequest", strlen("fileRequest") );
	}

    //Clear message buffer again
    memset(message,'\0',sizeof(message));

    //Read message from client
	READSIZE = recv(sock, message, MESSAGE_LENGTH, 0);

	//Check to see if it's time to start the file transfer
	if ( strcmp(message, "initTransfer") != 0 && strlen(message) > 0)
	{
		printf("Starting file transfer.\n");
		write( sock, "startTransfer", strlen("startTransfer") );
		printf("Filename: %s\n", message);

		//Set up directory to transfer file to
		char fr_path[200] = "/home/fluffyhobo/systems_software/ca2/src/server_files/";
	    strcat( fr_path, directory);
		strcat( fr_path, "/");
		printf("File read path is %s\n", fr_path);

        //Set up the data receive buffer
		char data_receive_buffer[DATA_BLOCK_LENGTH];
		char *fr_name = (char *) malloc( 1 + strlen(fr_path) + strlen(message) );
		strcpy( fr_name, fr_path );
		strcat( fr_name, message );
		printf("fr_name: %s\n", fr_name);

        //Open the file
		FILE *fr = fopen(fr_name, "w");
		if (fr == NULL)
		{
			printf("File %s cannot be opened in the server, errno: %d\n", fr_name, errno);
			exit(EXIT_FAILURE);
		}

        //Clear the data buffer received from client
        memset(data_receive_buffer,'\0',sizeof(data_receive_buffer));
        int file_block_size = 0;

        //Retrieve file data from client
        while ( (file_block_size = recv(sock, data_receive_buffer, DATA_BLOCK_LENGTH, 0) ) > 0 ) {
            int write_sz = fwrite(data_receive_buffer, sizeof(char), file_block_size, fr);
            if (write_sz < file_block_size) {
                perror("File write failed on the server\n");
            }
            memset(data_receive_buffer,'\0',sizeof(data_receive_buffer));
        }

        //Check to see if the file was written to properly
        if (file_block_size < 0)
        {
            fprintf(stderr, "Receive failed due to error: %d\n", errno);
            exit(1);
        }

        //Close file reader after finishing
		printf("Ok received from the client\n");
		fclose(fr);

		//Reset effective user ID
		seteuid(0);

		printf("\nEffective user ID has been reset\n");

		//Wait a short while to ensure server won't be clogged
		sleep(10);

        printf("\nSuccefully transferred file to %s\nUnlocking thread\n", directory);

		//Unlock mutex to allow another thread to run
		pthread_mutex_unlock(&lock_x);
	}

    //Clear message buffer
    memset(message,'\0',sizeof(message));

    //Ensure there is nothing left in the buffer
	if (READSIZE == 0)
	{
		puts("Client disconnected successfully\n");
		fflush(stdout);

	} else if (READSIZE == -1)
	{
		perror("Receive failed:\n");
	}

	return 0;
}

int is_user_in_group(char* group_name, gid_t* groups, int num_groups)
{
    struct group *user_groups;
    //Check all groups and return 1 if the groups match signifying the user is in the group
    for (int i = 1; i < num_groups; i++)
    {
        user_groups = getgrgid(groups[i]);
        if (strcmp(group_name, user_groups -> gr_name) == 0)
        {
            return 1;
        }
    }
    return 0;
}
