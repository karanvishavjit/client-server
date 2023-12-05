
#include <sys/socket.h>		//for using socket API's
#include <sys/signal.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <ftw.h>
#include <netdb.h>
#include <netinet/in.h>		//to store address information
#include <netinet/ip.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>


#define SERVER_PORT 4600
#define KV_IP_MIRROR "127.0.0.1"
#define KV_PORT_MIRROR 4600
#define MAX_RESPONSE_SIZE 1024
#define KV_ARGS_MAX 7
#define MAX_BUFFER_SIZE 1024
#define MAX_EXTENSION_COUNT 4
#define KV_TEXT_RES 1
#define KV_STR_RES 2
#define RESPONSE_FILE 3
#define KV_CONN_SERVER 'S'
#define KV_CONN_MIRROR 'M'
#define HOME_DIR getenv("HOME")
#define TEMP_TAR "temp1.tar.gz"
#define TEMP_FILELIST "temp_filelist.txt"


// Struct kvAddrInfo to tranfer Mirror IP & Port no. 
typedef struct {
    char kv_ipAddr[INET_ADDRSTRLEN];
    int kv_portNum;
} kvAddrInfo; 

// Global Variables
int client_no = 0;

// Function to handle client commands
void processClient(int clientSockfd);


// Functions to handle various commands
void findfile(int clientSockfd, char** arguments);
void filesrch(int clientSockfd, char** arguments);

int getdirf(int clientSockfd, char** arguments);
int tarfgetz(int clientSockfd, char** arguments);
int kvFgets(int clientSockfd, char** arguments, int argLen);
int targzf(int clientSockfd, char** arguments, int kvExtCount);

// Transmit data to client
int kvSendFile(int clientSockfd, const char* filename);
int kvSendMessage( int clientSockfd, char* buffer);



// Functions for recursive file tree search
int kvSearchExtReccur(char *dir_name, char **file_types, int kvExtCount, int *fileCount);
int kvSearchDateReccur(char *root_path, time_t date1, time_t date2, int *fileCount);
int kvSearchNameReccur(char *dir_name, char **file_names, int filenameCount, int *fileCount);
int kvSearchSizeReccur(char *dir_name, int size1, int size2, int *fileCount);


int executeTarCommand(const char* kvSrcFiles, const char* kvDestArchive);
void cleanupTempFiles();

// Function to convert input time to unix time
time_t kvDateUnix(const char *time_str, int dateType);


/**
 * Functin to remoe newLIne character
 * */
void kvRemveNewLine(char* kvStr) {
	
	//get lenght of input string
    size_t kvLen = strlen(kvStr);

    //terminate string with null terminating charcter if new line exists
    if (kvLen > 0 && kvStr[kvLen - 1] == '\n') {
        kvStr[kvLen - 1] = '\0';
    }
}

