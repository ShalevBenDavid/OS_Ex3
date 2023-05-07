#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
//#include <openssl/md5.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <poll.h>
#include <stdbool.h>

#define SIZE 2048 // Size of a chunk.
#define BUFFER_SIZE 104857600 // 100 MB.
#define MAX_CONNECTIONS 1 // Allowing only one connection.
#define NUM_OF_FD 2 // Number of file descriptors we monitor in poll().

// Methods.
void handle_client (int, char*[]);
void handle_server (int, char*[]);
void handle_client_performance (int, char*[]);
void handle_server_performance (int, char*[]);
void md5_checksum (unsigned char*, int, unsigned char*);
void generate100MB (char*);

int main(int argc, char* argv[]){
    // Define flags to indicate execute usage.
    bool p_flag = false;
    bool q_flag = false;
    bool ipv4_flag = false;
    bool ipv6_flag = false;
    bool tcp_flag = false;
    bool udp_flag = false;
    bool is_client = !strcmp(argv[1], "-c");
    bool is_server = !strcmp(argv[1], "-s");
    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) { p_flag = true; }
        if (!strcmp(argv[i], "-q")) { q_flag = true; }
        if (!strcmp(argv[i], "ipv4")) {ipv4_flag = true; }
        if (!strcmp(argv[i], "ipv6")) {ipv6_flag = true; }
        if (!strcmp(argv[i], "tcp")) {tcp_flag = true; }
        if (!strcmp(argv[i], "udp")) {udp_flag = true; }
    }
    if (is_client) {
        if (p_flag) { handle_client_performance(argc, argv); }
        else { handle_client(argc, argv);}
    }
    else if (is_server) {
        if (p_flag) { handle_server_performance(argc, argv); }
        else { handle_server(argc, argv); }
    }
    else {
        printf("(-) Not the right format.\n");
        exit(EXIT_FAILURE);
    }
}

