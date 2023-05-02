#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>

#define IP_ADDRESS "127.0.0.1"
#define SIZE 4098

size_t send_message(char* ,int);




size_t send_message(char* message ,int socketFD){
    size_t totalLengthSent = 0; // Variable for keeping track of number of bytes sent.
    while (totalLengthSent < SIZE) {
        ssize_t bytes = send(socketFD, message + totalLengthSent, SIZE - totalLengthSent, 0);
        if (bytes == -1) {
            return -1;
        }
        totalLengthSent += bytes;
    }
    return EXIT_SUCCESS;
}

int recv_message(int clientSocket, char* buffer) {
    size_t receivedTotalBytes = 0; // Variable for keeping track of number of received bytes.

    //----------------------------------Receive Half Loop---------------------------------
    while (receivedTotalBytes != SIZE) {
        bzero(buffer, SIZE + 1); // Clean buffer.
        ssize_t receivedBytes = recv(clientSocket, buffer, SIZE - receivedTotalBytes, 0);
        if (receivedBytes <= 0) { // Break if we got an error (-1) or peer closed half side of the socket (0).
            printf("(-) Error in receiving data or peer closed half side of the socket.");
            break;
        }
        receivedTotalBytes += receivedBytes; // Add the new received bytes to the total bytes received.
        // Check if we need to exit.
        if (strcmp(buffer, "exit") == 0) {
            return -1;
        }
    }
    return -1;
}



int main(int argc, char* argv[]){
    if(argc < 3){
        printf("(-) Not the right format.\n");
        exit(EXIT_FAILURE);
    }
    // Initialize variables for client or server appropriately.
    int c_flag, port;
    char message[SIZE + 1] = {0}; // size + 1 for the \0.;
    struct sockaddr_in serverAddress;


    // Checks if it's client/ server case.
    if(strcmp(argv[1],"-c") == 0){
        c_flag = 1;
        port = atoi(argv[3]);
        memset(&serverAddress, 0, sizeof(serverAddress));
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port);
        if (inet_pton(AF_INET, argv[2], &serverAddress.sin_addr) <= 0) {
            perror("Invalid server address");
            exit(EXIT_FAILURE);
        }
    }else{
        c_flag = 0;
        port = atoi(argv[2]);
    }
   

    //------------------------------- Create TCP Connection -----------------------------
    // Creates socket named "socketFD". FD for file descriptor.
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);
    memset(&serverAddress, '\0', sizeof(serverAddress));

    // Check if we were successful in creating socket.
    if(socketFD == -1) {
        printf("(-) Could not create socket! -> socket() failed with error code: %d\n", errno);
        exit(EXIT_FAILURE); // Exit program and return EXIT_FAILURE (defined as 1 in stdlib.h).
    }
    else {
        printf("(=) Socket created successfully.\n");
    }

    //------------------------------- CLIENT HANDLING ------------------------------------
    if(c_flag == true){
        printf("CLIENT\n");
        // Assign port and address to "serverAddress".
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port); // Short, network byte order.
        serverAddress.sin_addr.s_addr = inet_addr(IP_ADDRESS);

        // Convert address to binary.
        if (inet_pton(AF_INET, IP_ADDRESS, &serverAddress.sin_addr) <= 0)
        {
            printf("(-) Failed to convert IPv4 address to binary! -> inet_pton() failed with error code: %d\n", errno);
            exit(EXIT_FAILURE); // Exit program and return EXIT_FAILURE (defined as 1 in stdlib.h).
        }

        //Create connection with server.
        int connection = connect(socketFD, (struct sockaddr*) &serverAddress, sizeof(serverAddress));

        // Check if we were successful in connecting with server.
        if(connection == -1) {
            printf("(-) Could not connect to server! -> connect() failed with error code: %d\n", errno);
            exit(EXIT_FAILURE); // Exit program and return// EXIT_FAILURE (defined as 1 in stdlib.h).
        }
        else {
            printf("(=) Connection with server established.\n\n");
        }
        
        //------------------------------- SENDING & RECIEVING MESSAGE ------------------------------------
        char answer[256] = {0}; // size + 1 for the \0.;
        while(true){
            printf("Enter message to server: ");
            scanf("%s", message);
            if (send_message(message, socketFD) == -1) {
            printf("(-) Failed to send message! -> send() failed with error code: %d\n", errno);
            } else {
                printf("(+) Sent the message.\n");
            }

            if (recv(socketFD, answer, 256, 0) <= 0) { // Check if we got an error (-1) or peer closed half side of the socket (0).
            printf("(-) Error in receiving data or peer closed half side of the socket.");
            } else {
                printf("Server response: ");
                puts(answer);
            }
            

        }


    }
    //------------------------------- SERVER HANDLING ------------------------------------
    else{
        char buffer[SIZE + 1]; // Global array for holding the message. His size is SIZE + 1 for the \0.
        // Check if address is already in use.
        int enableReuse = 1;
        if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(enableReuse)) ==  -1)  {
            printf("setsockopt() failed with error code: %d\n" , errno);
            exit(EXIT_FAILURE); // Exit program and return EXIT_FAILURE (defined as 1 in stdlib.h).
        }
        // Assign port and address to "serverAddress".
        serverAddress.sin_family = AF_INET;
        serverAddress.sin_port = htons(port); // Short, network byte order.
        serverAddress.sin_addr.s_addr = INADDR_ANY;

        // Binding port and address to socket and check if binding was successful.
        if (bind(socketFD, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == -1) {
            printf("(-) Failed to bind address && port to socket! -> bind() failed with error code: %d\n", errno);
            close(socketFD); // close the socket.
            exit(EXIT_FAILURE); // Exit program and return EXIT_FAILURE (defined as 1 in stdlib.h).
        }
        else {
            printf("(=) Binding was successful!\n");
        }

        // Make server start listening and waiting, and check if listen() was successful.
        if (listen(socketFD, 1) == -1) { // We allow no more than one queue connections requests.
            printf("Failed to start listening! -> listen() failed with error code : %d\n", errno);
            close(socketFD); // close the socket.
            exit(EXIT_FAILURE); // Exit program and return EXIT_FAILURE (defined as 1 in stdlib.h).
        }
        printf("(=) Waiting for incoming TCP-connections...\n");

        // Create sockaddr_in for IPv4 for holding ip address and port of client and cleans it.
        struct sockaddr_in clientAddress;
        memset(&clientAddress, 0, sizeof(clientAddress));
        unsigned int clientAddressLen = sizeof(clientAddress);
        // Accept connection.
        int clientSocket = accept(socketFD, (struct sockaddr*)&clientAddress, &clientAddressLen); 
        if (clientSocket == -1) {
            printf("(-) Failed to accept connection. -> accept() failed with error code: %d\n", errno);
            close(socketFD);
            close(clientSocket);
            exit(EXIT_FAILURE); // Exit program and return EXIT_FAILURE (defined as 1 in stdlib.h).
        }
        else {
            printf("(=) Connection established.\n\n");
        }

        //----------------------------------Receive Messages---------------------------------
        if (recv_message(clientSocket, buffer) == -1) {
            return -1;
        }
        printf("client: ");
        puts(buffer);
        bzero(buffer, SIZE + 1); // Clean buffer.

        //----------------------------------SEND RESPONSE------------------------------------
        char answer[256] = "Got the message succesfully."; // Array for server response.
        if (send(clientSocket, answer, strlen(answer), 0) == -1) { // Send response to client.
            printf("(-) Failed to send answer! -> send() failed with error code: %d\n", errno);
        }
        else {
            printf("(+) Sent answer.\n");
        }

    }   
}