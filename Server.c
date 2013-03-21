/*
 * Server.c
 *
 *   Created on: Mar 08, 2013
 *       Author: Nathan Zondlo
 *       		 Zachary Swartwood
 *   Assignment: Project 1:
 *  Description:
 *  			 Deliverable 1:
 *  			 -Listen on a socket bound to a specific port on the host computer.
 *  			  Accept the port number as a program argument.
 *  			 -Accept connections from a telnet client
 *  			 -When "date" is entered, return system date to the client
 *  			 -When "time" is entered, return system time to the client
 *  			 -Provide console messages to keep the user informed of what
 *  			  the server program is doing
 *  			 -Handle possible error conditions correctly
 *  			 -Continue to accept connections until the program is stopped
 *
 *  			 Deliverable 2:
 *  			 -Prints messages to a text file log created in the working
 *  			  directory in the server.
 *  			 -Error messages printed to stderr using perror()
 *  			 -Handles SIGTERM and SIGINT signals and gracefully exits program
 *
 *				 Deliverable 3:
 *				 -Added functions to make server reusable and readable
 *				 -Fixed error handling
 *				 -Logging now in function and now works properly
 */

/* Libraries to include */
#include <stdio.h>				//Standard Input/Output
#include <stdlib.h>				//Standard Library
#include <errno.h>				//Error Handling
#include <string.h> 			//String function Library
#include <netinet/in.h>			//Network sockets
#include <sys/socket.h>			//System sockets
#include <unistd.h>
#include <time.h>				//Time and Date functions
#include <signal.h> 			// for the signal function


/*
 * To stay true to the spirit of errors,
 *  our errors were redefined as macros
 *
 * This lets us change our errors in one place,
 *  and takes away some overhead by relegating it to the preprocessor
 *
 */
#define PORT_ASSIGN_STATUS (const char *)("Port")
#define SOCKET_ASSIGN_STATUS (const char *)("Socket Assign")
#define SOCKET_BIND_STATUS (const char *)("Socket Bind")
#define SOCKET_LISTEN_STATUS (const char *)("Socket Listen")
#define SOCKET_CLOSE_STATUS (const char *)("Socket Close")
#define CLIENT_CONNECT_STATUS (const char *)("Client Connection")
#define CLIENT_CLOSE_STATUS (const char *)("Client Close")
#define BUFFER_WRITE_STATUS (const char *)("Buffer write")
#define BUFFER_READ_STATUS (const char *)("Buffer read")


/* logMessage has prototype because in more than one function */
void logMessage(char *message);


/* global variable to control server and client loops */
int 	Exit;

/* Handler for Signals */
void signalHandler( int signal ){

	/* Print to console that we're exiting and show type of signal */
	printf("Graceful Exit with %s %d\n", strsignal(signal), signal);

	/* Breaks loop when signalHandler returns */
    Exit = 1;

    return;

}

/* Sets the signal handler for graceful exiting */
void setSignalHandler(){

	/*
	 * Create struct of type sigaction
	 * sa_handler = the signal handler we created aboce
	 * sa_flags = 0, means the function you're waiting on does not get restarted
	 *
	 * sigaction() is called when it detects Ctrl+C (SIGINT)
	 * 	or kill process (SIGTERM) command
	 * 	throws error EINVAL if it fails
	 */
	struct 	sigaction sig;
	sig.sa_handler = signalHandler;
	sig.sa_flags = 0;
	sigemptyset( &sig.sa_mask );

	// handle SIGINT differently
	if ( sigaction( SIGINT, &sig, NULL ) == EINVAL){
		logMessage("Error assigning the handler for SIGINT\n");
	}

	// handle SIGTERM differently
	if ( sigaction( SIGTERM, &sig, NULL ) == EINVAL ){
		logMessage("Error assigning the handler for SIGTERM\n");
	}
}

/* Returns the System Time in a formatted string */
char * getTime(char * tempTimer){

	/* Timer variable to output time to user */
	time_t 	timer;
	timer = time(NULL);

	/* Temporary string for date construction */
	char 	temp[50];
	memset(&temp, 0, sizeof( temp ));

	/* Formats timer into time and stores it in tempTimer */
	strftime(temp, 50, "%I:%M:%S %p ", localtime(&timer));
	strcpy(tempTimer, temp);
	return tempTimer;
}

