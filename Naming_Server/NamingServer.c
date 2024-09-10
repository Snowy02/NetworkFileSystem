#include "headers.h"

char Operation[30];
int tempserverID;
int clientTempServerID;
char log_msg[2048];
// logging part functions

void get_timestamp(char *timestamp, size_t size)
{
    time_t rawtime;
    struct tm *info;
    time(&rawtime);
    info = localtime(&rawtime);
    strftime(timestamp, size, "%Y-%m-%d %H:%M:%S", info);
}

void log_entry(const char *message)
{
    char timestamp[20];
    FILE *log_file = fopen(LOG_FILE, "a");
    if (!log_file)
    {
        perror("Error opening log file");
        return;
    }
    get_timestamp(timestamp, sizeof(timestamp));
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fclose(log_file);
}

///

// Hashing function
int getHash(char *path)
{
    long long int hash = 0, goodPrime = 31, power = 1;
    for (int i = 0; i < strlen(path); i++)
    {
        hash = (goodPrime * (path[i] - 'a' + 1) + hash) % MAX_STORAGE_SERVERS;
        power = (goodPrime * power) % MAX_STORAGE_SERVERS;
    }
    return hash;
}

// create a new hash table
void createHashTable(struct Node **hashTable, char *path)
{
    int hash = getHash(path);
    struct Node *newNode = (struct Node *)malloc(sizeof(struct Node));
    if (newNode != NULL)
    {
        newNode->path = path;
        newNode->next = hashTable[hash];
        hashTable[hash] = newNode;
    }
    else
    {
        exit(1);
    }
    return;
}

// find a path in the hash table
int findPath(struct Node **hashtable, char *a)
{
    int n = getHash(a);
    // printf("hash:%d\n", n);
    if (hashtable[n] != NULL)
    {
        for (struct Node *trav = hashtable[n]; trav != NULL; trav = trav->next)
        {
            // printf("path :%s1\n", trav->path);

            if (strcmp(trav->path, a) == 0)
            {
                // printf("path found:%s2\n", trav->path);
                return 1;
            }
        }
    }
    return 0;
}

// initialize the hash server
void initializeHashServer(struct HashTable *hashTable, struct storageServer *SSData)
{
    hashTable->SSData = *SSData;
    for (int i = 0; i < MAX_STORAGE_SERVERS; i++)
    {
        hashTable->files[i] = NULL;
        hashTable->directory[i] = NULL;
    }
    for (int i = 0; i < SSData->numFiles; i++)
    {
        createHashTable(hashTable->files, SSData->files[i]);
    }
    for (int i = 0; i < SSData->numDirectories; i++)
    {
        createHashTable(hashTable->directory, SSData->directory[i]);
    }
}

void updateHashServer(struct HashTable *hashTable, struct storageServer *SSData)
{
    hashTable->SSData = *SSData;
    for (int i = 0; i < SSData->numFiles; i++)
    {
        createHashTable(hashTable->files, SSData->files[i]);
    }
    for (int i = 0; i < SSData->numDirectories; i++)
    {
        createHashTable(hashTable->directory, SSData->directory[i]);
    }
}

// destroy a hash table
void destroyHashTable(PtrToNode nodePtr)
{
    if (nodePtr->next != NULL)
    {
        destroyHashTable(nodePtr->next);
        free(nodePtr);
    }
    else
    {
        free(nodePtr);
    }
    return;
}

void stripper(char *path)
{
    // Find the last occurrence of '/'
    char *lastSlash = strrchr(path, '/');

    if (lastSlash != NULL)
    {
        // Null-terminate the string at the last '/'
        *lastSlash = '\0';

        // Find the last occurrence of '.' (dot) in the modified string
        char *lastDot = strrchr(path, '.');

        if (lastDot != NULL)
        {
            // Null-terminate the string at the last '.'
            *lastDot = '\0';
        }
    }
}

