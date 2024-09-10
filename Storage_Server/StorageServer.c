#include "headers.h"

void initializeStorageServer(struct storageServer storage_server, int serverID)
{
    // Initialize storage server details
    storage_server.isAlive = 1;
    storage_server.numFiles = 0;
    storage_server.numDirectories = 0;
    storage_server.port_nm = SS_PORT_NM;         // Naming server port
    storage_server.port_client = CLIENT_PORT_NM; // Client request port

    for (int i = 0; i < MAX_PATHS; i++)
    {
        strcpy(storage_server.files[i], "");
        strcpy(storage_server.directory[i], "");
    }
}

void tree_search(char *basepath, struct storageServer *server)
{
    DIR *dir;
    struct dirent *entry;
    struct stat statbuf;

    if ((dir = opendir(basepath)) == NULL)
    {
        printf("Error opening %s\n", basepath);
        return;
    }

    // print names of all files in dir
    while ((entry = readdir(dir)))
    {
        char temppath[MAX_PATH_LENGTH];
        strcpy(temppath, basepath);
        strcat(temppath, "/");
        strcat(temppath, entry->d_name);
        lstat(temppath, &statbuf);

        if (S_ISDIR(statbuf.st_mode))
        {
            // found a dir, but ignore . and ..
            if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, ".."))
            {
                continue;
            }

            // Allocate memory for the directory name
            strcpy(server->directory[server->numDirectories], (temppath + strlen(server->currentDirectory) + 1));
            server->numDirectories += 1;
            tree_search(temppath, server);
        }
        else if (S_ISREG(statbuf.st_mode))
        {
            // Allocate memory for the file name
            strcpy(server->files[server->numFiles], (temppath + strlen(server->currentDirectory) + 1));
            server->numFiles++;
        }
    }

    closedir(dir);
}

void getfulldir(char *dirname)
{
    char temp[1024];
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    strcpy(temp, cwd);
    if (dirname[0] != '\0')
    {
        strcat(temp, "/");
    }
    strcat(temp, dirname);
    strcpy(dirname, temp);
    return;
}

void handle_rwp(int nm_socket, char *req)
{
    // Ensure that MAX_CLIENT_THREADS limit is not reached
    // semwait(&thread_semaphore);

    // Find the first available slot in the client_threads array
    int i;
    for (i = 0; i < MAX_CLIENT_THREADS; ++i)
    {
        if (!client_threads1[i])
            break;
    }

    // Create a thread only if a slot is available
    if (i < MAX_CLIENT_THREADS)
    {
        if (pthread_create(&client_threads1[i], NULL, client_request_handler, (void *)req) != 0)
        {
            perror("Error creating client thread");
        }
    }
}

void *handle_CREATEDIR(void *arg)
{
    struct ThreadArgs1 *args = (struct ThreadArgs1 *)arg;

    int nm_socket = args->nm_socket;
    struct storageServer *storage_server = args->storage_server;
    // Receive the directory name from the naming server
    char dirname[MAX_PATH_LENGTH];
    recv(nm_socket, dirname, sizeof(dirname), 0);

    int ack = 1;
    // Handle CREATEDIR request
    if (mkdir(dirname, 0777) == 0)
    {
        printf("Successfully created the directory: %s\n", dirname);

        // Update the directory count and array
        // pthread_mutex_lock(&storage_mutex);
        strcpy(storage_server->directory[storage_server->numDirectories], dirname);
        storage_server->numDirectories++;
        // pthread_mutex_unlock(&storage_mutex);

        // Send acknowledgment to the naming server if required
        send(nm_socket, &ack, sizeof(int), 0);
    }
    else
    {
        ack = 0;
        printf("Failed to create the directory: %s\n", dirname);
        // Send failure acknowledgment to the naming server if required
        send(nm_socket, &ack, sizeof(int), 0);
    }
    // sempost(&thread_semaphore);
    pthread_exit(NULL);
}

