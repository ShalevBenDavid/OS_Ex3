#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <openssl/md5.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <poll.h>
#include <stdbool.h>

#define SIZE 2048 // Size of a chunk.
#define BUFFER_SIZE 104857600 // 100 MB.
#define MAX_CONNECTIONS 1 // Allowing only one connection.
#define NUM_OF_FD 2 // Number of file descriptors we monitor in poll().
#define PARAM_LEN 6 // Max parameter length.
#define TYPE_LEN 4 // Max type length.

// Methods.
void handle_client (int, char*[]);
void handle_server (int, char*[]);
void handle_client_performance (int, char*[], const bool[], const bool[], char*);
void handle_server_performance (int, char*[], bool);
void md5_checksum (unsigned char*, int, unsigned char*);
unsigned char* generateData (int size);
void check_checksums (const unsigned char*, const unsigned char*, bool);
void print_server_usage ();
void print_client_usage ();
int send_data (unsigned char[], int);
void recv_data (int, char*, char*, unsigned char*, bool);

int main(int argc, char* argv[]){
    // Define flags to indicate execute usage.
    bool p_flag = false;
    bool q_flag = false;
    bool types[5] = {false};
    bool params[4] = {false};
    bool is_client = !strcmp(argv[1], "-c");
    bool is_server = !strcmp(argv[1], "-s");
    char* filename;

    for (int i = 0; i < argc; i++) {
        if (!strcmp(argv[i], "-p")) { p_flag = true; }
        if (!strcmp(argv[i], "-q")) { q_flag = true; }
        if (!strcmp(argv[i], "ipv4")) { types[0] = true; }
        if (!strcmp(argv[i], "ipv6")) { types[1] = true; }
        if (!strcmp(argv[i], "mmap")) { types[2] = true; }
        if (!strcmp(argv[i], "pipe")) { types[3] = true; }
        if (!strcmp(argv[i], "uds")) { types[4] = true; }
        if (!strcmp(argv[i], "tcp")) { params[0] = true; }
        if (!strcmp(argv[i], "udp")) { params[1] = true; }
        if (!strcmp(argv[i], "dgram")) { params[2] = true; }
        if (!strcmp(argv[i], "stream")) { params[3] = true; }
    }
    if ((types[2] || types[3])) {
        if (argc != 7 ) {
            print_server_usage();
            print_client_usage();
            exit(EXIT_FAILURE);
        } else {
            filename = argv[6];
        } 
    }
    if (is_client) {
        if (p_flag) { handle_client_performance(argc, argv, types, params, filename); }
        else { handle_client(argc, argv); }
    }
    else if (is_server) {
        if (p_flag) { handle_server_performance(argc, argv, q_flag); }
        else { handle_server(argc, argv); }
    }
    else {
        printf("(-) Not the right format.\n");
        exit(EXIT_FAILURE);
    }

    printf("(=) Exiting...\n");
    return EXIT_SUCCESS;
}

