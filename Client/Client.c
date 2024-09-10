#include "headers.h"

pthread_mutex_t request_mutex = PTHREAD_MUTEX_INITIALIZER;

static int client_port = 20000;
int allocate_port()
{
    return client_port++;
}

void display()
{
    printf("Menu:\n");
    printf("1. Read File\n");
    printf("2. Write File\n");
    printf("3. Get Info about File\n");
    printf("4. Create File\n");
    printf("5. Delete File\n");
    printf("6. Copy File\n");
    printf("7. Create Directory\n");
    printf("8. Delete directory\n");
    printf("9. Copy Directory\n");
}

void get_file_path(char *file_path, int size)
{
    printf("Enter file path: ");
    fgets(file_path, size, stdin);
    file_path[strcspn(file_path, "\n")] = '\0';
}

void *client_thread(void *arg)
{
    int thread_id = *((int *)arg);
    int option;
    char file_path[256];
    char file_content[1024];
    char dir_path[256];
    char dir_content[1024];
    int ack;
    char response[1024];
    char request[2048];
    char ss_ip[50];
    int ss_port;
    int client_port = allocate_port();

    while (1)
    {
        display();
        printf("Select an option: ");
        scanf("%d", &option);
        getchar();
        char destination_path[256];

        switch (option)
        {
        case 1:
            get_file_path(file_path, sizeof(file_path));
            // snprintf(request, sizeof(request), "READ %d %s", client_port, file_path);
            // pthread_mutex_lock(&request_mutex);
            if (communicate_with_nm(READ, file_path, ss_ip, &ss_port) == 0)
            {
                if (communicate_with_ss(ss_ip, ss_port, READ, file_path, ack) == 0)
                {
                    printf("Request successful!\n\n");
                }
            }
            // pthread_mutex_unlock(&request_mutex);
            break;
        case 2:
            get_file_path(file_path, sizeof(file_path));
            printf("Enter the content that you want to add in the file: ");
            fgets(file_content, sizeof(file_content), stdin);
            file_content[strcspn(file_content, "\n")] = '\0';
            // snprintf(request, sizeof(request), "WRITE %d %s %s", client_port, file_path, file_content);
            // pthread_mutex_lock(&request_mutex);
            if (communicate_with_nm(WRITE, file_path, ss_ip, &ss_port) == 0)
            {
                if (communicate_with_ss(ss_ip, ss_port, WRITE, file_path, ack) == 0)
                {
                    printf("Request successful!\n\n");
                }
            }
            pthread_mutex_unlock(&request_mutex);
            break;
        case 3:
            get_file_path(file_path, sizeof(file_path));
            // snprintf(request, sizeof(request), "INFO %d %s", client_port, file_path);
            // pthread_mutex_lock(&request_mutex);
            if (communicate_with_nm(GETINFO, file_path, ss_ip, &ss_port) == 0)
            {
                if (communicate_with_ss(ss_ip, ss_port, GETINFO, file_path, ack) == 0)
                {
                    printf("Request successful!\n\n");
                }
            }
            // pthread_mutex_unlock(&request_mutex);
            break;

        case 4:
            get_file_path(file_path, sizeof(file_path));
            // pthread_mutex_lock(&request_mutex);
            if (create_file(client_port, file_path, response) == 0)
            {
                printf("Request successful!\n\n");
            }
            // pthread_mutex_unlock(&request_mutex);
        case 5:
            get_file_path(file_path, sizeof(file_path));
            // pthread_mutex_lock(&request_mutex);
            if (delete_file(client_port, file_path, response) == 0)
            {
                printf("Request successful!\n\n");
            }
            // pthread_mutex_unlock(&request_mutex);
            break;
        case 6:
            get_file_path(file_path, sizeof(file_path));
            get_file_path(destination_path, sizeof(destination_path));
            // pthread_mutex_lock(&request_mutex);
            if (copy_file(client_port, file_path, destination_path, response) == 0)
            {
                printf("Response from Naming Server: %s\n", response);
            }
            // pthread_mutex_unlock(&request_mutex);
            break;
        case 7:
            get_file_path(dir_path, sizeof(dir_path));
            // pthread_mutex_lock(&request_mutex);
            if (create_directory(client_port, dir_path, response) == 0)
            {
                printf("Response from Naming Server: %s\n", response);
            }
            // pthread_mutex_unlock(&request_mutex);
            break;
        case 8:
            get_file_path(dir_path, sizeof(dir_path));
            // pthread_mutex_lock(&request_mutex);
            if (delete_directory(client_port, dir_path, response) == 0)
            {
                printf("Response from Naming Server: %s\n", response);
            }
            // pthread_mutex_unlock(&request_mutex);
            break;
        case 9:
            get_file_path(dir_path, sizeof(dir_path));
            get_file_path(destination_path, sizeof(destination_path));
            // pthread_mutex_lock(&request_mutex);
            if (copy_directory(client_port, dir_path, destination_path, response) == 0)
            {
                printf("Response from Naming Server: %s\n", response);
            }
            // pthread_mutex_unlock(&request_mutex);
            break;
        case 10:
            return 0;
        default:
            printf("Invalid option. Please select a valid option.\n");
        }
    }

    return NULL;
}

