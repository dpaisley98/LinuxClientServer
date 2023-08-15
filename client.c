#include <sys/socket.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>

#define DATA_BLOCK_LENGTH 512
#define MESSAGE_LENGTH 500

int main(int argc, char *argv[])
{
    //Check how many arguments were given
	if (argc != 3)
	{
		printf("Error: Invalid amount of arguments, usage: ./client <file-name> <destination>\n");
		exit(EXIT_FAILURE);
	} 

    //Initiate global variables
	int SID;
	struct sockaddr_in server;
	char client_message[MESSAGE_LENGTH];
	char server_message[MESSAGE_LENGTH];
	char *filename = argv[1];
	char *directory = argv[2];
	int len;
	uid_t user_id, converted_user_id;

    //Clean buffers for client and server messages
    memset(server_message,'\0',sizeof(server_message));
    memset(client_message,'\0',sizeof(client_message));

	printf("File name passed from argument: %s\n", filename);

    //Check that directory is valid
	if ( !(strcmp(directory, "manufacturing") == 0 || strcmp(directory, "distribution") == 0 ))
	{
        printf("Error: Destination input %s invalid, input must be either \"manufacturing\" or \"distribution\".\n", directory);
        exit(EXIT_FAILURE);
	}

    //Create the socket
	SID = socket(AF_INET, SOCK_STREAM, 0);

    //Check to see if socket was setup correctly
	if (SID == -1)
		perror("Error: Socket creation failed.\n");
	else
		printf("Socket successfully created.\n");

	//Set socket variables
	server.sin_port = htons (8082);
	server.sin_addr.s_addr = inet_addr("127.0.0.1");
	server.sin_family = AF_INET;

	//Connect to the server and check if it connected
	if (connect(SID, (struct  sockaddr*)&server, sizeof(server)) < 0)
	{
		perror("Connection failed: \n");
		return 1;
	}

	printf("Connection to server succeeded.\n");

    //Get ID of client
	user_id = getuid();
	converted_user_id = htonl(user_id);
	printf("Sending user ID %d to the server.\n", user_id);

    //Send user id to the server so it can validate user permissions
	if (write(SID, &converted_user_id, sizeof(converted_user_id)) < 0)
	{
		perror("Sending ID of user to server failed:");
	    return 1;
	}

    //Send the directory which the user wants to transfer to
	if (write(SID, directory, strlen(directory)) < 0)
	{
		perror("Sending directory to server failed:\n");
       	return 1;
	}

    //Wait until the server has confirmed it has received the data
	if (recv(SID, server_message, strlen("directoryReceived"), 0) < 0)
	{
		perror("Error receiving data from server:\n");
		return 1;
	}

    //Check if the server allows the transfer
    if (strncmp(server_message, "notInGroup", strlen("notInGroup")) == 0)
    {
        printf("Transfer failed user is not in the %s group\n", directory);
        close(SID);
        exit(EXIT_FAILURE);
    }

    strcpy(client_message, "initTransfer");
    //Call the server to being transfer of data
	if (send(SID, client_message, strlen(client_message), 0) < 0)
	{
		perror("Sending call for transfer failed:\n");
		return 1;
	}

    memset(client_message,'\0',sizeof(client_message));

    //Wait until the server requests the file
	if((len = recv(SID, server_message, strlen("fileRequest"), 0)) < 0)
	{ 
		perror("Error receiving data from server.\n");
	}

	server_message[len] = '\0';
	printf("Server > %s\n", server_message);

	//Send the name of the file to the server
	if ( strcmp(server_message, "fileRequest") == 0 )
	{
		printf("Sending file name: %s\n", filename);

		if ( send(SID, filename, strlen(filename), 0) < 0 )
		{
			perror("Failed sending file to server:\n");
			return 1;
		}
	} 

    //Clear the buffer again
    memset(server_message,'\0',sizeof(server_message));
	
	//Wait for reply from server
	if ( recv(SID, server_message, MESSAGE_LENGTH, 0) < 0)
	{
		perror("Error receiving data fromm server:\n");
	}

    //Check to see if file transfer can begin
	if ( strcmp(server_message, "startTransfer") == 0 )
	{
		printf("Sending file data.\n");

        //Set up the file variables
		char *fs_path = "/home/fluffyhobo/systems_software/ca2/src/client_files/";
		char *fs_name = (char * ) malloc( 1 + strlen(fs_path) + strlen(filename) );
		char data_buffer[DATA_BLOCK_LENGTH];
		int file_block_size, i = 0;

        //Create path to file to transfer and open the file
        strcpy(fs_name, fs_path);
        strcat(fs_name, filename);
		FILE *fs = fopen(fs_name, "r");

		//Check to see if file exists
		if (fs == NULL)
		{
			printf("Error: %s, file not found\n", fs_name);
			return 1;
		}

        //Clear the data buffer
        memset(data_buffer,'\0',sizeof(data_buffer));

        //Run loop while there is still data to write to server
		while ( (file_block_size = fread(data_buffer, sizeof(char), DATA_BLOCK_LENGTH, fs)) > 0 )
		{
			printf("Data sent: %d / %d", i , file_block_size);
			if ( send(SID, data_buffer, file_block_size, 0) < 0)
			{
				fprintf(stderr, "Error: failed to send file %s, errorno: %d\n", fs_name, errno);
				exit(1);
			}

			//Clear the data buffer each time the loop iterates
			memset(data_buffer,'\0',sizeof(data_buffer));
			++i;
		}
	}

	close(SID);
	return 0;
}