void *ackChecker(void *arg)
{
    int serverID = *(int *)arg;
    int ssSocket = ss[serverID].SSSocket;
    int HB_Signal;
    recv(ssSocket, &HB_Signal, sizeof(int), 0); // receive the heartbeat signal
    sem_wait(&joinLocks[serverID]);             // lock the naming server, while the server is joining
    if (HB_Signal == SSAliveSignal)
    {
        isJoined[serverID] = 1;
        sem_post(&joinLocks[serverID]); // release the lock, so that the naming server can be accessed by other threads
        pthread_exit(NULL);
    }
}

void *throbbingHeart(void *arg)
{
    int serverID = *(int *)arg;
    int ssSocket = ss[serverID].SSSocket;
    int HB_Signal = SSAliveSignal;
    while (1)
    {
        sleep(3);                        // wait for 3 seconds, then send heartbeat
        sem_wait(&writeLocks[serverID]); // lock the naming server, so that no other thread can access it
        send(ssSocket, &HB_Signal, sizeof(int), 0);
        isJoined[serverID] = 0;
        sem_post(&writeLocks[serverID]); // release the lock, so that the naming server can be accessed by other threads
        pthread_t ackCheckerThread;
        pthread_create(&ackCheckerThread, NULL, ackChecker, &serverID);
        sleep(1);                // wait for 1 second, if no ack is received, then the server is dead
        if (!isJoined[serverID]) // if the server is dead, then exit the thread
        {
            SSs[serverID].isAlive = 0;
            printf("Storage Server %d disconnected\n", serverID);
            snprintf(log_msg, sizeof(log_msg), "Storage Server %d disconnected\n", serverID);
            log_entry(log_msg);

            sem_post(&writeLocks[serverID]);
            pthread_exit(NULL);
        }
        sem_post(&writeLocks[serverID]); // if the server is alive, then release the lock
    }
}

void *handleServerInitialization(void *fd)
{
    pthread_mutex_lock(&mutex);
    int SSSocket = *(int *)fd;
    struct storageServer SSData;

    // send the serverid to storage server
    // send(SSSocket, &threadArgs.serverID, sizeof(int), 0);

    // receive the storage server details
    if (recv(SSSocket, &SSData, sizeof(struct storageServer), 0) < 0)
    {
        printf("Error receiving server id\n");
        log_entry("Error receiving server id");
        exit(1);
    }

    int serverID = SSData.serverID;
    SSs[serverID] = SSData;
    initializeHashServer(&hashedData[serverID], &SSs[serverID]);
    numServersInit++;

    // if (numServersInit == INITSERVERS)
    // {
    //     pthread_cond_signal(&initdone);
    // }
    SSs[serverID].isAlive = 1;

    // print all the accessible files and directories in the naming server
    printf("Files in Server %d\n", serverID);
    snprintf(log_msg, sizeof(log_msg), "Files in Server %d\n", serverID);
    log_entry(log_msg);
    for (int i = 0; i < SSs[serverID].numFiles; i++)
    {
        printf("%s\n", SSs[serverID].files[i]);
        snprintf(log_msg, sizeof(log_msg), "%s\n", SSs[serverID].files[i]);
        log_entry(log_msg);
    }
    printf("Directories in Server %d\n", serverID);
    snprintf(log_msg, sizeof(log_msg), "Directories in Server %d\n", serverID);
    log_entry(log_msg);
    for (int i = 0; i < SSs[serverID].numDirectories; i++)
    {
        printf("%s\n", SSs[serverID].directory[i]);
        snprintf(log_msg, sizeof(log_msg), "%s\n", SSs[serverID].directory[i]);
        log_entry(log_msg);
    }

    // initialize semaphores
    sem_init(&writeLocks[serverID], 0, 1); // initialize write lock, which is used to grant access to write to the naming server
    sem_init(&joinLocks[serverID], 0, 0);  // initialize join lock, which joins the server to the naming server

    // create a thread for the throbbing heart
    pthread_t throbOfHeart;
    pthread_create(&throbOfHeart, NULL, throbbingHeart, &serverID);
    pthread_mutex_unlock(&mutex);
    pthread_exit(NULL);
}