//---------------------------------- Client Side ---------------------------------
void handle_client (int argc, char* argv[]) {
    // Not the right format.
    if(argc < 4){
        printf("(-) The usage is \"stnc -c IP PORT -p (p for performance test) <type> <param>\"\n");
        exit(EXIT_FAILURE);
    }

    // Initialize variables for client or server appropriately.
    char buffer[SIZE + 1] = {0}; // size + 1 for the \0.
    struct sockaddr_in serverAddress;

    // Initialize port and address.
    int port = atoi(argv[3]);
    char* address = argv[2];

    // Resting address.
    memset(&serverAddress, 0, sizeof(serverAddress));
    // Setting address to be IPv4.
    serverAddress.sin_family = AF_INET;
    // Setting the port.
    serverAddress.sin_port = htons(port);
    // Setting the IP address,
    serverAddress.sin_addr.s_addr = inet_addr(address);
    // If converting address files, exit.
    if (inet_pton(AF_INET, address, &serverAddress.sin_addr) == 0) {
        printf("(-) Failed to convert IPv4 address to binary! -> inet_pton() failed with error code: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    // <<<<<<<<<<<<<<<<<<<<<<<<< Create TCP Connection >>>>>>>>>>>>>>>>>>>>>>>>>
    // Creates TCP socket.
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);

    // Check if we were successful in creating socket.
    if (socketFD == -1) {
        printf("(-) Could not create socket! -> socket() failed with error code: %d\n", errno);
        exit(EXIT_FAILURE); // Exit program and return EXIT_FAILURE (defined as 1 in stdlib.h).
    }
    else {
        printf("(=) Socket created successfully.\n");
    }

    // Create connection with server.
    int connection = connect(socketFD, (struct sockaddr*) &serverAddress, sizeof(serverAddress));

    // Check if we were successful in connecting with server.
    if (connection == -1) {
        printf("(-) Could not connect to server! -> connect() failed with error code: %d\n", errno);
        close(socketFD);
        exit(EXIT_FAILURE);
    }
    else {
        printf("(=) Connection with server established.\n\n");
    }

    // Create pollfd to check which fd have current events.
    struct pollfd *pfds = malloc(sizeof *pfds * NUM_OF_FD);
    if (!pfds) {
        printf("(-) Failed to allocate memory for poll. %d\n" , errno);
        exit(EXIT_FAILURE);
    }

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    pfds[1].fd = socketFD;
    pfds[1].events = POLLIN;

    while (true) {
        // Call poll and see how events occurred.
        int poll_count = poll(pfds, NUM_OF_FD, -1);
        if (poll_count == -1) {
            printf("(-) poll() failed with error code: %d\n", errno);
            exit(EXIT_FAILURE);
        }

        // <<<<<<<<<<<<<<<<<<<<<<<<< Send To Socket >>>>>>>>>>>>>>>>>>>>>>>>>
        if (pfds[0].revents & POLLIN) {
            // Reset buffer.
            bzero(buffer, SIZE);
            // Get message from standard input.
            fgets(buffer, SIZE + 1, stdin);
            // Send message to the client.
            int sbytes = send(socketFD, buffer, strlen(buffer), 0);
            if(sbytes < 0){
                printf("(-) send() failed with error code: %d\n", errno);
            }
            bzero(buffer,SIZE);
        }
        // <<<<<<<<<<<<<<<<<<<<<<<<< Read From Socket >>>>>>>>>>>>>>>>>>>>>>>>>
        else if (pfds[1].revents & POLLIN) {
            // Reset buffer.
            bzero(buffer, SIZE);
            // Receive message on the socket.
            int nbytes = recv(socketFD, buffer, SIZE, 0);
            if (nbytes == 0) {
                printf("(-) Server hung up.\n");
            } else if (nbytes == -1) {
                printf("(-) recv() failed with error code: %d\n", errno);
            }
            // Print client message.
            printf("Message: %s", buffer);
        }
    }
}

//---------------------------------- Server Side ---------------------------------
void handle_server (int argc, char* argv[]) {
// Not the right format.
    if (argc < 3) {
        printf("(-) The usage is \"stnc -s port -p (p for performance test) -q (q for quiet)\"\n");
        exit(EXIT_FAILURE);
    }

    // Creates TCP socket.
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);

    // Check if address is already in use.
    int enableReuse = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(enableReuse)) == -1) {
        printf("(-) setsockopt() failed with error code: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    // Initialize variables.
    char buffer[SIZE + 1] = {0}; // size + 1 for the \0.
    struct sockaddr_in serverAddress;
    memset(&serverAddress, '\0', sizeof(serverAddress));

    // Initialize port.
    int port = atoi(argv[2]);

    // Assign port and address to "clientAddress".
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port); // Short, network byte order.
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Binding port and address to socket and check if binding was successful.
    if (bind(socketFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) {
        printf("(-) Failed to bind address && port to socket! -> bind() failed with error code: %d\n", errno);
        close(socketFD);
        exit(EXIT_FAILURE);
    } else {
        printf("(=) Binding was successful!\n");
    }

    // Make server start listening and waiting, and check if listen() was successful.
    if (listen(socketFD, MAX_CONNECTIONS) == -1) { // We allow no more than one queue connections requests.
        printf("Failed to start listening! -> listen() failed with error code : %d\n", errno);
        close(socketFD);
        exit(EXIT_FAILURE);
    }
    while (true) {
        printf("(=) Waiting for incoming TCP-connections...\n");

        // Create sockaddr_in for IPv4 for holding ip address and port of client and cleans it.
        struct sockaddr_in clientAddress;
        memset(&clientAddress, 0, sizeof(clientAddress));
        unsigned int clientAddressLen = sizeof(clientAddress);
        // Accept connection.
        int clientSocket = accept(socketFD, (struct sockaddr *) &clientAddress, &clientAddressLen);
        if (clientSocket == -1) {
            printf("(-) Failed to accept connection. -> accept() failed with error code: %d\n", errno);
            close(socketFD);
            close(clientSocket);
            exit(EXIT_FAILURE);
        } else {
            printf("(=) Connection established.\n\n");
        }

        // Create polled to check which fd have current events.
        struct pollfd *pfds = malloc(sizeof *pfds * NUM_OF_FD);
        if (!pfds) {
            printf("(-) Failed to allocate memory for poll. %d\n", errno);
            exit(EXIT_FAILURE);
        }

        pfds[0].fd = STDIN_FILENO; // Client socket.
        pfds[0].events = POLLIN;
        pfds[1].fd = clientSocket; // Client socket.
        pfds[1].events = POLLIN;

        while (true) {
            // Call poll and see how events occurred.
            int poll_count = poll(pfds, NUM_OF_FD, -1);
            if (poll_count == -1) {
                printf("(-) poll() failed with error code: %d\n", errno);
                exit(EXIT_FAILURE);
            }

            // <<<<<<<<<<<<<<<<<<<<<<<<< Send To Socket >>>>>>>>>>>>>>>>>>>>>>>>>
            if (pfds[0].revents & POLLIN) {
                // Reset buffer.
                bzero(buffer, SIZE);
                // Get message from standard input.
                fgets(buffer, SIZE + 1, stdin);
                // Send message to the client.
                int sbytes = send(clientSocket, buffer, strlen(buffer), 0);
                if (sbytes < 0) {
                    printf("(-) send() failed with error code: %d\n", errno);
                }
            }
                // <<<<<<<<<<<<<<<<<<<<<<<<< Read From Socket >>>>>>>>>>>>>>>>>>>>>>>>>
            else if (pfds[1].revents & POLLIN) {
                // Reset buffer.
                bzero(buffer, SIZE);
                // Receive message on the socket.
                int nbytes = recv(clientSocket, buffer, SIZE, 0);
                if (nbytes == 0) {
                    printf("(-) Client hung up\n");
                    break;
                } else if (nbytes == -1) {
                    printf("(-) recv() failed with error code: %d\n", errno);
                }
                // Print client message.
                printf("Message: %s", buffer);
            }
        }
    }
}

//---------------------------------- Client Side - Performance Mode ---------------------------------
void handle_client_performance (int argc, char* argv[]) {
    // Not the right format.
    if(argc < 4){
        printf("(-) The usage is \"stnc -c IP PORT -p (p for performance test) <type> <param>\"\n");
        exit(EXIT_FAILURE);
    }

    // Initialize variables for client or server appropriately.
    char buffer[SIZE + 1] = {0}; // size + 1 for the \0.
    struct sockaddr_in serverAddress;

    // Initialize port and address.
    int port = atoi(argv[3]);
    char* address = argv[2];

    // Resting address.
    memset(&serverAddress, 0, sizeof(serverAddress));
    // Setting address to be IPv4.
    serverAddress.sin_family = AF_INET;
    // Setting the port.
    serverAddress.sin_port = htons(port);
    // Setting the IP address,
    serverAddress.sin_addr.s_addr = inet_addr(address);
    // If converting address files, exit.
    if (inet_pton(AF_INET, address, &serverAddress.sin_addr) == 0) {
        printf("(-) Failed to convert IPv4 address to binary! -> inet_pton() failed with error code: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    // <<<<<<<<<<<<<<<<<<<<<<<<< Create TCP Connection >>>>>>>>>>>>>>>>>>>>>>>>>
    // Creates TCP socket.
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);

    // Check if we were successful in creating socket.
    if (socketFD == -1) {
        printf("(-) Could not create socket! -> socket() failed with error code: %d\n", errno);
        exit(EXIT_FAILURE); // Exit program and return EXIT_FAILURE (defined as 1 in stdlib.h).
    }
    else {
        printf("(=) Socket created successfully.\n");
    }

    // Create connection with server.
    int connection = connect(socketFD, (struct sockaddr*) &serverAddress, sizeof(serverAddress));

    // Check if we were successful in connecting with server.
    if (connection == -1) {
        printf("(-) Could not connect to server! -> connect() failed with error code: %d\n", errno);
        close(socketFD);
        exit(EXIT_FAILURE);
    }
    else {
        printf("(=) Connection with server established.\n\n");
    }

    // Create pollfd to check which fd have current events.
    struct pollfd *pfds = malloc(sizeof *pfds * NUM_OF_FD);
    if (!pfds) {
        printf("(-) Failed to allocate memory for poll. %d\n" , errno);
        exit(EXIT_FAILURE);
    }

    pfds[0].fd = STDIN_FILENO;
    pfds[0].events = POLLIN;
    pfds[1].fd = socketFD;
    pfds[1].events = POLLIN;

    while (true) {
        // Call poll and see how events occurred.
        int poll_count = poll(pfds, NUM_OF_FD, -1);
        if (poll_count == -1) {
            printf("(-) poll() failed with error code: %d\n", errno);
            exit(EXIT_FAILURE);
        }

        // <<<<<<<<<<<<<<<<<<<<<<<<< Send To Socket >>>>>>>>>>>>>>>>>>>>>>>>>
        if (pfds[0].revents & POLLIN) {
            // Reset buffer.
            bzero(buffer, SIZE);
            // Get message from standard input.
            fgets(buffer, SIZE + 1, stdin);
            // Send message to the client.
            int sbytes = send(socketFD, buffer, strlen(buffer), 0);
            if(sbytes < 0){
                printf("(-) send() failed with error code: %d\n", errno);
            }
            bzero(buffer,SIZE);
        }
            // <<<<<<<<<<<<<<<<<<<<<<<<< Read From Socket >>>>>>>>>>>>>>>>>>>>>>>>>
        else if (pfds[1].revents & POLLIN) {
            // Reset buffer.
            bzero(buffer, SIZE);
            // Receive message on the socket.
            int nbytes = recv(socketFD, buffer, SIZE, 0);
            if (nbytes == 0) {
                printf("(-) Server hung up.\n");
            } else if (nbytes == -1) {
                printf("(-) recv() failed with error code: %d\n", errno);
            }
            // Print client message.
            printf("Message: %s", buffer);
        }
    }
}

//---------------------------------- Server Side - Performance Mode ---------------------------------
void handle_server_performance (int argc, char* argv[]) {
// Not the right format.
    if (argc < 3) {
        printf("(-) The usage is \"stnc -s port -p (p for performance test) -q (q for quiet)\"\n");
        exit(EXIT_FAILURE);
    }

    // Creates TCP socket.
    int socketFD = socket(AF_INET, SOCK_STREAM, 0);

    // Check if address is already in use.
    int enableReuse = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(enableReuse)) == -1) {
        printf("(-) setsockopt() failed with error code: %d\n", errno);
        exit(EXIT_FAILURE);
    }

    // Initialize variables.
    char buffer[SIZE + 1] = {0}; // size + 1 for the \0.
    struct sockaddr_in serverAddress;
    memset(&serverAddress, '\0', sizeof(serverAddress));

    // Initialize port.
    int port = atoi(argv[2]);

    // Assign port and address to "clientAddress".
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port); // Short, network byte order.
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Binding port and address to socket and check if binding was successful.
    if (bind(socketFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) {
        printf("(-) Failed to bind address && port to socket! -> bind() failed with error code: %d\n", errno);
        close(socketFD);
        exit(EXIT_FAILURE);
    } else {
        printf("(=) Binding was successful!\n");
    }

    // Make server start listening and waiting, and check if listen() was successful.
    if (listen(socketFD, MAX_CONNECTIONS) == -1) { // We allow no more than one queue connections requests.
        printf("Failed to start listening! -> listen() failed with error code : %d\n", errno);
        close(socketFD);
        exit(EXIT_FAILURE);
    }
    while (true) {
        printf("(=) Waiting for incoming TCP-connections...\n");

        // Create sockaddr_in for IPv4 for holding ip address and port of client and cleans it.
        struct sockaddr_in clientAddress;
        memset(&clientAddress, 0, sizeof(clientAddress));
        unsigned int clientAddressLen = sizeof(clientAddress);
        // Accept connection.
        int clientSocket = accept(socketFD, (struct sockaddr *) &clientAddress, &clientAddressLen);
        if (clientSocket == -1) {
            printf("(-) Failed to accept connection. -> accept() failed with error code: %d\n", errno);
            close(socketFD);
            close(clientSocket);
            exit(EXIT_FAILURE);
        } else {
            printf("(=) Connection established.\n\n");
        }

        // Create polled to check which fd have current events.
        struct pollfd *pfds = malloc(sizeof *pfds * NUM_OF_FD);
        if (!pfds) {
            printf("(-) Failed to allocate memory for poll. %d\n", errno);
            exit(EXIT_FAILURE);
        }

        pfds[0].fd = STDIN_FILENO; // Client socket.
        pfds[0].events = POLLIN;
        pfds[1].fd = clientSocket; // Client socket.
        pfds[1].events = POLLIN;

        while (true) {
            // Call poll and see how events occurred.
            int poll_count = poll(pfds, NUM_OF_FD, -1);
            if (poll_count == -1) {
                printf("(-) poll() failed with error code: %d\n", errno);
                exit(EXIT_FAILURE);
            }

            // <<<<<<<<<<<<<<<<<<<<<<<<< Send To Socket >>>>>>>>>>>>>>>>>>>>>>>>>
            if (pfds[0].revents & POLLIN) {
                // Reset buffer.
                bzero(buffer, SIZE);
                // Get message from standard input.
                fgets(buffer, SIZE + 1, stdin);
                // Send message to the client.
                int sbytes = send(clientSocket, buffer, strlen(buffer), 0);
                if (sbytes < 0) {
                    printf("(-) send() failed with error code: %d\n", errno);
                }
            }
                // <<<<<<<<<<<<<<<<<<<<<<<<< Read From Socket >>>>>>>>>>>>>>>>>>>>>>>>>
            else if (pfds[1].revents & POLLIN) {
                // Reset buffer.
                bzero(buffer, SIZE);
                // Receive message on the socket.
                int nbytes = recv(clientSocket, buffer, SIZE, 0);
                if (nbytes == 0) {
                    printf("(-) Client hung up\n");
                    break;
                } else if (nbytes == -1) {
                    printf("(-) recv() failed with error code: %d\n", errno);
                }
                // Print client message.
                printf("Message: %s", buffer);
            }
        }
    }
}

//---------------------------------- Checksum Function ---------------------------------
//void md5_checksum(unsigned char *data, int len, unsigned char *checksum) {
//    MD5_CTX md5;
//
//    MD5_Init(&md5);
//    MD5_Update(&md5, data, len);
//    MD5_Final(checksum, &md5);
//}

//---------------------------------- Checksum 2MB Of Data ---------------------------------
void generate100MB (char* buffer) {
    for (int i = 0; i < BUFFER_SIZE; i++) {
        buffer[i] = rand() % 256;
    }
}