// Main Function
int main(int argc, char *argv[])
{
	int kvSocketDes, kvClientScoketDes, kvPortNum, kvConnStatus;
	socklen_t len;
	struct sockaddr_in servAdd;	//ipv4
	
	if (argc == 2) {
        kvPortNum = atoi(argv[1]);
    } else {
        kvPortNum = SERVER_PORT;
    }

	// Create Socket
	if ((kvSocketDes = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		//printf("[-] Error Creating Socket\n");
		//exit(1);
		perror("TCP Server-Socket Error");
        exit(EXIT_FAILURE);
	}

	servAdd.sin_family = AF_INET;
	servAdd.sin_addr.s_addr = htonl(INADDR_ANY);	
	servAdd.sin_port = htons(kvPortNum);

	bind(kvSocketDes, (struct sockaddr *) &servAdd, sizeof(servAdd));
	listen(kvSocketDes, 5);
	printf(" Server is up and Listening on Port Number >>  %d\n", kvPortNum);
	while (1)
	{	
		// Increment client number 
		client_no++;
		
		// Determine which server to connect: primary server or mirror
		
		// Connect 
			kvClientScoketDes = accept(kvSocketDes, (struct sockaddr *) NULL, NULL);
			printf("Client %d connected\n", client_no);
			
			// Fork a child process to handle client request 
			if (!fork()){	 
				processClient(kvClientScoketDes);
				close(kvClientScoketDes);
				exit(0);
			}
			waitpid(0, &kvConnStatus, WNOHANG);
	
	}
}	

void processClient(int kvclientsd)
{
	char kvCmmndBuff[MAX_BUFFER_SIZE];
	char responseText[MAX_BUFFER_SIZE];

	
	kvSendMessage(kvclientsd, "Enter command :->");

	while (1)
	{	
		
		// Clear the command buffer
		memset(kvCmmndBuff, 0, sizeof(kvCmmndBuff)); 
		
		int kvIpBytes = read(kvclientsd, kvCmmndBuff, MAX_BUFFER_SIZE);
		kvRemveNewLine(kvCmmndBuff);
		
		// Check if no data received (client closed connection)
		if (kvIpBytes <= 0) { 
           printf("Disconnected Client Number: %d\n", client_no);
           break; 
		}
		
		printf("Command from client: %s\n", kvCmmndBuff);
		
		char *kvCmndArgs[KV_ARGS_MAX];
		int num_arguments = 0;
		
		// Parse the command received from client
		char* kvToken = strtok(kvCmmndBuff, " "); // Tokenize command using space as delimiter
		char* kvCommand = kvToken; // Store the first token in cmd
		
		while (kvToken != NULL) {
			kvToken = strtok(NULL, " "); // Get the next token
			if (kvToken != NULL) { // Check if token is not NULL before storing it
				kvCmndArgs[num_arguments++] = kvToken; // Store the token in the array
			}
		}
		kvCmndArgs[num_arguments] = NULL; // Set the last element of the array to NULL	
		
		printf("Individual command: %s\n",kvCommand);
		
		// Process the command and generate response
		if (strcmp(kvCommand, "filesrch") == 0)
		{
			// Call the function to handle request
			filesrch(kvclientsd, kvCmndArgs);
		}
		else if (strcmp(kvCommand, "tarfgetz") == 0)
		{
			// Call the function to handle request
			int resultsgetfiles = tarfgetz(kvclientsd, kvCmndArgs);
			
			// Check the result of the function call
               if (resultsgetfiles == 1) {
					kvSendMessage(kvclientsd, "Exception while runnig command.");
					printf("Exception while runnig: tarfgetz\n");
				}
		}
		else if (strcmp(kvCommand, "getdirf") == 0)
           {
			   // Call the function to handle request
               int resultdgetfiles = getdirf(kvclientsd, kvCmndArgs);
			   
			   // Check the result of the function call
               if (resultdgetfiles == 1) {
					kvSendMessage(kvclientsd, "Exception while runnig command.");
					printf("Exception while runnig: getdirf\n");
				}
		}
		else if (strcmp(kvCommand, "fgets") == 0)
		{	
			// Call the function to handle request
			int resultgetfiles = kvFgets(kvclientsd, kvCmndArgs, num_arguments);
			
			// Check the result of the function call
			if (resultgetfiles == 1) {
				kvSendMessage(kvclientsd, "Exception while runnig command.");
				printf("Exception while runnig: kvFgets\n");
			}
		}
		else if (strcmp(kvCommand, "targzf") == 0)
		{
   			// Call the function to handle request
			int result = targzf(kvclientsd, kvCmndArgs, num_arguments);

			// Check the result of the function call
			if (result == 1) {
				kvSendMessage(kvclientsd, "Exception while runnig command.");
				printf("Exception while runnig: targzf\n");
			}

		}

		else if (strcmp(kvCommand, "quit") == 0)
		{		
			printf("Client entered quit\n");
	
			// Acknoledge quit request and close the socket
			kvSendMessage(kvclientsd, "Connection closed by server");
            close(kvclientsd); 
			printf("Socket closed!!!.\n");
            break;
            exit(0); 
		}
		
		else
		{	
			// Invalid command, inform client 
			kvSendMessage(kvclientsd, "Not a valid command\n");
			continue;	// Continue to next iteration of loop to wait for new command
		}
	}
}



/**
 * this funciton will send message to client
 * */

int kvSendMessage(int kvClientSd, char* kvMessage){
	
	// Send response type AS text response
	long kvRespType = KV_TEXT_RES;

		//send response type to client and return error in case of failure
    if (send(kvClientSd, &kvRespType, sizeof(kvRespType), 0) == -1) {
        perror("Error sending response type");
        return -1;
    }

    printf("Sending Message to client : %s\n", kvMessage);

    // Send response text to Client
    size_t kvMessagelen = strlen(kvMessage);
    ssize_t kvBytesSent = 0;

    	//while number of bytes sent is less than actual lenth of message send the response to client
    while (kvBytesSent < kvMessagelen) {
        ssize_t kvNumBytesSent = send(kvClientSd, kvMessage + kvBytesSent, kvMessagelen - kvBytesSent, 0);
        if (kvNumBytesSent == -1) {
            perror("Error sending message");
            return -1;
        }
        kvBytesSent += kvNumBytesSent;
    }

    return 0;
}

/**
 * 
 * Open file read its content and send the file to client
 * */
int kvSendFile(int kvFileScoket, const char* kvfilename) {
    
    	// Open file to transfer
    int kvFileDes = open(kvfilename, O_RDONLY);
    if (kvFileDes < 0) {
        perror("operation failed failed");
        exit(EXIT_FAILURE);
    }

    char kvBuff[MAX_RESPONSE_SIZE];
    ssize_t kvBytesRead;

    // Read file contents and send to client
    while ((kvBytesRead = read(kvFileDes, kvBuff, MAX_RESPONSE_SIZE)) > 0) {
        send(kvFileScoket, kvBuff, kvBytesRead, 0);
    }

    // Close file and socket
    close(kvFileDes);

    return 0;

}
	


/**
 * Function to execute command by opening a pipe
 * */

char* kvExecuteCommand(const char* kvFindCmnd) {

	//create a pipe to execute commadn in read mode and return file pointer that can be used to read the output
    FILE* kvPipe = popen(kvFindCmnd, "r");
    if (kvPipe != NULL) {
    	//read the ouput of find command
        char* kvLine = NULL;

        //variable to store results after reading from pipe
        size_t kvLenght = 0;
        ssize_t kvRead;

        //read line from pointer to file 'kvPipe' and store it into 'kvLine'
        // 'kvRead!=1' remove any newLine characters and return teh command output
        if ((kvRead = getline(&kvLine, &kvLenght, kvPipe)) != -1) {
            kvRemveNewLine(kvLine);
            pclose(kvPipe);
            return kvLine;
        }
        
        //free the memory and close pipe
        free(kvLine);
        pclose(kvPipe);
    } else {
        printf("Error opening pipe to command\n");
    }

    return NULL;
}

void filesrch(int kvclientsd, char** kvargs) {
	
    // filename is first argument
    char* kvfilename = kvargs[0];

    printf("Starting findfile process\n");
    printf("Filename: %s\n", kvfilename);

    char response[MAX_RESPONSE_SIZE];

    char* kvhomeDir = HOME_DIR;
    char kvFindCmd[strlen(kvhomeDir) + strlen(kvfilename) + 27];
    sprintf(kvFindCmd, "find %s -name '%s' -print -quit", kvhomeDir, kvfilename);

    printf("Execute findFile command: %s\n", kvFindCmd);

    //call 'kvExecuteCommand' function and get output in 'kvLine'
    char* kvLine = kvExecuteCommand(kvFindCmd);

    //if command executed successfully
    if (kvLine != NULL) {

        struct stat sb;
        	//stat funciton call to retrieve information specified by 'kvLIne'
        	//if it returns 0 stat function retrieved successfully
        if (stat(kvLine, &sb) == 0) {

            
            time_t kvFileTime = sb.st_mtime;;

            //kvFileTime = sb.st_birthtime;

            	//convert it to human redable format
            char* time = ctime(&kvFileTime);

            	//remove any newline characters
            kvRemveNewLine(time);

            	//populate response with all the file information
            sprintf(response, "%s \n File Name: %s\n File Size: %lld bytes\n,File Creation date: %s", kvLine,kvfilename, (long long)sb.st_size, time);
        } else {

            sprintf(response, "Unable to get file information for %s", kvLine);
        }

        //free memory
        free(kvLine);
    } else {

        sprintf(response, "File not found.");
    }

    kvSendMessage(kvclientsd, response);
}


int kvSearchExtReccur(char *kvDirectory, char **kvExtTypes, int kvExtCount, int *kvFileCount) {
    DIR *kvDir;
    struct dirent *kvEnt;
    struct stat kvBufStat;

    if ((kvDir = opendir(kvDirectory)) == NULL) {
        perror("Exception while opening directory");
        return 1;
    }

    	//opne temporary file list in append mode
    FILE *kvFilePtr = fopen(TEMP_FILELIST, "a");
    if (kvFilePtr == NULL) {
        perror("Failed to open file");
        return 1;
    }

    	//iterate through each directory
    while ((kvEnt = readdir(kvDir)) != NULL) {

    			//skip the current and parent directory
        if (strcmp(kvEnt->d_name, ".") == 0 || strcmp(kvEnt->d_name, "..") == 0) {
            continue;
        }

        printf("Reading directory: %s\n", kvEnt->d_name);

        	//buffer to stor complete path of each directory
        char kvBuff[MAX_BUFFER_SIZE];

        	//construct complete path by concatenating directy name and entry in directory
        snprintf(kvBuff, sizeof(kvBuff), "%s/%s", kvDirectory, kvEnt->d_name);

        	//return info specified by 'kvBuff'
        if (stat(kvBuff, &kvBufStat) == -1) {
            perror("Error getting file stats");
            continue;
        }

        	//differentiate between file and directory

        if (S_ISDIR(kvBufStat.st_mode)) {

        		//recursively call funciton if it is a directory

            kvSearchExtReccur(kvBuff, kvExtTypes, kvExtCount, kvFileCount);
        } 
        	//get size of file if it is a directory
        else if (S_ISREG(kvBufStat.st_mode)) {

        	for (int i=0;i<kvExtCount;i++){

        		printf("Matching input file [%s] with physical file [%s]\n", kvExtTypes[i], kvEnt->d_name);

        		if (fnmatch(kvExtTypes[i], kvEnt->d_name, FNM_PATHNAME) == 0){

        		fprintf(kvFilePtr, "%s\n", kvBuff);


                printf("Saving to %s: %s\n", TEMP_FILELIST, kvBuff);
                
                (*kvFileCount)++;	
                	break;
        		}
        	}

        }
    }

    	//close directory and file pointer
    closedir(kvDir);
    fclose(kvFilePtr);
    return 0;

}


int targzf(int kvClientSd, char **kvArgs, int kvExtCount) {
    char* kvHomeDir = HOME_DIR;
 
    	//keep track fo num of files
    int kvFileCount = 0;

    
    char *kvExtTypes[kvExtCount];


     printf("Home directory: %s\n", kvHomeDir);
    printf("%d File names parsed:", kvExtCount);


for (int i = 0; i < kvExtCount; i++) {
        printf(" %s", kvArgs[i]);
    }
	printf("\n");
	
    if ((opendir(kvHomeDir)) == NULL) {
        printf("opendir failed");
        return 1;
    }
    

    for (int i = 0; i < kvExtCount; i++) {
        kvExtTypes[i] = malloc(strlen(kvArgs[i]) + 2);
        sprintf(kvExtTypes[i], "*.%s", kvArgs[i]);
    }

    //call file searhc function to get the file count in 'kvFileCount'

    if(kvSearchExtReccur(kvHomeDir, kvExtTypes, kvExtCount, &kvFileCount)!=0){

    	printf("specified Files not found ");

    	return 1;
    }

    printf("Number of files found in range: %d\n", kvFileCount);

    if (kvFileCount > 0) {
        printf("Executing 'tar' command >>> ");

        //if tar command is executed succesfully open the file to read it
        if (executeTarCommand(TEMP_FILELIST, TEMP_TAR) == 0) {
            printf("Tar command executed successfully\n");

            	//open file in binary read mode
            FILE* kvFilePtr = fopen(TEMP_TAR, "rb");
            if (kvFilePtr == NULL) {
                printf("Error opening file\n");
                return 1;
            }

            	//move pointer to the end of file to determine size
            fseek(kvFilePtr, 0, SEEK_END);

            //get the size in a variable
            long kvFileSize = ftell(kvFilePtr);

            	//reset file pointer
            fseek(kvFilePtr, 0, SEEK_SET);

            	//contents read into 'buffer' using fread
            char* kvBuff = (char*)malloc((kvFileSize + 1) * sizeof(char));
            fread(kvBuff, kvFileSize, 1, kvFilePtr);

				//send fileSize to clinet             	

            send(kvClientSd, &kvFileSize, sizeof(kvFileSize), 0);

            		//send the compressed file to client
            int kvFiletrf = kvSendFile(kvClientSd, TEMP_TAR);
            		

            		//check response of file transger to client
            if (kvFiletrf != 0) {
                printf("There is an error in transferring files\n");
            } else {


                printf("Transfer Successful.....\n");
            }

            	//close file pionter
            fclose(kvFilePtr);
        } else {


            printf("Exception in running command\n");
            
            return 1;
        }
    } else {
    			//file with given size not found

        printf("File not Found!!.\n");

        	//send reponse to client 
        kvSendMessage(kvClientSd, "File not Found!!.");
    }

    	//Clera temporary files
    cleanupTempFiles();


    return 0;




}


int kvSearchNameReccur(char *kvDirectory, char **file_names, int kvInputFilesCount, int *kvFileCount) {

    DIR *kvDir;
    struct dirent *kvEnt;
    struct stat kvBufStat;

    if ((kvDir = opendir(kvDirectory)) == NULL) {
        perror("Exception while opening directory");
        return 1;
    }

    	//opne temporary file list in append mode
    FILE *kvFilePtr = fopen(TEMP_FILELIST, "a");
    if (kvFilePtr == NULL) {
        perror("Failed to open file");
        return 1;
    }

    	//iterate through each directory
    while ((kvEnt = readdir(kvDir)) != NULL) {

    			//skip the current and parent directory
        if (strcmp(kvEnt->d_name, ".") == 0 || strcmp(kvEnt->d_name, "..") == 0) {
            continue;
        }

        printf("Reading directory: %s\n", kvEnt->d_name);

        	//buffer to stor complete path of each directory
        char kvBuff[MAX_BUFFER_SIZE];

        	//construct complete path by concatenating directy name and entry in directory
        snprintf(kvBuff, sizeof(kvBuff), "%s/%s", kvDirectory, kvEnt->d_name);

        	//return info specified by 'kvBuff'
        if (stat(kvBuff, &kvBufStat) == -1) {
            perror("Error getting file stats");
            continue;
        }

        	//differentiate between file and directory

        if (S_ISDIR(kvBufStat.st_mode)) {

        		//recursively call funciton if it is a directory

            kvSearchNameReccur(kvBuff, file_names, kvInputFilesCount, kvFileCount);
        } 
        	//get size of file if it is a directory
        else if (S_ISREG(kvBufStat.st_mode)) {

        	for (int i=0;i<kvInputFilesCount;i++){

        		printf("Matching input file [%s] with physical file [%s]\n", file_names[i], kvEnt->d_name);

        		if (strcmp(file_names[i], kvEnt->d_name) == 0){

        		fprintf(kvFilePtr, "%s\n", kvBuff);


                printf("Saving to %s: %s\n", TEMP_FILELIST, kvBuff);
                
                (*kvFileCount)++;	
                	break;
        		}
        	}

        }
    }

    	//close directory and file pointer
    closedir(kvDir);
    fclose(kvFilePtr);
    return 0;
}


int kvFgets(int kvClientSd, char **kvArgs, int kvInputFilesCount)
{	
	char* kvHomeDir = HOME_DIR;
 
    	//keep track fo num of files
    int kvFileCount = 0;

    


     printf("Home directory: %s\n", kvHomeDir);
    printf("%d File names parsed:", kvInputFilesCount);


for (int i = 0; i < kvInputFilesCount; i++) {
        printf(" %s", kvArgs[i]);
    }
	printf("\n");
	
    if ((opendir(kvHomeDir)) == NULL) {
        printf("opendir failed");
        return 1;
    }
    

    //call file searhc function to get the file count in 'kvFileCount'

    if(kvSearchNameReccur(kvHomeDir, kvArgs, kvInputFilesCount, &kvFileCount)!=0){

    	printf("specified Files not found ");

    	return 1;
    }

    printf("Number of files found in range: %d\n", kvFileCount);

    if (kvFileCount > 0) {
        printf("Executing 'tar' command >>> ");

        //if tar command is executed succesfully open the file to read it
        if (executeTarCommand(TEMP_FILELIST, TEMP_TAR) == 0) {
            printf("Tar command executed successfully\n");

            	//open file in binary read mode
            FILE* kvFilePtr = fopen(TEMP_TAR, "rb");
            if (kvFilePtr == NULL) {
                printf("Error opening file\n");
                return 1;
            }

            	//move pointer to the end of file to determine size
            fseek(kvFilePtr, 0, SEEK_END);

            //get the size in a variable
            long kvFileSize = ftell(kvFilePtr);

            	//reset file pointer
            fseek(kvFilePtr, 0, SEEK_SET);

            	//contents read into 'buffer' using fread
            char* kvBuff = (char*)malloc((kvFileSize + 1) * sizeof(char));
            fread(kvBuff, kvFileSize, 1, kvFilePtr);

				//send fileSize to clinet             	

            send(kvClientSd, &kvFileSize, sizeof(kvFileSize), 0);

            		//send the compressed file to client
            int kvFiletrf = kvSendFile(kvClientSd, TEMP_TAR);
            		

            		//check response of file transger to client
            if (kvFiletrf != 0) {
                printf("There is an error in transferring files\n");
            } else {


                printf("Transfer Successful.....\n");
            }

            	//close file pionter
            fclose(kvFilePtr);
        } else {


            printf("Exception in running command\n");
            
            return 1;
        }
    } else {
    			//file with given size not found

        printf("File not Found!!.\n");

        	//send reponse to client 
        kvSendMessage(kvClientSd, "File not Found!!.");
    }

    	//Clera temporary files
    cleanupTempFiles();


    return 0;



}


/**
 * Fucntion to recursively search files inside directories withing sizse range
 * */
int kvSearchSizeReccur(char *kvDirectory, int size1, int size2, int *kvFileCount) {


    DIR *kvDir;
    struct dirent *kvEnt;
    struct stat kvBufStat;

    if ((kvDir = opendir(kvDirectory)) == NULL) {
        perror("Exception while opening directory");
        return 1;
    }

    	//opne temporary file list in append mode
    FILE *kvFilePtr = fopen(TEMP_FILELIST, "a");
    if (kvFilePtr == NULL) {
        perror("Failed to open file");
        return 1;
    }

    	//iterate through each directory
    while ((kvEnt = readdir(kvDir)) != NULL) {

    			//skip the current and parent directory
        if (strcmp(kvEnt->d_name, ".") == 0 || strcmp(kvEnt->d_name, "..") == 0) {
            continue;
        }

        printf("Reading directory: %s\n", kvEnt->d_name);

        	//buffer to stor complete path of each directory
        char kvBuff[MAX_BUFFER_SIZE];

        	//construct complete path by concatenating directy name and entry in directory
        snprintf(kvBuff, sizeof(kvBuff), "%s/%s", kvDirectory, kvEnt->d_name);

        	//return info specified by 'kvBuff'
        if (stat(kvBuff, &kvBufStat) == -1) {
            perror("Error getting file stats");
            continue;
        }

        	//differentiate between file and directory

        if (S_ISDIR(kvBufStat.st_mode)) {

        		//recursively call funciton if it is a directory

            kvSearchSizeReccur(kvBuff, size1, size2, kvFileCount);
        } 
        	//get size of file if it is a directory
        else if (S_ISREG(kvBufStat.st_mode)) {

            int kvFileSize = kvBufStat.st_size;
            printf("File %s with Size %d\n", kvEnt->d_name, kvFileSize);

            	//if size is between specified range append in temporary list
            if (kvFileSize >= size1 && kvFileSize <= size2 && size1 <= size2) {
            		//append file to temporary list
                fprintf(kvFilePtr, "%s\n", kvBuff);


                printf("Saving to %s: %s\n", TEMP_FILELIST, kvBuff);
                
                (*kvFileCount)++;
            }
        }
    }

    	//close directory and file pointer
    closedir(kvDir);
    fclose(kvFilePtr);
    return 0;
}

/**
 * Execute archive command
 * */
int executeTarCommand(const char* kvSrcFiles, const char* kvDestArchive) {
    char kvCommand[MAX_BUFFER_SIZE];

    	//construct 'tar' command to be executed to create archive
    snprintf(kvCommand, MAX_BUFFER_SIZE, "tar -czf %s -T %s", kvDestArchive, kvSrcFiles);
    printf("%s\n",kvCommand);
    return system(kvCommand);
}

/**
 * Clear out temporary fies
 * */
void cleanupTempFiles() {
		//remove system call to clear temporary files
    if(remove(TEMP_FILELIST)!=0){
    	printf("Exception while deleting Temporary files!!");
    }
    else if(remove(TEMP_TAR)!=0){
    	printf("Exception while deleting Temporary Archive!!");	
    }
    else{
    	printf("Temporary files cleared!!");
    }
    
}

int tarfgetz(int kvClientSd, char** kvArgs) {
    char* kvHomeDir = HOME_DIR;

    //get size from command line arguments
    size_t size1 = atoi(kvArgs[0]);
    size_t size2 = atoi(kvArgs[1]);
    
    	//keep track fo num of files
    int kvFileCount = 0;

    

    printf("Getting files in below range.\n");
    printf("Start Range: %ld\n", size1);
    printf("End Range: %ld\n", size2);

    
    //call file searhc function to get the file count in 'kvFileCount'

    kvSearchSizeReccur(kvHomeDir, size1, size2, &kvFileCount);

    printf("Number of files found in range: %d\n", kvFileCount);

    if (kvFileCount > 0) {
        printf("Executing 'tar' command >>> ");

        //if tar command is executed succesfully open the file to read it
        if (executeTarCommand(TEMP_FILELIST, TEMP_TAR) == 0) {
            printf("Tar command executed successfully\n");

            	//open file in binary read mode
            FILE* kvFilePtr = fopen(TEMP_TAR, "rb");
            if (kvFilePtr == NULL) {
                printf("Error opening file\n");
                return 1;
            }

            	//move pointer to the end of file to determine size
            fseek(kvFilePtr, 0, SEEK_END);

            //get the size in a variable
            long kvFileSize = ftell(kvFilePtr);

            	//reset file pointer
            fseek(kvFilePtr, 0, SEEK_SET);

            	//contents read into 'buffer' using fread
            char* kvBuff = (char*)malloc((kvFileSize + 1) * sizeof(char));
            fread(kvBuff, kvFileSize, 1, kvFilePtr);

				//send fileSize to clinet             	

            send(kvClientSd, &kvFileSize, sizeof(kvFileSize), 0);

            		//send the compressed file to client
            int kvFiletrf = kvSendFile(kvClientSd, TEMP_TAR);
            		

            		//check response of file transger to client
            if (kvFiletrf != 0) {
                printf("There is an error in transferring files\n");
            } else {


                printf("Transfer Successful.....\n");
            }

            	//close file pionter
            fclose(kvFilePtr);
        } else {


            printf("Exception in running command\n");
            
            return 1;
        }
    } else {
    			//file with given size not found

        printf("File not Found!!.\n");

        	//send reponse to client 
        kvSendMessage(kvClientSd, "File not Found!!.");
    }

    	//Clera temporary files
    cleanupTempFiles();


    return 0;


}



int kvSearchDateReccur(char *kvDirectory, time_t date1, time_t date2, int *kvFileCount)
{
	DIR *kvDir;
    struct dirent *kvEnt;
    struct stat kvBufStat;

    if ((kvDir = opendir(kvDirectory)) == NULL) {
        perror("Exception while opening directory");
        return 1;
    }

    	//opne temporary file list in append mode
    FILE *kvFilePtr = fopen(TEMP_FILELIST, "a");
    if (kvFilePtr == NULL) {
        perror("Failed to open file");
        return 1;
    }

    	//iterate through each directory
    while ((kvEnt = readdir(kvDir)) != NULL) {

    			//skip the current and parent directory
        if (strcmp(kvEnt->d_name, ".") == 0 || strcmp(kvEnt->d_name, "..") == 0) {
            continue;
        }

        printf("Reading directory: %s\n", kvEnt->d_name);

        	//buffer to stor complete path of each directory
        char kvBuff[MAX_BUFFER_SIZE];

        	//construct complete path by concatenating directy name and entry in directory
        snprintf(kvBuff, sizeof(kvBuff), "%s/%s", kvDirectory, kvEnt->d_name);

        	//return info specified by 'kvBuff'
        if (stat(kvBuff, &kvBufStat) == -1) {
            perror("Error getting file stats");
            continue;
        }

        	//differentiate between file and directory

        if (S_ISDIR(kvBufStat.st_mode)) {

        		//recursively call funciton if it is a directory

            kvSearchDateReccur(kvBuff, date1, date2, kvFileCount);
        } 
        	//get size of file if it is a directory
        else if (S_ISREG(kvBufStat.st_mode)) {

            time_t kvFileTime = kvBufStat.st_mtime;;
            printf("File %s with Date: %d\n", kvEnt->d_name, kvFileTime);

            	//if size is between specified range append in temporary list
            if (kvFileTime >= date1 && kvFileTime <= date2)
            {
            		//append file to temporary list
                fprintf(kvFilePtr, "%s\n", kvBuff);


                printf("Saving to %s: %s\n", TEMP_FILELIST, kvBuff);
                
                (*kvFileCount)++;
            }
        }
    }

    	//close directory and file pointer
    closedir(kvDir);
    fclose(kvFilePtr);
    return 0;
}

int getdirf(int kvClientSd, char** kvArgs)
{
    
    char* kvHomeDir = HOME_DIR;

    //get date from command line arguments
    time_t date1 = kvDateUnix(kvArgs[0], 1);

    time_t date2 = kvDateUnix(kvArgs[1], 2);
    
    
    	//keep track fo num of files
    int kvFileCount = 0;

    

    printf("Getting files in below range.\n");
    printf("Start Range: %ld\n", date1);
    printf("End Range: %ld\n", date2);

    
    //call file searhc function to get the file count in 'kvFileCount'

    kvSearchDateReccur(kvHomeDir, date1, date2, &kvFileCount);

    printf("Number of files found in range: %d\n", kvFileCount);

    if (kvFileCount > 0) {
        printf("Executing 'tar' command >>> ");

        //if tar command is executed succesfully open the file to read it
        if (executeTarCommand(TEMP_FILELIST, TEMP_TAR) == 0) {
            printf("Tar command executed successfully\n");

            	//open file in binary read mode
            FILE* kvFilePtr = fopen(TEMP_TAR, "rb");
            if (kvFilePtr == NULL) {
                printf("Error opening file\n");
                return 1;
            }

            	//move pointer to the end of file to determine size
            fseek(kvFilePtr, 0, SEEK_END);

            //get the size in a variable
            long kvFileSize = ftell(kvFilePtr);

            	//reset file pointer
            fseek(kvFilePtr, 0, SEEK_SET);

            	//contents read into 'buffer' using fread
            char* kvBuff = (char*)malloc((kvFileSize + 1) * sizeof(char));
            fread(kvBuff, kvFileSize, 1, kvFilePtr);

				//send fileSize to clinet             	

            send(kvClientSd, &kvFileSize, sizeof(kvFileSize), 0);

            		//send the compressed file to client
            int kvFiletrf = kvSendFile(kvClientSd, TEMP_TAR);
            		

            		//check response of file transger to client
            if (kvFiletrf != 0) {
                printf("There is an error in transferring files\n");
            } else {


                printf("Transfer Successful.....\n");
            }

            	//close file pionter
            fclose(kvFilePtr);
        } else {


            printf("Exception in running command\n");
            
            return 1;
        }
    } else {
    			//file with given size not found

        printf("File not Found!!.\n");

        	//send reponse to client 
        kvSendMessage(kvClientSd, "File not Found!!.");
    }

    	//Clera temporary files
    cleanupTempFiles();


    return 0;
}

	/**
	 * Function to converto date to unix format and return
	 * */
time_t kvDateUnix(const char *kvInputTime, int kvAmPm) {

    	//initialize struc to all zeros
    struct tm kvtimeInfo = { 0 };

    	//variable to store year month and day
    int year, month, day;

    	//extract 'year', 'month', 'day' from input date
    if (sscanf(kvInputTime, "%d-%d-%d", &year, &month, &day) != 3) {
        perror("Invalid format of date");
        return (time_t)-1;
    }

    	//struct tm represent number since 1900, hence need to adjust year

    kvtimeInfo.tm_year = year - 1900;

    	//month in struct tm start from 0, hence we need to adjust month value
    kvtimeInfo.tm_mon = month - 1;
    kvtimeInfo.tm_mday = day;

    	//if kvAmPm is 1 (start of the date), it sets the time to 00:00:00.
    	// else set it to 23:59:59
    kvtimeInfo.tm_hour = (kvAmPm == 1) ? 0 : 23;
    kvtimeInfo.tm_min = (kvAmPm == 1) ? 0 : 59;
    kvtimeInfo.tm_sec = (kvAmPm == 1) ? 0 : 59;
    
    	// mktime determine Day light saving is effectie or not

    kvtimeInfo.tm_isdst = -1;  

    	//convert struct tm to unix timestamp and store it into a variable
    time_t kvUnixTime = mktime(&kvtimeInfo);

    if (kvUnixTime == (time_t)-1) {
        perror("Exception while converting to Unix timestamp");
        return (time_t)-1;
    }

    return kvUnixTime;
}
