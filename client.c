#include <arpa/inet.h>
#include <ctype.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>



#define SERVER_PORT 4500
#define KV_ARGS_MAX 10

#define KV_TEXT_RES 1
#define KV_STR_RES 2
#define RESPONSE_FILE 3


#define BUFFER_SIZE 1024

#define RESPONSE_FILE 3


char kvListOfCommands[] = "List of Commands\n \
> fgets file1 file2 (upto 4) -Returns archive cotains one or more of listed files\n \
> tarfgetz size1 size2 <-u> - Returns archive of files whose sizes in bytes >=size1 and <=size2\n \
> getdirf date1 date2 <-u> 	Returns archive of files whose dates are >=date1 and <=date2\n \
> filesrch  <filename> 		Returns filename, size(in bytes), and date created\n \
> targzf  <extension list> <-u> Returns files with extensions (upto 4 file types)\n \
> *<-u> unzip temp.tar.gz in the Present Working Directory of client\n \
> quit - exit\n \
-------------------------------------------\n";

char kvArgsLimit[] = "Argument Lenght exceeded or too few Args!!\n";

// Flag for Unzip and Quit function
int kvUnzipFlag = 0, kvQuitFlag = 0, kvFileFound = 0;


// Validate Input comand from user
int kvVerifyInput(char* buffer);

// Check for Unzip option
void kvFlagUnzip(char buffer[]);

// Remove Line break from input
void kvRemveNewLine(char buffer[]);

// Validate input date
int kvVerifyDate(char date[]);

// Validate input size
int kvIsInteger(char* kvStr);


// Struct AddressInfo to tranfer Mirror IP & Port no. 
typedef struct {
    char ip_address[INET_ADDRSTRLEN];
    int port_number;
} AddressInfo;