void *handleClientRequests(void *arg)
{
    Client *clients = (Client *)arg;

    while (1)
    {
        char data[MAX_BUFFER_SIZE];
        char srcPath[MAX_PATH_LENGTH];
        char filePath[MAX_PATH_LENGTH];
        char destPath[MAX_PATH_LENGTH];

        // receive operation and file path from the client
        char receivedData[2 * MAX_PATH_LENGTH + 1]; // Assuming PATH_LENGTH is the maximum length of both strings
        recv(client->clientSocket, receivedData, sizeof(receivedData), 0);
        printf("Data received by the nms from client: %s\n", receivedData);
        snprintf(log_msg, sizeof(log_msg), "Data received by the nms from client: %s\n", receivedData);
        log_entry(log_msg);
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
                strcpy(Operation, operation);
                strcpy(filePath, filepath);
            }
            else
            {
                // Handle error: Not enough tokens
                printf("Not enough tokens\n");
                log_entry("Not enough tokens");
            }
        }
        else
        {
            // Handle error: No tokens found
            printf("No tokens found\n");
            log_entry("No tokens found");
        }

        if (strcmp(Operation, READ) == 0)
        {
            SBI_READ_WRITE_GETINFO(Operation, filePath);
            break;
        }
        else if (strcmp(Operation, WRITE) == 0)
        {
            SBI_READ_WRITE_GETINFO(Operation, filePath);
            break;
        }
        else if (strcmp(Operation, GETINFO) == 0)
        {
            SBI_READ_WRITE_GETINFO(Operation, filePath);
            break;
        }
        else if (strcmp(Operation, CREATEFILE) == 0)
        {
            SBI_CREATE_DELETE(Operation, filePath);
            break;
        }
        else if (strcmp(Operation, DELETEFILE) == 0)
        {
            SBI_CREATE_DELETE(Operation, filePath);
            break;
        }
        else if (strcmp(Operation, COPYFILE) == 0)
        {
            SBI_COPY(Operation, srcPath, destPath);
            break;
        }
        else if (strcmp(Operation, CREATEDIR) == 0)
        {
            SBI_CREATE_DELETE(Operation, filePath);
            break;
        }
        else if (strcmp(Operation, DELETEDIR) == 0)
        {
            SBI_CREATE_DELETE(Operation, filePath);
            break;
        }
        else if (strcmp(Operation, COPYDIR) == 0)
        {
            SBI_COPY(Operation, srcPath, destPath);
            break;
        }
        else
        {
            printf("The specified operation is invalid\n");
            log_entry("The specified operation is invalid");
        }
    }
}

// SBI means search by iteration
void SBI_READ_WRITE_GETINFO(char *Operation, char *filePath)
{
    int found;
    // search for the file in the hash table
    for (int i = 0; i < MAX_STORAGE_SERVERS; i++)
    {
        SSs[i].port_client = i * 10 + 3000;
        printf("Searching in Bucket %d \n", i);
        snprintf(log_msg, sizeof(log_msg), "Searching in Bucket %d \n", i);
        log_entry(log_msg);
        found = findPath(hashedData[i].files, filePath);
        if (found)
        {
            printf("Found in Bucket %d\n", i);
            snprintf(log_msg, sizeof(log_msg), "Found in Bucket %d\n", i);
            log_entry(log_msg);

            // sending the ip address and ss port for that storage server to the client
            char combinedDataclient[2 * MAX_PATH_LENGTH + 1]; // Assuming PATH_LENGTH is the maximum length of both strings
            snprintf(combinedDataclient, sizeof(combinedDataclient), "%s:%d", NM_IP, SSs[i].port_client);
            printf("Data sent by the nms to client: %s\n", combinedDataclient);
            snprintf(log_msg, sizeof(log_msg), "Data sent by the nms to client: %s\n", combinedDataclient);
            log_entry(log_msg);
            send(client->clientSocket, combinedDataclient, strlen(combinedDataclient), 0);

            // send the port to the storage server
            send(ss[i].SSSocket, &SSs[i].port_client, sizeof(int), 0);
            break;
        }
    }
    if (!found)
    {
        printf("Not found in any storage server\n");
        log_entry("Not found in any storage server");
    }
}