int communicate_with_nm(char *request, char *filePath, char *ss_ip, int *ss_port)
{
    fd_set read_fds;
    struct timeval timeout;

    // Handling Naming Server Connection
    struct sockaddr_in nmAddress;
    int nm_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (nm_socket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    nmAddress.sin_family = AF_INET;
    nmAddress.sin_port = htons(CLIENT_PORT_NM);
    nmAddress.sin_addr.s_addr = inet_addr(NM_IP);

    if (connect(nm_socket, (struct sockaddr *)&nmAddress, sizeof(nmAddress)) == -1)
    {
        perror("Connection to Naming Server failed");
        exit(EXIT_FAILURE);
    }

    printf("Request sent by client to nm: %s\n", request);
    printf("File Path sent by client to nm: %s\n", filePath);

    // Create a buffer to hold the combined data
    char combinedData[2 * MAX_PATH_LENGTH + 1]; // Assuming PATH_LENGTH is the maximum length of both strings
    snprintf(combinedData, sizeof(combinedData), "%s:%s", request, filePath);

    // Send the combined data
    send(nm_socket, combinedData, strlen(combinedData), 0);

    FD_ZERO(&read_fds);
    FD_SET(nm_socket, &read_fds);
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    int select_result = select(nm_socket + 1, &read_fds, NULL, NULL, &timeout);
    if (select_result == 0)
    {
        fprintf(stderr, "Timeout: Initial ACK still not received yet.\n");
        close(nm_socket);
        return -1;
    }
    else if (select_result == -1)
    {
        perror("Error in selecting");
        close(nm_socket);
        return -1;
    }

    // receiving the ip address and port number of the storage server from the naming server
    char receivedData[2 * MAX_PATH_LENGTH + 1]; // Assuming PATH_LENGTH is the maximum length of both strings
    recv(nm_socket, receivedData, sizeof(receivedData), 0);
    // Tokenize the received data using the delimiter (':' in this case)
    char *token = strtok(receivedData, ":");
    if (token != NULL)
    {
        // 'token' now contains the operatfilion
        const char *SS_IP = token;

        // Move to the next token
        token = strtok(NULL, ":");

        if (token != NULL)
        {
            // 'token' now contains the filePath
            const char *SS_port = token;

            strcpy(ss_ip, SS_IP);
            *ss_port = atoi(SS_port);
        }
        else
        {
            // Handle error: Not enough tokens
            printf("Not enough tokens\n");
        }
    }
    else
    {
        // Handle error: No tokens found
        printf("No tokens found\n");
    }

    close(nm_socket);
    return 0;
}

int communicate_with_nm_and_recv(int client_port, char *filePath, char *request, char *response)
{
    // Handling Naming Server Connection
    struct sockaddr_in nmAddress;
    int nm_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (nm_socket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    nmAddress.sin_family = AF_INET;
    nmAddress.sin_port = htons(CLIENT_PORT_NM);
    nmAddress.sin_addr.s_addr = inet_addr(NM_IP);

    if (connect(nm_socket, (struct sockaddr *)&nmAddress, sizeof(nmAddress)) == -1)
    {
        perror("2, Connection to Naming Server failed");
        exit(EXIT_FAILURE);
    }
    printf("Naming Server Connected\n");

    printf("Request sent by client to nm and recv: %s\n", request);
    printf("File Path sent by client to nm and recv: %s\n", filePath);

    ///////
    // sending the operation to the naming server
    char combinedData[2 * MAX_PATH_LENGTH + 1]; // Assuming PATH_LENGTH is the maximum length of both strings
    snprintf(combinedData, sizeof(combinedData), "%s:%s", request, filePath);
    printf("Combined Data: %s\n", combinedData);
    // Send the combined data
    send(nm_socket, combinedData, strlen(combinedData), 0);
    ///////

    fd_set read_fds;
    struct timeval timeout;
    FD_ZERO(&read_fds);
    FD_SET(nm_socket, &read_fds);
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    int select_result = select(nm_socket + 1, &read_fds, NULL, NULL, &timeout);
    if (select_result == 0)
    {
        fprintf(stderr, "Timeout:Initial ACK still not received yet.\n");
        close(nm_socket);
        return -1;
    }
    else if (select_result == -1)
    {
        perror("Error in selecting");
        close(nm_socket);
        return -1;
    }

    ///////
    // receiving the acknowledgement from the naming server regarding status of operation
    recv(nm_socket, response, 1024, 0);
    ///////

    close(nm_socket);
    return 0;
}

int communicate_with_nm_and_recv_copy(int client_port, char *srcPath, char *destPath, char *request, char *response)
{
    // Handling Naming Server Connection
    struct sockaddr_in nmAddress;
    int nm_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (nm_socket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    nmAddress.sin_family = AF_INET;
    nmAddress.sin_port = htons(CLIENT_PORT_NM);
    nmAddress.sin_addr.s_addr = inet_addr(NM_IP);

    if (connect(nm_socket, (struct sockaddr *)&nmAddress, sizeof(nmAddress)) == -1)
    {
        perror("3, Connection to Naming Server failed");
        exit(EXIT_FAILURE);
    }
    printf("Naming Server Connected\n");
    char request_with_port[1024]; // this should be filepath
    snprintf(request_with_port, sizeof(request_with_port), "%d %s", client_port, request);

    ///////
    // sending the operation to the naming server
    send(nm_socket, request_with_port, strlen(request_with_port), 0);
    // sending the source path to the naming server, added
    send(nm_socket, srcPath, strlen(srcPath), 0);
    // sending the destination path to the naming server, added
    send(nm_socket, destPath, strlen(destPath), 0);
    ///////

    fd_set read_fds;
    struct timeval timeout;
    FD_ZERO(&read_fds);
    FD_SET(nm_socket, &read_fds);
    timeout.tv_sec = TIMEOUT_SECONDS;
    timeout.tv_usec = 0;
    int select_result = select(nm_socket + 1, &read_fds, NULL, NULL, &timeout);
    if (select_result == 0)
    {
        fprintf(stderr, "Timeout:Initial ACK still not received yet.\n");
        close(nm_socket);
        return -1;
    }
    else if (select_result == -1)
    {
        perror("Error in selecting");
        close(nm_socket);
        return -1;
    }

    ///////
    // receiving the acknowledgement from the naming server regarding status of operation
    recv(nm_socket, response, 1024, 0);
    ///////

    close(nm_socket);
    return 0;
}

int communicate_with_ss(char *ss_ip, int ss_port, char *operation, char *filePath, int ack)
{
    int ss_socket;
    struct sockaddr_in server_addr;
    ss_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (ss_socket == -1)
    {
        perror("Socket creation failed");
        return -1;
    }
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(ss_port);
    server_addr.sin_addr.s_addr = inet_addr(ss_ip);

    if (connect(ss_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1)
    {
        perror("Connection to SS failed");
        return -1;
    }

    printf("Request sent by client to ss: %s\n", operation);
    printf("File Path sent by client to ss: %s\n", filePath);

    ///////
    // send combined operation and file path to SS
    char combinedData[2 * MAX_PATH_LENGTH + 1]; // Assuming PATH_LENGTH is the maximum length of both strings
    snprintf(combinedData, sizeof(combinedData), "%s:%s", operation, filePath);

    // Send the combined data to the storage server
    if (send(ss_socket, combinedData, strlen(combinedData), 0) == -1)
    {
        perror("Error in sending data to SS");
        return -1;
    }
    printf("Sent request and file path to SS");

    // char buffer[MAX_BUFFER_SIZE];
    // int bytesReceived;

    // while ((bytesReceived = recv(ss_socket, buffer, sizeof(buffer), 0)) > 0)
    // {
    //     // Write the received data to the file
    //     printf("buffer: %s", buffer);
    //     // Check if the buffer is smaller than the expected data
    //     if (bytesReceived < MAX_BUFFER_SIZE)
    //     {
    //         break; // Data reception is complete
    //     }
    // }

    // printf("Received data from SS ");
    if (strcmp(operation, READ) == 0)
    {
        char buffer[MAX_BUFFER_SIZE];
        int bytesReceived;
        int i = 0;
        // printf("Entering the while loop");
        while ((bytesReceived = recv(ss_socket, buffer, sizeof(buffer), 0)) > 0)
        {
            // Write the received data to the file or handle as needed
            for (int i = 0; i < bytesReceived; ++i)
            {
                printf("%c", buffer[i]); // Print each character individually
            }

            // Check if the buffer is smaller than the expected data
            if (bytesReceived < MAX_BUFFER_SIZE)
            {
                break; // Data reception is complete
            }
        }

        // Check if there was an error or the transmission ended
        if (bytesReceived < 0)
        {
            perror("\nrecv failed");
        }
        else if (bytesReceived == 0)
        {
            printf("\nConnection closed by peer\n");
        }
        else
        {
            printf("\nReceived data from SS\n");
            // goto out;
            // printf("Received 1");
            // exit(1);
        }
        // out:READ
        // printf("Received 2");
        char ackno[100] = "";
        // receiving the acknowledgement from the storage server regarding status of operation
        // printf("Received ackknoledgement from SS %d", ackno);
        recv(ss_socket, &ackno, 100, 0);
        printf("Received ackknoledgement from SS %s", ackno);
    }
    else if (strcmp(operation, GETINFO) == 0)
    {
        struct FileInfo file_info;

        // Receive file information from the server
        int recvResult = recv(ss_socket, &file_info, sizeof(struct FileInfo), 0);
        if (recvResult == -1)
        {
            perror("Error receiving file information");
        }
        else if (recvResult == 0)
        {
            printf("Connection closed by server\n");
        }
        else
        {
            // Access received file information
            printf("Received file information:\n");
            printf("File Size: %lld bytes\n", (long long)file_info.size);
            printf("Permissions: %o\n", file_info.permissions);
        }
    }

    char ackno[100] = "";
    recv(ss_socket, &ackno, 100, 0);
    printf("Received ackknoledgement from SS:  %s\n", ackno);

    close(ss_socket);
    return 0;
}

int create_delete_copy_file(int client_port, char *action, char *path, char *response)
{
    char request[1024];
    snprintf(request, sizeof(request), "%s", action);
    return communicate_with_nm_and_recv(client_port, path, request, response);
}

int create_file(int client_port, char *file_path, char *response)
{
    return create_delete_copy_file(client_port, CREATEFILE, file_path, response);
}

int delete_file(int client_port, char *file_path, char *response)
{
    return create_delete_copy_file(client_port, DELETEFILE, file_path, response);
}

int create_directory(int client_port, char *dir_path, char *response)
{
    return communicate_with_nm_and_recv(client_port, dir_path, CREATEDIR, response);
}

int delete_directory(int client_port, char *dir_path, char *response)
{
    return communicate_with_nm_and_recv(client_port, dir_path, DELETEDIR, response);
}

int copy_file(int client_port, char *source_path, char *destination_path, char *response)
{
    return communicate_with_nm_and_recv_copy(client_port, source_path, destination_path, COPYFILE, response);
}

int copy_directory(int client_port, char *source_path, char *destination_path, char *response)
{
    return communicate_with_nm_and_recv_copy(client_port, source_path, destination_path, COPYDIR, response);
}

int main()
{
    pthread_t thread;
    int client_id = 0;

    while (1)
    {
        // Create a new thread when the user requests it
        printf("Do you want to create a new client thread? (y/n): ");
        char choice;
        scanf(" %c", &choice);

        if (choice == 'n')
            break;

        if (pthread_create(&thread, NULL, client_thread, (void *)&client_id) != 0)
        {
            perror("Error in creation of client thread");
            exit(EXIT_FAILURE);
        }

        pthread_join(thread, NULL); // Wait for the thread to finish
        client_id++;
    }
    return 0;
}