// Main Function
int main(int argc, char *argv[]) {
    int kvSocket = 0, valread = 0;
    struct sockaddr_in serverAddress;
	struct sockaddr_in mirrorAddress;
    char commandBuffer[BUFFER_SIZE] = {0};
	char kvResponseTxt[BUFFER_SIZE];
	char kvResponseFile[BUFFER_SIZE];
	char kvValBuffer[BUFFER_SIZE];
    char kvServerIp[16];
    char *filename = "temp.tar.gz";

    if (argc < 2) {
        printf("Comand %s <kvServerIp>\n", argv[0]);
        return 1;
    }
    strcpy(kvServerIp, argv[1]);

    if ((kvSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Exception while creating socket\n");
        return 1;
    }

    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(SERVER_PORT);

    if (inet_pton(AF_INET, kvServerIp, &serverAddress.sin_addr) <= 0) {
        printf("Invalid Address or Address not supported\n");
        return 1;
    }

    if (connect(kvSocket, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) {
        printf("Connection failed\n");
        return 1;
    }
	
	long initialResponse = 0;
	recv(kvSocket, &initialResponse, sizeof(initialResponse), 0);
	
	printf("First ResponseType: %ld\n", initialResponse);
	
	// Server sends text (case: normal connection)
	if(initialResponse == KV_TEXT_RES){
		printf("Server Connected!\n");
		
		// Read greeting text
		memset(kvResponseTxt, 0, sizeof(kvResponseTxt)); // Clear the response text buffer
		read(kvSocket, kvResponseTxt, sizeof(kvResponseTxt));
		printf("Response frm server: %s\n", kvResponseTxt);
	}
	
	// Server sends structure (case: redirection)
	else if(initialResponse == KV_STR_RES)
	{	
		printf("Redirected to Mirror.\n");
		
		AddressInfo mirrorInfo;
		
		// Read structure containing Mirror IP address & Port
		recv(kvSocket, &mirrorInfo, sizeof(AddressInfo), 0);
		
		// Close connection to Main Server
		close(kvSocket);
		
		printf("Mirror IP from server: %s\n", mirrorInfo.ip_address);
		printf("Mirror Port from server: %d\n", mirrorInfo.port_number);
		
		// Create a new socket
		kvSocket = socket(AF_INET, SOCK_STREAM, 0);
		if (kvSocket < 0) {
			printf("Socket failed\n");
			return 1;
		}
		
		mirrorAddress.sin_family = AF_INET;
		mirrorAddress.sin_port = htons(mirrorInfo.port_number);
		if (inet_pton(AF_INET, mirrorInfo.ip_address, &mirrorAddress.sin_addr) <= 0) {
			printf("Invalid mirror address.\n");
			return 1;
		}
		
		// Connect to Mirror Server
		if (connect(kvSocket, (struct sockaddr *)&mirrorAddress, sizeof(mirrorAddress)) < 0) {
			printf("Connection to mirror failed.\n");
			return 1;
		}
		
		printf("Connected to the mirror server!\n");
		recv(kvSocket, &initialResponse, sizeof(initialResponse), 0);
		// Read greeting message from Mirror Server
		memset(kvResponseTxt, 0, sizeof(kvResponseTxt)); // Clear the response text buffer
		read(kvSocket, kvResponseTxt, sizeof(kvResponseTxt));
		printf("Message from server: %s\n", kvResponseTxt);
	}
	
	// Main process to interact with Server or Mirror
    while (1) {
		
		// Clear the command buffer
		memset(commandBuffer, 0, sizeof(commandBuffer)); 
		kvFileFound = 0;
		
        printf("\nshell$: ");
        fgets(commandBuffer, 1024, stdin);
		
		// Clean linebreak from input stream 
		kvRemveNewLine(commandBuffer);
		
		// Check for unzip option
		kvFlagUnzip(commandBuffer);
		
		// Validation on input
		strcpy(kvValBuffer, commandBuffer);
		if(kvVerifyInput(kvValBuffer))
			continue;
		
        // send command to server
        send(kvSocket, commandBuffer, strlen(commandBuffer), 0);

		// Receive header to identify response type
        long header;
		valread = read(kvSocket, &header, sizeof(header));

        if (valread > 0) {
			
			// Text response
			if(header == KV_TEXT_RES){
				
				memset(kvResponseTxt, 0, sizeof(kvResponseTxt)); // Clear the response text buffer
				
				// output text response
				read(kvSocket, kvResponseTxt, sizeof(kvResponseTxt));
				printf("Received response text: %s\n", kvResponseTxt);
			}
			
			// File Response
			else{
				
				kvFileFound = 1;
				
				// File size obtained from header
				long fileSize = header;
				
				memset(kvResponseFile, 0, sizeof(kvResponseFile)); // Clear the response file buffer
				
				FILE *fp = fopen(filename, "wb");
				if (fp == NULL) {
					printf("Error creating file.\n");
					return 1;
				}
				
				
				
				// Receive file data from server
				long total_bytes_received = 0;
				printf("File Size: %ld Bytes\n",fileSize);
				while (total_bytes_received < fileSize) {
					
					int bytes_to_receive = BUFFER_SIZE;
					if (total_bytes_received + BUFFER_SIZE > fileSize) {
						bytes_to_receive = fileSize - total_bytes_received;
					}
					int bytes_received = recv(kvSocket, kvResponseFile, bytes_to_receive, 0);
					if (bytes_received < 0) {
						printf("Error receiving file data");
						return 1;
					}
					fwrite(kvResponseFile, sizeof(char), bytes_received, fp);
					total_bytes_received += bytes_received;
					if (total_bytes_received >= fileSize) {
						break;
					}
				}
				
				printf("\n File received: %s\n", filename);
				fclose(fp);
			}
			
            
        } else {
			// Read 0 bytes from server ie. disconnected 
            printf("Server disconnected\n");
            break;
        }
		
		// Handle command quit 
		if(kvQuitFlag){
			printf("Bye.\n");
			break;
		}
		
		// Handle unzip option <-u> on client side
		if (kvUnzipFlag && kvFileFound) {
			char cmd[BUFFER_SIZE];
			snprintf(cmd, BUFFER_SIZE, "tar -xzf %s", filename);
			system(cmd);
			printf("File successfully unzipped \n");
			kvUnzipFlag = 0;
		}
    }
	
	// Close connection to Server / Mirror
    close(kvSocket);
    return 0;
}



int kvVerifyInput(char* cmndBuff){

	// Tokenize input
	char *kvArgs[KV_ARGS_MAX];
	int kvArgsNum = 0;

		// split input command based on spaee
	char* kvToken = strtok(cmndBuff, " "); 

		//until input is not null split the input
	while (kvToken != NULL) {
		kvArgs[kvArgsNum++] = kvToken; 
		kvToken = strtok(NULL, " "); 
	}
	
	// Set the last element to NULL
	kvArgs[kvArgsNum] = NULL; 
	
	// Verify entered commadn is from list of commands
	if (strcmp(kvArgs[0], "filesrch") != 0 &&
	    strcmp(kvArgs[0], "tarfgetz") != 0 &&
	    strcmp(kvArgs[0], "getdirf") != 0 &&
	    strcmp(kvArgs[0], "fgets") != 0 &&
	    strcmp(kvArgs[0], "targzf") != 0 &&
	    strcmp(kvArgs[0], "quit") != 0) {
		printf("Not a valid Command!!!\n");
		printf(kvListOfCommands);
		return 1;
	}
	
	// Check that arguments are given along with the command 
	if(kvArgsNum < 2 && strcmp(kvArgs[0], "quit") != 0){
		printf("Command Arguments required!!!\n");
		return 1;
	}
		// Verify Args are not exceeding limit
	else if(kvArgsNum > 7){
		printf(kvArgsLimit);
		return 1;
	}
	
		// Verify arguments  comand : filesrch
	if( strcmp(kvArgs[0], "filesrch") == 0 && kvArgsNum != 2){
		printf(kvArgsLimit);
		return 1;
	}


		//verify comand arguments of 'targzf' command
    if( strcmp(kvArgs[0], "targzf") == 0 && (kvArgsNum > 5 || kvArgsNum < 2)){
        printf("Command 'targzf' Minimum 2 and maximum 4 arguments allowed!!\n");
        return 1;
    }

 

    	//verify the command arguments in case of 'fgets' command
    if( strcmp(kvArgs[0], "fgets") == 0 && (kvArgsNum > 5 || kvArgsNum < 2)){
        printf("Command 'fgets' atleast one filename and maximum upto 4 filenames allowed!\n");
        return 1;
    }
	
		// Verify arguments for command : tarfgetz
	if (strcmp(kvArgs[0], "tarfgetz") == 0) {
		
		if(kvArgsNum != 3){
			printf(kvArgsLimit);
			printf("Command 'tarfgetz' size1 size2 <-u> \n");
			return 1;
		}
		
        if (kvIsInteger(kvArgs[1]) == 0) {
            printf("Invalid Size1 value!!\n");
			printf("Command 'tarfgetz' size1 size2 <-u> \n");
            return 1;
        }
        if (kvIsInteger(kvArgs[2]) == 0) {
            printf("Invalid Size2 value!!\n");
			printf("Command 'tarfgetz' size1 size2 <-u> \n");
            return 1;
        }

        	// Check size 1 is smaller than size2
        if (atoi(kvArgs[1]) > atoi(kvArgs[2])) {
            printf("Error : Size 1 has to be smaller than Size 2!\n");
            return 1;
        }
    }
	
		// Verify Argumetns for command : getdirf
	if (strcmp(kvArgs[0], "getdirf") == 0) {


		if(kvArgsNum !=3)
			// Verify atlest 3 arumsetns are passed
		{
			printf(kvArgsLimit);
			printf("Command 'getdirf' date1 date2 <-u>\n");
			return 1;
		}

				//Verify Date1 is valid or not

        if (kvVerifyDate(kvArgs[1]) == 0) {
            printf("Invalid Date1: Valid Format (YYYY-MM-DD)!\n");
			printf("Command 'getdirf' date1 date2 <-u>\n");
            return 1;
        }

        		//Verify Date1 is valid or not

        if (kvVerifyDate(kvArgs[2]) == 0) {
            printf("Invalid Date2: Valid Format (YYYY-MM-DD)!\n");
			printf("Command 'getdirf' date1 date2 <-u>\n");
            return 1;
        }

		struct tm tmDate1 = { 0 };
		struct tm tmDate2 = { 0 };

		int kvYear, kvMonth, kvDay;
		
		if (sscanf(kvArgs[1], "%d-%d-%d", &kvYear, &kvMonth, &kvDay) == 3) {
			tmDate1.tm_year = kvYear - 1900;
			tmDate1.tm_mon = kvMonth - 1;
			tmDate1.tm_mday = kvDay;
		}

		time_t kvDate1 = mktime(&tmDate1);
		
		
		if (sscanf(kvArgs[2], "%d-%d-%d", &kvYear, &kvMonth, &kvDay) == 3) {
			tmDate2.tm_year = kvYear - 1900;
			tmDate2.tm_mon = kvMonth - 1;
			tmDate2.tm_mday = kvDay;
		}

		time_t kvDate2 = mktime(&tmDate2);
		
			// Verify Date1 is smaller thatn Date2
		if (kvDate1 > kvDate2) {
			printf("Date 1 has to be smaller Date 2!\n");
			return 1;
		}
    }


	
		// Skip unzip for filesrch and quit command
	if(( strcmp(kvArgs[0], "filesrch") == 0 ||
			strcmp(kvArgs[0], "quit") == 0 ) && 
			kvUnzipFlag == 1){
		kvUnzipFlag = 0;
		kvQuitFlag = 0;
		printf("<-u> not allowed with with 'filesrch' or 'quit'.\n");
		return 1;
	}
	
		// Flag quit
	if (strcmp(kvArgs[0], "quit") == 0) {
		kvQuitFlag = 1;
	}

return 0;
	
}


/**
 * Function to set the flag to unzipi directory
 * */
void kvFlagUnzip(char kvBuff[]) {

	 int kvLength = strlen(kvBuff);
    const char *unzipFlag = " -u";

    if (kvLength >= 3 && strcmp(kvBuff + kvLength - 3, unzipFlag) == 0) {
        kvUnzipFlag = 1;
        kvBuff[kvLength - 3] = '\0';
    }
}

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


	/**
	 * Check if the passed year is leap year or not
	 * */
int kvIsLeapYear(int kvYear) {
    return (kvYear % 4 == 0 && kvYear % 100 != 0) || (kvYear % 400 == 0);
}

/**
 * Thsi funciton will verify passed date is in valid format or not
 * */
int kvVerifyDate(char date[]) {
		//date variables
    int kvYear, kvMonth, kvDay;

    // Year monath and Day extracted from input string 
    if (sscanf(date, "%d-%d-%d", &kvYear, &kvMonth, &kvDay) != 3) {
        // Exception in parsing
        return 0;
    }

    	// Verify year is in range 1000 and 9999
    if (kvYear < 1000 || kvYear > 9999) {
        return 0;
    }

    	// Verify monath range
    if (kvMonth < 1 || kvMonth > 12) {
        return 0;
    }

    	// Vefiry Day range
    if (kvDay < 1 || kvDay > 31) {
        return 0;
    }

    	// Verify months with 30 dyys
    if (kvMonth == 4 || kvMonth == 6 || kvMonth == 9 || kvMonth == 11) {
        if (kvDay > 30) {
            return 0;
        }
    } 
    	//verify for February
    else if (kvMonth == 2) {

    		//check leap year
        if (kvIsLeapYear(kvYear)) {
            if (kvDay > 29) {
                return 0;
            }
        } else {
            if (kvDay > 28) {
                return 0;
            }
        }
    }

    // Date is valid all checks passed.
    return 1;
}

int kvIsInteger(char* kvStr) 
	//iterrate givne input and validate wheterh givne input is interger value or not
{
    for (int i = 0; kvStr[i] != '\0'; i++) {

        if (!isdigit(kvStr[i])) {
            return 0;  // Non interger input
        }
    }
    return 1;  // givne input is interger
}