/* Returns the System Date in a formatted string */
char * getDate(char * tempTimer){

	/* Timer variable to output date for user */
	time_t 	timer;
	timer = time(NULL);

	/* Temporary string for date construction */
	char 	temp[50];
	memset(&temp, 0, sizeof ( temp ));

	/* Formats timer into date and stores it in tempTimer */
	strftime(temp, 50, "%b %d %Y ", localtime(&timer));
	strcpy(tempTimer, temp);
	return tempTimer;
}

/* Prints message passed in to the file log with Date and Timestamp */
void logMessage(char *message){

	/* Log File and result for */
	FILE 	*logFile;
	int 	result;

	/* Date and time strings for time-stamping */
	char 	tempDate[50];
	char 	tempTime[50];
	memset(&tempDate, 0, sizeof (tempDate));
	memset(&tempTime, 0, sizeof (tempTime));

	/*
	 * If logFile assignment == NULL
	 * 		print error to stderr
	 * else
	 * 		print to logFile
	 * 		if closing logFile returns End Of File (EOF) error
	 * 			print error to stderr
	 */
	if ((logFile = fopen("ServerLog.txt", "a")) == NULL)
		perror("File did not open");
	else{
		fprintf(logFile, "%s %s - %s\n", getDate(tempDate), getTime(tempTime), message);
		if((result = fclose(logFile)) == EOF){
			perror("File failed to close");
		}
	}
}

/* Checks suspect if -1 and outputs bad message to log file, else log a successful message */
void checkError(int suspect, const char * statusMessage){

	char 	temp[100];

	/*
	 * If we get error
	 * 		print to log with FAILED
	 * else we are fine
	 * 		print to log with SUCCESS
	 */
	if(suspect == -1){
		snprintf(temp, sizeof(temp), "%s %s", statusMessage, "FAILED");
		perror( temp );
		logMessage(temp);
		Exit = 1;
	}else{
		snprintf(temp, sizeof(temp), "%s %s", statusMessage, "SUCCESS");
		logMessage(temp);
	}
}


/* Checks if the port passed in as an argument is valid, returns -1 if not */
int portAssign(int *port, int argc, char **argv){

	/* If there's an argument
	 * 		*port = int cast of the argument
	 */
	if (argc > 1){
		*port = strtol(argv[1], NULL, 0);
	}

	/* If invalid port
	 * 		return error
	 */
	if( *port <= 0 || *port > 65535){
		errno = EINVAL;
		return -1;
	}

	return 0;
}

/* Sets the port, binds and listens*/
int setSocket(int *result, int port){

	char 	temp[255];
	int 	sockfd;

	/*
	 * if result was an error
	 * 		sockfd is an error
	 * else
	 * 		construct socket
	 */
	if (*result == -1){
			sockfd = -1;
	}else{
		/* Declare/Initialize socket */
		sockfd = socket( AF_INET, SOCK_STREAM, 0 );
		snprintf(temp, sizeof(temp),"Socket Started - port number: %d", port);
		logMessage(temp);
	}
	checkError(sockfd, SOCKET_ASSIGN_STATUS);

	/* declare address variable */
	struct sockaddr_in    address;

	/* set the address to the computer's */
	memset( &address, 0 , sizeof( address ) );
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons( port );


	/* Bind socket */
	*result = bind( sockfd, (struct sockaddr *) &address, sizeof( address ) );
	checkError(*result, SOCKET_BIND_STATUS);

	/* listen for socket, maximum 5 connections */
	*result = listen(sockfd, 5);
	if(*result != -1) printf("Server started on %d...\n", port);
	checkError(*result, SOCKET_LISTEN_STATUS);

	return sockfd;
}