void *handle_DELETEDIR(void *arg)
{
    struct ThreadArgs1 *args = (struct ThreadArgs1 *)arg;

    int nm_socket = args->nm_socket;
    struct storageServer *storage_server = args->storage_server;
    // Receive the directory name from the naming server
    char dirname[MAX_PATH_LENGTH];
    recv(nm_socket, dirname, sizeof(dirname), 0);

    int ack = 1;
    // Handle DELETEDIR request
    if (rmdir(dirname) == 0)
    {
        printf("Successfully deleted the directory: %s\n", dirname);

        // Update the directory count and array
        for (int i = 0; i < storage_server->numDirectories; ++i)
        {
            if (strcmp(storage_server->directory[i], dirname) == 0)
            {
                // pthread_mutex_lock(&storage_mutex);
                for (int j = i; j < storage_server->numDirectories - 1; ++j)
                {
                    strcpy(storage_server->directory[j], storage_server->directory[j + 1]);
                }
                storage_server->numDirectories--;
                // pthread_mutex_unlock(&storage_mutex);
                break;
            }
        }

        // Send acknowledgment to the naming server if required
        send(nm_socket, &ack, sizeof(int), 0);
    }
    else
    {
        ack = 0;
        printf("Failed to delete the directory: %s\n", dirname);
        // Send failure acknowledgment to the naming server if required
        send(nm_socket, &ack, sizeof(int), 0);
    }
    // sempost(&thread_semaphore);
    pthread_exit(NULL);
}

void *handle_CREATEFILE(void *arg)
{
    struct ThreadArgs1 *args = (struct ThreadArgs1 *)arg;

    int nm_socket = args->nm_socket;
    struct storageServer *storage_server = args->storage_server;

    int ack = 1;
    // Handle CREATEFILE request

    getfulldir(args->filepath);
    printf("Creating File %s\n", args->filepath);
    // go to parent dir
    chdir(args->filepath);
    FILE *file = fopen(args->filepath, "w");
    if (file == NULL)
    {
        ack = 0;
        send(nm_socket, &ack, sizeof(int), 0);
        printf("Failed to create the file: %s\n", args->filepath);
        // Send failure acknowledgment to the naming server if required
    }
    else
    {
        // Send acknowledgment to the naming server
        send(nm_socket, &ack, sizeof(int), 0);
        printf("Successfully created the file: %s\n", args->filepath);
        fclose(file);

        // Update the file count and array
        // pthread_mutex_lock(&storage_mutex);
        strcpy(storage_server->files[storage_server->numFiles], args->filepath);
        storage_server->numFiles++;
        // pthread_mutex_unlock(&storage_mutex);
    }
    fclose(file);
    // sempost(&thread_semaphore);
    pthread_exit(NULL);
}

void *handle_DELETEFILE(void *arg)
{
    struct ThreadArgs1 *args = (struct ThreadArgs1 *)arg;

    int nm_socket = args->nm_socket;
    struct storageServer *storage_server = args->storage_server;

    // Receive the file name from the naming server
    char filepath[MAX_PATH_LENGTH];
    recv(nm_socket, filepath, MAX_PATH_LENGTH, 0);

    int ack = 1;
    // Handle DELETEFILE request
    if (remove(filepath) == 0)
    {
        printf("Successfully deleted the file: %s\n", filepath);

        // Update the file count and array
        for (int i = 0; i < storage_server->numFiles; ++i)
        {
            if (strcmp(storage_server->files[i], filepath) == 0)
            {
                // pthread_mutex_lock(&storage_mutex);
                for (int j = i; j < storage_server->numFiles - 1; ++j)
                {
                    strcpy(storage_server->files[j], storage_server->files[j + 1]);
                }
                storage_server->numFiles--;
                // pthread_mutex_unlock(&storage_mutex);
                break;
            }
        }

        // Send acknowledgment to the naming server if required
        send(nm_socket, &ack, sizeof(int), 0);
    }
    else
    {
        ack = 0;
        printf("Failed to delete the file: %s\n", filepath);
        // Send failure acknowledgment to the naming server if required
        send(nm_socket, &ack, sizeof(int), 0);
    }
    // sempost(&thread_semaphore);
    pthread_exit(NULL);
}