void SBI_CREATE_DELETE(char *Operation, char *filePath)
{
    int found;
    int ack = 1;
    if (strcmp(Operation, CREATEFILE) == 0)
    {
        printf("Create File\n");
        log_entry("Create File\n");
        for (int i = 0; i < MAX_STORAGE_SERVERS; i++)
        {
            // search for the file in the hash table
            SSs[i].port_client = i * 10 + 3000;
            found = findPath(hashedData[i].files, filePath);
            if (found)
            {
                printf("Found in Bucket %d\n", i);
                snprintf(log_msg, sizeof(log_msg), "Found in Bucket %d\n", i);
                log_entry(log_msg);
                stripper(filePath);
                printf("File already exists in storage server\n");
                log_entry("File already exists in storage server");

                

                // receive acknowledgement of operation status from storage server
                recv(ss[i].SSSocket, &ack, sizeof(int), 0);
                break;
            }
        }
        if (!found)
        {
            send(ss[1].SSSocket, Operation, 30, 0);
            send(ss[1].SSSocket, filePath, MAX_PATH_LENGTH, 0);

            // update the files and directories array in the hashtable, and variables associated with it
                updateHashServer(&hashedData[1], &SSs[1]);
        }
        send(client->clientSocket, &ack, sizeof(int), 0);
    }
    else if (strcmp(Operation, CREATEDIR) == 0)
    {
        for (int i = 0; i < MAX_STORAGE_SERVERS; i++)
        {
            // search for the file in the hash table
            SSs[i].port_client = i * 10 + 3000;
            found = findPath(hashedData[i].directory, filePath);
            if (found)
            {
                printf("Found in Bucket %d\n", i);
                snprintf(log_msg, sizeof(log_msg), "Found in Bucket %d\n", i);
                log_entry(log_msg);
                stripper(filePath);

                // send the file path to the storage server
                send(ss[i].SSSocket, filePath, sizeof(filePath), 0);

                // update the files and directories array in the hashtable, and variables associated with it
                updateHashServer(&hashedData[i], &SSs[i]);

                // receive acknowledgement of operation status from storage server
                recv(ss[i].SSSocket, &ack, sizeof(int), 0);
                break;
            }
        }
        if (!found)
        {
            ack = 0;
            printf("Not found in any storage server\n");
            log_entry("Not found in any storage server\n");
        }
        send(client->clientSocket, &ack, sizeof(int), 0);
    }
    else if (strcmp(Operation, DELETEFILE) == 0)
    {
        printf("Delete file\n");
        log_entry("Delete file\n");
        for (int i = 0; i < MAX_STORAGE_SERVERS; i++)
        {
            // search for the file in the hash table
            SSs[i].port_client = i * 10 + 3000;
            found = findPath(hashedData[i].files, filePath);
            if (found)
            {
                printf("Found in Bucket %d\n", i);
                snprintf(log_msg, sizeof(log_msg), "Found in Bucket %d\n", i);
                log_entry(log_msg);

                // send the file path to the storage server
                send(ss[i].SSSocket, filePath, sizeof(filePath), 0);

                // update the files and directories array in the hashtable, and variables associated with it
                updateHashServer(&hashedData[i], &SSs[i]);

                // receive acknowledgement of operation status from storage server
                recv(ss[i].SSSocket, &ack, sizeof(int), 0);
                break;
            }
        }
        if (!found)
        {
            ack = 0;
            printf("Not found in any storage server\n");
            log_entry("Not found in any storage server");
        }
        send(client->clientSocket, &ack, sizeof(int), 0);
    }
    else if (strcmp(Operation, DELETEDIR) == 0)
    {
        for (int i = 0; i < MAX_STORAGE_SERVERS; i++)
        {
            // search for the file in the hash table
            SSs[i].port_client = i * 10 + 3000;
            found = findPath(hashedData[i].directory, filePath);
            if (found)
            {
                printf("Found in Bucket %d\n", i);
                snprintf(log_msg, sizeof(log_msg), "Found in Bucket %d\n", i);
                log_entry(log_msg);

                // send the file path to the storage server
                send(ss[i].SSSocket, filePath, sizeof(filePath), 0);

                // update the files and directories array in the hashtable, and variables associated with it
                updateHashServer(&hashedData[i], &SSs[i]);

                // receive acknowledgement of operation status from storage server
                recv(ss[i].SSSocket, &ack, sizeof(int), 0);
                break;
            }
        }
        if (!found)
        {
            ack = 0;
            printf("Not found in any storage server\n");
            log_entry("Not found in any storage server");
        }
        send(client->clientSocket, &ack, sizeof(int), 0);
    }
}