/* The conversations between the client and the socket */
void clientConversation(int clientfd){

	//Temp string variables
	char 	temp[255];
	char 	tempTimer[50];

	//Read/write buffers
	char 	read_buffer[255];
	char 	write_buffer[255];

	//number a characters written and read
	int 	bytes_read;
	int 	bytes_written;

	//used for closing the client
	int 	result;

	/* Sentinel for client conversation loop */
	int 	talking;


	/* Sets all of write_buffer to 0 */
	memset( write_buffer, 0, sizeof( write_buffer) );
	strcpy(write_buffer, "Enter date, time, or done.\n");

	/* Write the message */
	bytes_written = write( clientfd, write_buffer, strlen( write_buffer ) );
	checkError(bytes_written, BUFFER_WRITE_STATUS);

	talking = 1;
	while(talking && Exit == 0){

		/* Reset temporary variables */
		memset(&temp, 0, sizeof( temp ));
		memset(&tempTimer, 0, sizeof( tempTimer));

		/* read() the message from client*/
		bytes_read = read( clientfd, read_buffer, 254 );
		if(Exit == 1)
			break;
		checkError(bytes_read, BUFFER_READ_STATUS);

		//Loop that cuts off the '\r\n' in read buffer.
		//Only works for 4 character entries
		int i;
		for(i = 0; i < 4; i++)
		{
			temp[i] = read_buffer[i];
		}

		/***************************************************************************************/

		//Checks if date was entered
		if(strcasecmp(temp, "date") == 0)
		{
			/* Format for date, store it in temp1, and copy to buffer */
			logMessage("Client requested date");
			snprintf(write_buffer, sizeof(write_buffer), "System date: %s\n", getDate(tempTimer));
		}
		//Checks if time was entered.
		else if(strcasecmp(temp, "time") == 0)
		{
			/* Format for time, store it in temp1, and copy to buffer */
			logMessage("Client requested time");
			snprintf(write_buffer, sizeof(write_buffer), "System time: %s\n", getTime(tempTimer));
		}
		//Checks if done was entered
		else if(strcasecmp(temp, "done") == 0)
		{
			strcpy(write_buffer, "Disconnecting...\n");
			talking = 0;
		}
		//If incorrect message was entered
		else{
			strcpy(write_buffer, "Text entered was invalid.\n"
					"Please enter 'date','time',or 'done'.\n");
		}
		/****************************************************************************************/

		/* write the message */
		bytes_written = write( clientfd, write_buffer, strlen( write_buffer ) );
		checkError(bytes_written, BUFFER_WRITE_STATUS);

	}
	/* Close client connection */
	result = close( clientfd );
	if(result == 0){
		printf("Client disconnected...\n");
	}
	checkError(result, CLIENT_CLOSE_STATUS);
}

/*
 * This loop signifies where the server part actually starts
 * Will keep looping to ask for input until server is killed manually
 *
 * Initialize global var Exit to 0
*/
void runServer(int sockfd, int *Exit){

	/* File Descriptor for client connection*/
	int 	clientfd;

	while(*Exit == 0)
	{

		/* declare a variables for the client address */
		struct sockaddr_in      client_addr;
		int                     client_addr_len;

		/* initialize the client address */
		memset( &client_addr, 0, sizeof( client_addr ) );

		/* call accept to get a connection */
		clientfd = accept( sockfd, (struct sockaddr *) &client_addr, (unsigned int *) &client_addr_len );
		if(*Exit == 1)
			break;

		/* Error check on the connected client */
		if(clientfd != -1) printf("Client connected...\n");
		checkError(clientfd, CLIENT_CONNECT_STATUS);

		/* Conversation with client starts here*/
		clientConversation(clientfd);

	}
}

/* Main code execution starts here */
int main(int argc, char **argv)
{
	/* Writes our opening message to log file */
	logMessage("Open Program");

	/* Used to store the port number argument */
	int port;
	int result;

	/* Used for creating and binding the socket */
	int socketID;

	/* Sets the signals */
	setSignalHandler();

	/* Assigns the port and checks error */
	result = portAssign(&port, argc, argv);
	checkError(result, PORT_ASSIGN_STATUS);

	/* Construct socket, binds and listens */
	socketID = setSocket(&result, port);
	if(socketID == -1)
		return (1);

	/* Accepts clients and allows communication*/
	Exit = 0;
	runServer(socketID, &Exit);

	/* Close socket connection */
	result = close( socketID );
	if(result != -1) printf("Server closed.\n");
	checkError(result, SOCKET_CLOSE_STATUS);

	/* Log final message and end program */
	logMessage("Close Program\n");
	return(0);
}
