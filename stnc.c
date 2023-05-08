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
#include <poll.h>
#include <stdbool.h>

#define SIZE 2048 // Size of a chunk.
#define BUFFER_SIZE 104857600 // 100 MB.
#define MAX_CONNECTIONS 1 // Allowing only one connection.
#define NUM_OF_FD 2 // Number of file descriptors we monitor in poll().
#define CHECKSUM_SIZE 16 // The size of the checksum header.

// Methods.
void handle_client (int, char*[]);
void handle_server (int, char*[]);
void handle_client_performance (int, char*[], bool[], bool[], char*);
void handle_server_performance (int, char*[], bool);
void md5_checksum (unsigned char*, int, unsigned char*);
unsigned char* generateData (int size);
bool check_checksums (unsigned char*, unsigned char*);
void print_server_usage ();
void print_client_usage ();



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
        if (!strcmp(argv[i], "udp")) { params[0] = true; }
        if (!strcmp(argv[i], "tcp")) { params[1] = true; }
        if (!strcmp(argv[i], "dgram")) { params[2] = true; }
        if (!strcmp(argv[i], "stream")) { params[3] = true; }
    }
    if( (types[2] || types[3])) {
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
        else { handle_client(argc, argv);}
    }
    else if (is_server) {
        if (p_flag) { handle_server_performance(argc, argv, q_flag); }
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
            int sbytes = send(socketFD, buffer, strlen(buffer), 0);
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
                int sbytes = send(clientSocket, buffer, strlen(buffer), 0);
                if (sbytes == -1) {
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
void handle_client_performance (int argc, char* argv[], bool types[], bool params[], char* filename) {
    // Not the right format.
    if(argc < 4){
        print_client_usage();
        exit(EXIT_FAILURE);
    }

    // Generate 100MB of data.
    unsigned char* buffer = generateData(BUFFER_SIZE);

    // Initialize variables for server.
    struct sockaddr_in serverAddress;

    // Initialize port.
    int port = atoi(argv[3]);

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
    }
    else {
        printf("(=) UDP Socket created successfully.\n");
    }
    // Sending the <type> and <param>.
    int sbytes1 = sendto(socketFD, argv[5], strlen(argv[5]), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    int sbytes2 = sendto(socketFD, argv[6], strlen(argv[6]), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress));
    if (sbytes1 || sbytes2 == -1) {
        printf("(-) sendto() failed with error code: %d\n", errno);
        exit(EXIT_FAILURE);
    }
    
    
}

//---------------------------------- Server Side - Performance Mode ---------------------------------
void handle_server_performance (int argc, char* argv[], bool q_flag) {
// Not the right format.
    if (argc < 3) {
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

    result = (unsigned char*)malloc((size + CHECKSUM_SIZE) * sizeof(char));

    // iI malloc failed return NULL.
    if(!result) {
        return NULL;
    }

    // Randomaize data.
    srand(time(NULL));
    for (int i = 0; i < size; i++) {
        result[i] = ((char)rand() % 256);
    }

    // Call checksum.
    md5_checksum(result, size, result + size);
    
    // Returning the pointer to the data.
    return result;
}

//---------------------------------- Compare Checksums ---------------------------------
bool check_checksums(unsigned char* checksum1, unsigned char* checksum2) {
    for (int i = 0; i < CHECKSUM_SIZE; i++) {
        if (checksum1[i] != checksum2[i]) {
            return false;
        }
    }
    return true;
}

//---------------------------------- Print Usages ---------------------------------------
void print_server_usage() {
    printf("(-) The usage is \"stnc -s port -p (p for performance test) -q (q for quiet)\"\n");
}
void print_client_usage() {
    printf("(-) The usage is \"stnc -c IP PORT -p (p for performance test) <type> <param>\"\n");
}