void SBI_COPY(char *Operation, char *srcPath, char *destPath)
{
    // recv(client->clientSocket, srcPath, sizeof(srcPath), 0);
    recv(client->clientSocket, destPath, sizeof(destPath), 0);
    printf("Source Path: %s\n", srcPath);
    snprintf(log_msg, sizeof(log_msg), "Source Path: %s\n", srcPath);
    log_entry(log_msg);
    printf("Destination Path: %s\n", destPath);
    snprintf(log_msg, sizeof(log_msg), "Destination Path: %s\n", destPath);
    log_entry(log_msg);

    int ack = 1;
    int found1;
    int found2;
    // search for the file in the hash table
    for (int i = 0; i < MAX_STORAGE_SERVERS; i++)
    {
        found1 = findPath(hashedData[i].files, srcPath);
        found2 = findPath(hashedData[i].files, destPath);
        printf("Searching in Bucket %d \n", i);
        snprintf(log_msg, sizeof(log_msg), "Searching in Bucket %d \n", i);
        log_entry(log_msg);

        if (found1 && found2)
        {
            printf("Found in Bucket %d\n", i);
            snprintf(log_msg, sizeof(log_msg), "Found in Bucket %d\n", i);
            log_entry(log_msg);
            // sending operation and source path and destination path to storage server
            send(ss[i].SSSocket, Operation, sizeof(int), 0);
            send(ss[i].SSSocket, srcPath, sizeof(srcPath), 0);
            send(ss[i].SSSocket, destPath, sizeof(destPath), 0);

            // update the files and directories array in the hashtable, and variables associated with it
            updateHashServer(&hashedData[i], &SSs[i]);

            // receive acknowledgement of operation status from storage server
            recv(ss[i].SSSocket, &ack, sizeof(int), 0);
            break;
        }
    }
    if (!found1 || !found2)
    {
        ack = 0;
        printf("Source Path not found in any storage server\n");
        log_entry("Source Path not found in any storage server");
    }

    send(client->clientSocket, &ack, sizeof(int), 0);
}

void *storageServerListener(void *args)
{
    int SS_Socket = *(int *)args;

    if (listen(SS_Socket, MAX_STORAGE_SERVERS) < 0)
    {
        printf("Error listening to storage server\n");
        log_entry("Error listening to storage server");
        exit(1);
    }
    // printf("Listening/Waiting to accept Storage Servers\n");
    int i = 0;
    pthread_t storageServerListenerThread[MAX_STORAGE_SERVERS];
    while (1)
    {
        int size = sizeof(ss[i].SSAddress);
        ss[i].SSSocket = accept(SS_Socket, (struct sockaddr *)&ss[i].SSAddress, &size);
        if (ss[i].SSSocket < 0)
        {
            printf("Error accepting storage server\n");
            log_entry("Error accepting storage server");
            exit(1);
        }
        printf("Connected to server\n");
        log_entry("Connected to server");
        if (i == MAX_STORAGE_SERVERS)
        {
            printf("Too many clients\n");
            log_entry("Too many clients");
            break;
        }
        else
        {
            pthread_t thread;
            tempserverID = i;
            if (pthread_create(&thread, NULL, handleServerInitialization, &ss[i].SSSocket))
            {
                perror("Error creating storage server thread");
                log_entry("Error creating storage server thread");
                exit(EXIT_FAILURE);
            }
            i++;
        }
    }
    // // Join the created threads
    // for (int j = 0; j < i; j++)
    // {
    //     pthread_join(storageServerListenerThread[j], NULL);
    // }
}