//---------------------------------- Client Side ---------------------------------
void handle_client (int argc, char* argv[]) {
    // Not the right format.
    if(argc != 4){
        print_client_usage();
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
    struct pollfd *pfds = calloc(NUM_OF_FD, sizeof(struct pollfd));

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
            ssize_t sbytes = send(socketFD, buffer, strlen(buffer), 0);
            if(sbytes == -1){
                printf("(-) send() failed with error code: %d\n", errno);
            }
            bzero(buffer,SIZE);
        }
        // <<<<<<<<<<<<<<<<<<<<<<<<< Read From Socket >>>>>>>>>>>>>>>>>>>>>>>>>
        else if (pfds[1].revents & POLLIN) {
            // Reset buffer.
            bzero(buffer, SIZE);
            // Receive message on the socket.
            ssize_t nbytes = recv(socketFD, buffer, SIZE, 0);
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
    if (argc != 3) {
        print_server_usage();
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
        struct pollfd *pfds = calloc(NUM_OF_FD, sizeof(struct pollfd));
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
                ssize_t sbytes = send(clientSocket, buffer, strlen(buffer), 0);
                if (sbytes == -1) {
                    printf("(-) send() failed with error code: %d\n", errno);
                }
            }
                // <<<<<<<<<<<<<<<<<<<<<<<<< Read From Socket >>>>>>>>>>>>>>>>>>>>>>>>>
            else if (pfds[1].revents & POLLIN) {
                // Reset buffer.
                bzero(buffer, SIZE);
                // Receive message on the socket.
                ssize_t nbytes = recv(clientSocket, buffer, SIZE, 0);
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
void handle_client_performance (int argc, char* argv[], const bool types[], const bool params[], char* filename) {
    // Not the right format.
    if(argc > 7){
        print_client_usage();
        exit(EXIT_FAILURE);
    }

    // Generate 100MB of data.
    unsigned char* buffer = generateData(BUFFER_SIZE);

    // Initialize variables for server.
    struct sockaddr_in serverAddress;

    // Initialize port.
    int port = atoi(argv[3]);
    char* address = argv[2];

    // Resting address.
    memset(&serverAddress, 0, sizeof(serverAddress));
    // Setting address to be IPv4.
    serverAddress.sin_family = AF_INET;
    // Setting the port.
    serverAddress.sin_port = htons(port);
    // Allow everyone to connect.
    serverAddress.sin_addr.s_addr = INADDR_ANY;
    

    // <<<<<<<<<<<<<<<<<<<<<<<<< Create UDP Connection To Transmit <type> And <param> >>>>>>>>>>>>>>>>>>>>>>>>>
    // Creates UDP socket.
    int socketFD = socket(AF_INET, SOCK_DGRAM, 0);

    // Check if we were successful in creating socket.
    if (socketFD == -1) {
        printf("(-) Could not create socket! -> socket() failed with error code: %d\n", errno);
        exit(EXIT_FAILURE); // Exit program and return EXIT_FAILURE (defined as 1 in stdlib.h).
    } else {
        printf("(=) UDP Socket created successfully.\n");
    }
    // Sending the <type> and <param>.
    ssize_t sbytes1 = sendto(socketFD, argv[5], strlen(argv[5]), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    ssize_t sbytes2 = sendto(socketFD, argv[6], strlen(argv[6]), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (sbytes1 == -1 || sbytes2 == -1) {
        printf("(-) sendto() failed with error code: %d\n", errno);
        exit(EXIT_FAILURE);
    }
    printf("(+) Sent connection type (%s, %s) successfully.\n", argv[5], argv[6]);

    // Close socket.
    close(socketFD);

    // <<<<<<<<<<<<<<<<<<<<<<<<< Handling All Combinations >>>>>>>>>>>>>>>>>>>>>>>>>
    sleep(1);
    // TCP
    if (params[0]) {
        // IPv4
        if (types[0]) {
            //-------------------------------Create TCP Connection-----------------------------
            int sock = socket(AF_INET, SOCK_STREAM, 0);

            // Check if we were successful in creating socket.
            if (sock == -1) {
                printf("(-) Could not create socket! -> socket() failed with error code: %d\n", errno);
                exit(EXIT_FAILURE);
            } else {
                printf("(=) Socket created successfully.\n");
            }

            // Initialize variables for server.
            struct sockaddr_in serverAddress4;
            // Clean the server address we already created earlier.
            memset(&serverAddress4, '\0', sizeof(serverAddress4));

            // Assign port and address to "serverAddress".
            serverAddress4.sin_family = AF_INET;
            serverAddress4.sin_port = htons(port); // Short, network byte order.

            // Convert address to binary.
            if (inet_pton(AF_INET, address, &serverAddress4.sin_addr) <= 0) {
                printf("(-) Failed to convert IPv4 address to binary! -> inet_pton() failed with error code: %d\n",
                       errno);
                exit(EXIT_FAILURE);
            }

            //Create connection with server.
            int connection = connect(sock, (struct sockaddr *) &serverAddress4, sizeof(serverAddress4));

            // Check if we were successful in connecting with server.
            if (connection == -1) {
                printf("(-) Could not connect to server! -> connect() failed with error code: %d\n", errno);
                exit(EXIT_FAILURE); // Exit program and return// EXIT_FAILURE (defined as 1 in stdlib.h).
            } else {
                printf("(=) Connection with server established.\n\n");
            }

            // Sending the data.
            if (send_data(buffer, sock) == -1) {
                printf("(-) Failed to send data! -> send() failed with error code: %d\n", errno);
            } else {
                printf("(+) Sent the data successfully.\n");
            }
            //-------------------------------Close Connection-----------------------------
            if (close(sock) == -1) {
                printf("(-) Failed to close connection! -> close() failed with error code: %d\n", errno);
            } else {
                printf("(=) Connection closed!\n");
            }
        }
        // IPv6
        if (types[1]) {
            //-------------------------------Create TCP Connection-----------------------------
            int sock = socket(AF_INET6, SOCK_STREAM, 0);

            // Check if we were successful in creating socket.
            if (sock == -1) {
                printf("(-) Could not create socket! -> socket() failed with error code: %d\n", errno);
                exit(EXIT_FAILURE);
            } else {
                printf("(=) Socket created successfully.\n");
            }

            // Initialize variables for server.
            struct sockaddr_in6 serverAddress6;
            // Clean the server address we already created earlier.
            memset(&serverAddress6, '\0', sizeof(serverAddress6));

            // Assign port and address to "serverAddress".
            serverAddress6.sin6_family = AF_INET6 ;
            serverAddress6.sin6_port = htons(port); // Short, network byte order.

            // Convert address to binary.
            if (inet_pton(AF_INET6, address, &serverAddress6.sin6_addr) <= 0) {
                printf("(-) Failed to convert IPv6 address to binary! -> inet_pton() failed with error code: %d\n",
                       errno);
                exit(EXIT_FAILURE);
            }

            //Create connection with server.
            int connection = connect(sock, (struct sockaddr *) &serverAddress6, sizeof(serverAddress6));

            // Check if we were successful in connecting with server.
            if (connection == -1) {
                printf("(-) Could not connect to server! -> connect() failed with error code: %d\n", errno);
                exit(EXIT_FAILURE); // Exit program and return// EXIT_FAILURE (defined as 1 in stdlib.h).
            } else {
                printf("(=) Connection with server established.\n\n");
            }

            // Sending the data.
            if (send_data(buffer, sock) == -1) {
                printf("(-) Failed to send data! -> send() failed with error code: %d\n", errno);
            } else {
                printf("(+) Sent the data successfully.\n");
            }
            //-------------------------------Close Connection-----------------------------
            if (close(sock) == -1) {
                printf("(-) Failed to close connection! -> close() failed with error code: %d\n", errno);
            } else {
                printf("(=) Connection closed!\n");
            }
        }
    }
}

//---------------------------------- Server Side - Performance Mode ---------------------------------
void handle_server_performance (int argc, char* argv[], bool q_flag) {
    // Not the right format.
    if (argc > 5) {
        print_server_usage();
        exit(EXIT_FAILURE);
    }

    // Setting buffers to hold <type> and <param> from client. Plus 1 for both for /0.
    char type[TYPE_LEN + 1] = {0};
    char param[PARAM_LEN + 1] = {0};

    // Initialize variables for server.
    struct sockaddr_in serverAddress;
    unsigned char* buffer = (unsigned char*) calloc ((BUFFER_SIZE + MD5_DIGEST_LENGTH), sizeof(char));
    if (!buffer) {
        if (!q_flag) { printf("(-) Memory allocation failed!\n"); }
        exit(EXIT_FAILURE);
    }
    unsigned char checksum[MD5_DIGEST_LENGTH] = {0};

    // Initialize port.
    int port = atoi(argv[2]);

    // Resting address.
    memset(&serverAddress, 0, sizeof(serverAddress));
    // Setting address to be IPv4.
    serverAddress.sin_family = AF_INET;
    // Setting the port.
    serverAddress.sin_port = htons(port);
    // Allow everyone to connect.
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    // Creates UDP socket.
    int socketFD = socket(AF_INET, SOCK_DGRAM, 0);
    // Check if we were successful in creating socket.
    if (socketFD == -1) {
        if (!q_flag) { printf("(-) Could not create socket! -> socket() failed with error code: %d\n", errno); }
        exit(EXIT_FAILURE); // Exit program and return EXIT_FAILURE (defined as 1 in stdlib.h).
    } else {
        if (!q_flag) { printf("(=) UDP Socket created successfully.\n"); }
    }

    int reuse = 1;
    if (setsockopt(socketFD, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0) {
        if (!q_flag) { perror("(-) Failed to set SO_REUSEADDR option.\n"); }
        exit(EXIT_FAILURE);
    }

    // Binding port and address to socket and check if binding was successful.
    if (bind(socketFD, (struct sockaddr *) &serverAddress, sizeof(serverAddress)) == -1) {
        if (!q_flag) { printf("(-) Failed to bind address && port to socket! -> bind() failed with error code: %d\n", errno); }
        close(socketFD);
        exit(EXIT_FAILURE);
    } else {
        if (!q_flag) { printf("(=) UDP Binding was successful\n"); }
    }

    // Receiving <type> and <param> from client.
    ssize_t rbytes1 = recvfrom(socketFD, type, TYPE_LEN, 0, NULL, NULL);
    ssize_t rbytes2 = recvfrom(socketFD, param, PARAM_LEN, 0, NULL, NULL);
    if (rbytes1 == -1 || rbytes2 == -1) {
        if (!q_flag) { printf("(-) recv() failed with error code: %d\n", errno); }
    }

    // Close socketFD.
    close(socketFD);

    // <<<<<<<<<<<<<<<<<<<<<<<<< Handling All Combinations >>>>>>>>>>>>>>>>>>>>>>>>>
    //  TCP
    if(!strcmp(param, "tcp")) {
        // IPv4
        if (!strcmp(type, "ipv4")) {
            // Creates TCP socket.
            int sock = socket(AF_INET, SOCK_STREAM, 0);

            // Check if address is already in use.
            int enableReuse = 1;
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(enableReuse)) == -1) {
                if (!q_flag) { printf("(-) setsockopt() failed with error code: %d\n", errno); }
                exit(EXIT_FAILURE);
            }

            // Initialize variables for server.
            struct sockaddr_in serverAddress4, clientAddress4;

            // Resting address.
            memset(&serverAddress4, 0, sizeof(serverAddress));
            // Setting address to be IPv4.
            serverAddress4.sin_family = AF_INET;
            // Setting the port.
            serverAddress4.sin_port = htons(port);
            // Allow everyone to connect.
            serverAddress4.sin_addr.s_addr = INADDR_ANY;

            // Binding port and address to socket and check if binding was successful.
            if (bind(sock, (struct sockaddr *) &serverAddress4, sizeof(serverAddress4)) == -1) {
                if (!q_flag) { printf("(-) Failed to bind address && port to socket! -> bind() failed with error code: %d\n", errno); }
                close(sock);
                exit(EXIT_FAILURE);
            } else {
                if (!q_flag) { printf("(=) Binding was successful!\n"); }
            }

            // Make server start listening and waiting, and check if listen() was successful.
            if (listen(sock, MAX_CONNECTIONS) == -1) { // We allow no more than one queue connections requests.
                if (!q_flag) { printf("Failed to start listening! -> listen() failed with error code : %d\n", errno); }
                close(sock);
                exit(EXIT_FAILURE);
            }
            while (true) {
                if (!q_flag) { printf("(=) Waiting for incoming TCP IPv4-connections...\n"); }

                // Create sockaddr_in for IPv4 for holding ip address and port of client and cleans it.
                memset(&clientAddress4, 0, sizeof(clientAddress4));
                unsigned int clientAddressLen = sizeof(clientAddress4);
                // Accept connection.
                int clientSocket = accept(sock, (struct sockaddr *) &clientAddress4, &clientAddressLen);
                if (clientSocket == -1) {
                    if (!q_flag) { printf("(-) Failed to accept connection. -> accept() failed with error code: %d\n", errno); }
                    close(sock);
                    close(clientSocket);
                    exit(EXIT_FAILURE);
                } else {
                    if (!q_flag) { printf("(=) Connection established.\n\n"); }
                }

                // Receive data from the user.
                recv_data(clientSocket, type, param, buffer, q_flag);
                // Do a checksum in server side.
                md5_checksum(buffer, BUFFER_SIZE, checksum);
                // Compare the checksums.
                check_checksums(checksum, buffer + BUFFER_SIZE, q_flag);
            }
        }
        // IPv6
        if (!strcmp(type, "ipv6")) {
            // Creates TCP socket.
            int sock = socket(AF_INET6, SOCK_STREAM, 0);

            // Check if address is already in use.
            int enableReuse = 1;
            if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(enableReuse)) == -1) {
                if (!q_flag) { printf("(-) setsockopt() failed with error code: %d\n", errno); }
                exit(EXIT_FAILURE);
            }

            // Initialize variables for server.
            struct sockaddr_storage serverAddress6, clientAddress6;
            // Clean the server address we already created earlier.
            memset(&serverAddress6, '\0', sizeof(serverAddress6));

            // Assign port and address to "serverAddress6".
            ((struct sockaddr_in6 *) &serverAddress6) -> sin6_family = AF_INET6;
            ((struct sockaddr_in6*) &serverAddress6) -> sin6_port = htons(port);
            ((struct sockaddr_in6*) &serverAddress6) -> sin6_addr = in6addr_any;

            // Binding port and address to socket and check if binding was successful.
            if (bind(sock, (struct sockaddr *) &serverAddress6, sizeof(serverAddress6)) == -1) {
                if (!q_flag) { printf("(-) Failed to bind address && port to socket! -> bind() failed with error code: %d\n", errno); }
                close(sock);
                exit(EXIT_FAILURE);
            } else {
                if (!q_flag) { printf("(=) Binding was successful!\n"); }
            }

            // Make server start listening and waiting, and check if listen() was successful.
            if (listen(sock, MAX_CONNECTIONS) == -1) { // We allow no more than one queue connections requests.
                if (!q_flag) { printf("Failed to start listening! -> listen() failed with error code : %d\n", errno); }
                close(sock);
                exit(EXIT_FAILURE);
            }
            while (true) {
                if (!q_flag) { printf("(=) Waiting for incoming TCP IPv4-connections...\n"); }

                // Create sockaddr_in for IPv4 for holding ip address and port of client and cleans it.
                memset(&clientAddress6, 0, sizeof(clientAddress6));
                unsigned int clientAddressLen = sizeof(clientAddress6);
                // Accept connection.
                int clientSocket = accept(sock, (struct sockaddr *) &clientAddress6, &clientAddressLen);
                if (clientSocket == -1) {
                    if (!q_flag) { printf("(-) Failed to accept connection. -> accept() failed with error code: %d\n", errno); }
                    close(sock);
                    close(clientSocket);
                    exit(EXIT_FAILURE);
                } else {
                    if (!q_flag) { printf("(=) Connection established.\n\n"); }
                }

                // Receive data from the user.
                recv_data(clientSocket, type, param, buffer, q_flag);
                // Do a checksum in server side.
                md5_checksum(buffer, BUFFER_SIZE, checksum);
                // Compare the checksums.
                check_checksums(checksum, buffer + BUFFER_SIZE, q_flag);
            }
        }
    }
}

//---------------------------------- Checksum Function ---------------------------------
void md5_checksum (unsigned char* data, int size, unsigned char* checksum) {
   // Initializing MD5 struct. 
   MD5_CTX md5;

   MD5_Init(&md5);
   MD5_Update(&md5, data, size);
   MD5_Final(checksum, &md5);
}

//---------------------------------- Generate Data And Add Checksum ---------------------------------
unsigned char* generateData (int size) {
    unsigned char* result = NULL;

    // User wants to generate nothing. 
    if(size == 0) {
        return NULL;
    }

    result = (unsigned char*) calloc((size + MD5_DIGEST_LENGTH), sizeof(char));
    // iI calloc failed return NULL.
    if (!result) {
        return NULL;
    }

    // Randomize data.
    srand(time(NULL));
    for (int i = 0; i < size; i++) {
        result[i] = ((char) rand() % 256);
    }

    // Call checksum.
    md5_checksum(result, size, result + size);
    
    // Returning the pointer to the data.
    return result;
}

//---------------------------------- Compare Checksums ---------------------------------
void check_checksums(const unsigned char* checksum1, const unsigned char* checksum2, bool q_flag) {
    for (int i = 0; i < MD5_DIGEST_LENGTH; i++) {
        if (checksum1[i] != checksum2[i]) {
            if (!q_flag) { printf("(-) Error: Some data has been lost!\n"); }
            return;
        }
    }
    if (!q_flag) { printf("(+) All data was successfully received.\n"); }
}

//---------------------------------- Print Usages ---------------------------------------
void print_server_usage() {
    printf("(-) The usage is \"stnc -s port -p (p for performance test) -q (q for quiet)\"\n");
}
void print_client_usage() {
    printf("(-) The usage is \"stnc -c IP PORT -p (p for performance test) <type> <param>\"\n");
}

//---------------------------------- Sending Data-----------------------------------------
int send_data(unsigned char data[], int socketFD) {
    size_t totalLengthSent = 0; // Variable for keeping track of number of bytes sent.
    while (totalLengthSent < BUFFER_SIZE + MD5_DIGEST_LENGTH) {
        ssize_t bytes = send(socketFD, data + totalLengthSent,
                             BUFFER_SIZE + MD5_DIGEST_LENGTH - totalLengthSent, 0);
        if (bytes == -1) {
            return -1;
        }
        totalLengthSent += bytes;
    }
    return 1;
}

//---------------------------------- Receiving Data-----------------------------------------
void recv_data (int clientSocket, char* type, char* param, unsigned char* buffer, bool q_flag) {
    // Initialize variables.
    size_t receivedTotalBytes = 0; // Variable for keeping track of number of received bytes.
    ssize_t receivedBytes = 0;
    struct timeval tv;
    long long start, end;

    // Start measuring time.
    gettimeofday(&tv, NULL);
    start = tv.tv_sec * 1000LL + tv.tv_usec / 1000;

    // While there is still data to receive.
    while (receivedTotalBytes < BUFFER_SIZE + MD5_DIGEST_LENGTH ) {
        receivedBytes = recv(clientSocket, buffer + receivedTotalBytes,
                             BUFFER_SIZE + MD5_DIGEST_LENGTH - receivedTotalBytes, 0);
        if (receivedBytes <= 0) { // Break if we got an error (-1) or peer closed half side of the socket (0).
            if (!q_flag) { printf("(-) Error in receiving data or peer closed half side of the socket."); }
            break;
        }
        receivedTotalBytes += receivedBytes; // Add the new received bytes to the total bytes received.
    }
    // Stop measuring time.
    gettimeofday(&tv, NULL);
    end = tv.tv_sec * 1000LL + tv.tv_usec / 1000;

    // Calculate time in milliseconds.
    long long duration = end - start;

    // Print stats.
    printf("%s_%s,%lld \n",type, param, duration);
}

    