void *client_request_handler(void *arg)
{
    int newSocket = *(int *)arg;
    char filePath[MAX_PATH_LENGTH];
    char request[30];
    // receive combined operation and file path from the client
    char receivedData[2 * MAX_PATH_LENGTH + 1]; // Assuming PATH_LENGTH is the maximum length of both strings
    if (recv(newSocket, &receivedData, sizeof(receivedData), 0) < 0)
    {
        perror("Error receiving data");
        exit(EXIT_FAILURE);
    }
    printf("1:Received Data: %s\n", receivedData);
    // Tokenize the received data using the delimiter (':' in this case)
    char *token = strtok(receivedData, ":");
    if (token != NULL)
    {
        // 'token' now contains the operatfilion
        const char *operation = token;

        // Move to the next token
        token = strtok(NULL, ":");

        if (token != NULL)
        {
            // 'token' now contains the filePath
            const char *filepath = token;

            // Now you have 'operation' and 'filePath' as separate strings
            printf("1 Operation: %s\n", operation);
            printf("1 File Path: %s\n", filepath);
            strcpy(request, operation);
            strcpy(filePath, filepath);
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

    if (strcmp(request, READ) == 0)
    {
        char ack[100] = "Request completed successfully"; 
        // Handling READ request
        FILE *file = fopen(filePath, "r");
        if (file == NULL)
        {
            strcpy(ack, FOPENERR);
            perror("Error opening file");
            pthread_exit(NULL);
        }
        else
        {
            char buffer[MAX_BUFFER_SIZE];
            size_t bytesRead;
            int i = 0;

            while ((bytesRead = fread(buffer, sizeof(char), sizeof(buffer), file)) > 0)
            {
                // Send the file content to the client using new_socket
                size_t bytesSent = send(newSocket, buffer, bytesRead, 0);

                if (bytesSent < 0)
                {
                    strcpy(ack, SENDERR);
                    perror("Error sending file content");
                    break; // Break the loop if send fails
                }
                else if (bytesSent != bytesRead)
                {
                    fprintf(stderr, "Incomplete send: %zd out of %zu bytes sent\n", bytesSent, bytesRead);
                    // Handle the situation where not all bytes were sent
                    // You might want to resend the remaining bytes or break the loop based on your application's logic
                    break;
                }
            }

            // File sending completed or error occurred
            if (bytesRead < 0)
            {
                strcpy(ack, BADREAD);
                perror("Error reading file");
            }
            else
            {
                printf("Sent the data to client.\n");
                // send acknowledgement to the naming server to send to the client
                
            }
            send(newSocket, &ack, 100, 0);
                printf("Sent the ack to client.\n");

            fclose(file);
        }
        // send(newSocket, &ack, sizeof(int), 0);
    }
    else if (strcmp(request, WRITE) == 0)
    {
        char ack[100] = "Request completed successfully"; 
        // Handling WRITE request
        FILE *file = fopen(filePath, "w");
        if (file == NULL)
        {
            strcpy(ack, FOPENERR);
            perror("Error opening file");
            pthread_exit(NULL);
        }
        else
        {
            char buffer[MAX_BUFFER_SIZE];
            int bytesReceived;

            while ((bytesReceived = recv(newSocket, buffer, sizeof(buffer), 0)) > 0)
            {
                // Write the received data to the file
                fwrite(buffer, sizeof(char), bytesReceived, file);
                // Check if the buffer is smaller than the expected data
                if (bytesReceived < MAX_BUFFER_SIZE)
                {
                    break; // Data reception is complete
                }
            }

            // Check if there was an error or the transmission ended
            if (bytesReceived < 0)
            {
                strcpy(ack, RECVERR);
                perror("Error receiving data");
            }

            fclose(file);
        }
        send(newSocket, &ack, 100, 0);
    }
    else if (strcmp(request, GETINFO) == 0)
    {
        char ack[100] = "Request completed successfully";
        struct FileInfo *file_info = (struct FileInfo *)malloc(sizeof(struct FileInfo));

        // Check if malloc was successful
        if (file_info == NULL)
        {
            perror("Memory allocation failed");
        }

        // Get file information using stat() or fstat() based on the filepath
        struct stat file_stat;
        if (stat(filePath, &file_stat) == -1)
        {
            strcpy(ack, GETFINFO);
            perror("Error getting file information");
        }

        // Populate file information into the FileInfo structure
        file_info->size = file_stat.st_size;
        file_info->permissions = file_stat.st_mode;

        // Send file information to the client
        if (send(newSocket, file_info, sizeof(struct FileInfo), 0) == -1)
        {
            strcpy(ack, SENDFINFO);
            perror("Error sending file information");
        }
        
        
        send(newSocket, &ack, 100, 0);
        printf("Sent acknowledgement to client");
        // Free allocated memory after sending to client
        free(file_info);
    }

    // sempost(&thread_semaphore);
    pthread_exit(NULL);
}

void *nm_request_handler(void *arg)
{
    struct Request *nm_req = (struct Request *)arg;
    struct storageServer *storage_server = nm_req->storage_server;

    // Receive requests and handle them
    while (1)
    {
        printf("Waiting for request\n");
        // operation is stored in this
        int valread = recv(nm_req->socket, nm_req->buffer,30, 0);
        if (valread == 0)
        {
            puts("Breaking out of the while loop");
            break;
        }
        else if (valread == -1)
        {
            perror("Recv failed");
            break;
        }
        else
        {
            nm_req->buffer[valread] = '\0'; // Null-terminate the received message
            printf("Received message: %s\n", nm_req->buffer);

            // Handle different types of requests
            if (strcmp(nm_req->buffer, READ) == 0)
            {
                handle_rwp(nm_req->socket, READ);
            }
            else if (strcmp(nm_req->buffer, WRITE) == 0)
            {
                handle_rwp(nm_req->socket, WRITE);
            }
            else if (strcmp(nm_req->buffer, COPYDIR) == 0)
            {
                // Handle COPYDIR request
            }
            else if (strcmp(nm_req->buffer, COPYFILE) == 0)
            {
                // Handle COPYFILE request
            }
            else if (strcmp(nm_req->buffer, CREATEDIR) == 0)
            {
                // Ensure that MAX_CLIENT_THREADS limit is not reached
                // semwait(&thread_semaphore);
                // Find the first available slot in the client_threads array
                int i;
                for (i = 0; i < MAX_CLIENT_THREADS; ++i)
                {
                    if (!client_threads1[i])
                        break;
                }
                struct ThreadArgs1 *args = (struct ThreadArgs1 *)malloc(sizeof(struct ThreadArgs1));
                args->nm_socket = nm_req->socket;
                args->storage_server = storage_server;

                char filepath[MAX_PATH_LENGTH];
                // receive the file path from naming server
                if (recv(args->nm_socket, filepath, MAX_PATH_LENGTH, 0) < 0)
                {
                    perror("Error receiving file path");
                    exit(EXIT_FAILURE);
                }
                strcpy(args->filepath, filepath);

                // Create a thread only if a slot is available
                if (i < MAX_CLIENT_THREADS && pthread_create(&client_threads1[i], NULL, handle_CREATEDIR, (void *)args) != 0)
                {
                    perror("Error creating client thread");
                }
                // free(args);
            }
            else if (strcmp(nm_req->buffer, DELETEDIR) == 0)
            {
                // Ensure that MAX_CLIENT_THREADS limit is not reached
                // semwait(&thread_semaphore);
                // Find the first available slot in the client_threads array
                int i;
                for (i = 0; i < MAX_CLIENT_THREADS; ++i)
                {
                    if (!client_threads1[i])
                        break;
                }
                struct ThreadArgs1 *args = (struct ThreadArgs1 *)malloc(sizeof(struct ThreadArgs1));
                args->nm_socket = nm_req->socket;
                args->storage_server = storage_server;

                char filepath[MAX_PATH_LENGTH];
                // receive the file path from naming server
                if (recv(args->nm_socket, filepath, MAX_PATH_LENGTH, 0) < 0)
                {
                    perror("Error receiving file path");
                    exit(EXIT_FAILURE);
                }
                strcpy(args->filepath, filepath);

                // Create a thread only if a slot is available
                if (i < MAX_CLIENT_THREADS && pthread_create(&client_threads1[i], NULL, handle_DELETEDIR, (void *)args) != 0)
                {
                    perror("Error creating client thread");
                }
                // free(args);
            }
            else if (strcmp(nm_req->buffer, CREATEFILE) == 0)
            {
                // Ensure that MAX_CLIENT_THREADS limit is not reached
                // semwait(&thread_semaphore);
                // Find the first available slot in the client_threads array
                int i;
                for (i = 0; i < MAX_CLIENT_THREADS; ++i)
                {
                    if (!client_threads1[i])
                        break;
                }
                struct ThreadArgs1 *args = (struct ThreadArgs1 *)malloc(sizeof(struct ThreadArgs1));
                args->nm_socket = nm_req->socket;
                args->storage_server = storage_server;

                char filepath[MAX_PATH_LENGTH];
                // receive the file path from naming server
                if (recv(args->nm_socket, filepath, MAX_PATH_LENGTH, 0) < 0)
                {
                    perror("Error receiving file path");
                    exit(EXIT_FAILURE);
                }
                strcpy(args->filepath, filepath);

                // Create a thread only if a slot is available
                if (i < MAX_CLIENT_THREADS && pthread_create(&client_threads1[i], NULL, handle_CREATEFILE, (void *)args) != 0)
                {
                    perror("Error creating client thread");
                }
                // free(args);
            }
            else if (strcmp(nm_req->buffer, DELETEFILE) == 0)
            {
                // Ensure that MAX_CLIENT_THREADS limit is not reached
                // semwait(&thread_semaphore);
                // Find the first available slot in the client_threads array
                int i;
                for (i = 0; i < MAX_CLIENT_THREADS; ++i)
                {
                    if (!client_threads1[i])
                        break;
                }
                struct ThreadArgs1 *args = (struct ThreadArgs1 *)malloc(sizeof(struct ThreadArgs1));
                args->nm_socket = nm_req->socket;
                args->storage_server = storage_server;

                char filepath[MAX_PATH_LENGTH];
                // receive the file path from naming server
                if (recv(args->nm_socket, filepath, MAX_PATH_LENGTH, 0) < 0)
                {
                    perror("Error receiving file path");
                    exit(EXIT_FAILURE);
                }
                strcpy(args->filepath, filepath);

                // Create a thread only if a slot is available
                if (i < MAX_CLIENT_THREADS && pthread_create(&client_threads1[i], NULL, handle_DELETEFILE, (void *)args) != 0)
                {
                    perror("Error creating client thread");
                }
                // free(args);
            }
            else
            {
                // Unknown request received
                printf("Unknown request received: %s\n", nm_req->buffer);
            }
        }
    }

    close(nm_req->socket);
    pthread_exit(NULL);
}

void *clientListener(void *args)
{
    int clientSocket = *(int *)args;
    struct sockaddr_in clientAddress;

    if (listen(clientSocket, MAX_CLIENTS) < 0)
    {
        printf("Error listening to client\n");
        exit(1);
    }

    int i = 0;
    pthread_t clientListenerThread[MAX_CLIENTS];
    while (1)
    {
        int clientlen = sizeof(clientAddress);
        int new_socket = accept(clientSocket, (struct sockaddr *)&clientAddress, &clientlen);
        if (new_socket < 0)
        {
            printf("Error accepting client\n");
            exit(1);
        }

        if (i == MAX_CLIENTS)
        {
            printf("Too many storage servers\n");
            break;
        }
        else
        {
            pthread_t thread;
            if (pthread_create(&thread, NULL, client_request_handler, (void *)&new_socket))
            {
                perror("Error creating client thread");
                exit(EXIT_FAILURE);
            }
            i++;
        }
    }
    // Join the created threads
    for (int j = 0; j < i; j++)
    {
        pthread_join(clientListenerThread[j], NULL);
    }
}

int main()
{
    char truecwd[MAX_PATH_LENGTH];
    getcwd(truecwd, MAX_PATH_LENGTH);
    printf("Current working directory: %s\n", truecwd);

    struct storageServer storage_server;

    printf("Enter Server No:\n");
    scanf("%d", &storage_server.serverID);

    strcpy(storage_server.currentDirectory, truecwd);
    initializeStorageServer(storage_server, storage_server.serverID);

    // Start recursive tree search from the current directory
    tree_search(storage_server.currentDirectory, &storage_server);

    // Handling Naming Server Connection
    struct sockaddr_in nmAddress;
    int nm_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (nm_socket < 0)
    {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    nmAddress.sin_family = AF_INET;
    nmAddress.sin_port = htons(SS_PORT_NM);
    nmAddress.sin_addr.s_addr = inet_addr(NM_IP);

    if (connect(nm_socket, (struct sockaddr *)&nmAddress, sizeof(nmAddress)) == -1)
    {
        perror("Connection to Naming Server failed");
        exit(EXIT_FAILURE);
    }

    int client_port = 10 * storage_server.serverID + 3000;
    printf("Client Port: %d\n", client_port);

    // Send the storage server details to the naming server
    if (send(nm_socket, &storage_server, sizeof(struct storageServer), 0) < 0)
    {
        perror("Sending storage server details failed");
        exit(EXIT_FAILURE);
    }

    // Create a new thread to handle naming server request
    pthread_t nm_thread;
    if (pthread_create(&nm_thread, NULL, nm_request_handler, (void *)&nm_socket) != 0)
    {
        perror("Error creating naming server thread");
        exit(EXIT_FAILURE);
    }

    // pthread_mutex_lock(&mutex);
    // pthread_cond_wait(&initdone, &mutex);
    // pthread_mutex_unlock(&mutex);
    printf("Naming Server Connected\n");
    // seminit(&thread_semaphore, 0, MAX_STORAGE_SERVERS);

    // Handling Incoming Client Connection
    int clientSocket;
    struct sockaddr_in clientAddress;

    // Create socket for client requests
    if ((clientSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Client Socket creation failed");
        pthread_exit(NULL);
    }
    clientAddress.sin_family = AF_INET;
    clientAddress.sin_addr.s_addr = INADDR_ANY;
    clientAddress.sin_port = htons(client_port);

    if (bind(clientSocket, (struct sockaddr *)&clientAddress, sizeof(clientAddress)) < 0)
    {
        perror("Client Socket bind failed");
        pthread_exit(NULL);
    }

    pthread_t clientThread;
    if (pthread_create(&clientThread, NULL, clientListener, (void *)&clientSocket))
    {
        perror("Error creating client thread");
        exit(EXIT_FAILURE);
    }
    printf("All Clients Connected\n");

    // pthread_mutex_lock(&mutex);
    // pthread_cond_wait(&initdone, &mutex);
    pthread_join(nm_thread, NULL); // Wait for the thread to finish before exiting
    pthread_join(clientThread, NULL);
    // pthread_mutex_unlock(&mutex);

    printf("Exiting Storage Server\n");
    close(nm_socket);
    close(clientSocket);

    return 0;
}