void *clientListener(void *args)
{
    int clientSocket = *(int *)args;

    if (listen(clientSocket, MAX_CLIENTS) < 0)
    {
        printf("Error listening to client\n");
        log_entry("Error listening to client");
        exit(1);
    }

    int i = 0;
    pthread_t clientListenerThread[MAX_CLIENTS];
    while (1)
    {
        int clientlen = sizeof(client[i].clientAddress);
        client[i].clientSocket = accept(clientSocket, (struct sockaddr *)&client[i].clientAddress, &clientlen);
        if (client[i].clientSocket < 0)
        {
            printf("Error accepting client\n");
            log_entry("Error accepting client");
            exit(1);
        }

        if (i == MAX_CLIENTS)
        {
            printf("Too many storage servers\n");
            log_entry("Too many storage servers");
            break;
        }
        else
        {
            pthread_t thread;
            clientTempServerID = i;
            if (pthread_create(&thread, NULL, handleClientRequests, (void *)&client[i]))
            {
                perror("Error creating client thread");
                log_entry("Error creating client thread");
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
    // Handling Incoming Storage Server Connection
    for (int i = 0; i < MAX_STORAGE_SERVERS; i++)
    {
        SSs[i].isAlive = -1;
    }
    numServersInit = 0;
    struct sockaddr_in SSAddress;
    int SSSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (SSSocket < 0)
    {
        printf("Error creating storage server socket\n");
        log_entry("Error creating storage server socket");
        exit(1);
    }

    SSAddress.sin_family = AF_INET;
    SSAddress.sin_port = htons(SS_PORT_NM);
    SSAddress.sin_addr.s_addr = INADDR_ANY;

    int SSBindStatus = bind(SSSocket, (struct sockaddr *)&SSAddress, sizeof(SSAddress));
    if (SSBindStatus < 0)
    {
        printf("Error binding storage server socket\n");
        log_entry("Error binding storage server socket");
        exit(1);
    }

    pthread_t SSListenerThread;
    if (pthread_create(&SSListenerThread, NULL, storageServerListener, (void *)&SSSocket))
    {
        perror("Error creating storage server thread");
        log_entry("Error creating storage server thread");
        exit(EXIT_FAILURE);
    }

    // pthread_mutex_lock(&mutex);
    // pthread_cond_wait(&initdone, &mutex);
    // pthread_mutex_unlock(&mutex);
    printf("Sorage Server Connections Initialized\n");
    log_entry("Sorage Server Connections Initialized");

    // Handling Incoming Client Connection
    struct sockaddr_in clientAddress;
    int clientSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (clientSocket < 0)
    {
        printf("Error creating client socket\n");
        log_entry("Error creating client socket");
        exit(1);
    }

    clientAddress.sin_family = AF_INET;
    clientAddress.sin_port = htons(CLIENT_PORT_NM);
    clientAddress.sin_addr.s_addr = INADDR_ANY;

    int clientBindStatus = bind(clientSocket, (struct sockaddr *)&clientAddress, sizeof(clientAddress));
    if (clientBindStatus < 0)
    {
        printf("Error binding client socket\n");
        log_entry("Error binding client socket");
        exit(1);
    }

    pthread_t ClientListenerThread;
    if (pthread_create(&ClientListenerThread, NULL, clientListener, (void *)&clientSocket))
    {
        perror("Error creating client thread");
        log_entry("Error creating client thread");
        exit(EXIT_FAILURE);
    }

    printf("Client Connections Initialized\n");
    log_entry("Client Connections Initialized");

    // pthread_mutex_lock(&mutex);
    // pthread_cond_wait(&initdone, &mutex);
    pthread_join(SSListenerThread, NULL);
    pthread_join(ClientListenerThread, NULL);
    // pthread_mutex_unlock(&mutex);

    printf("Exiting Naming Server\n");
    log_entry("Exiting Naming Server");
    close(SSSocket);
    close(clientSocket);

    return 0;
}
