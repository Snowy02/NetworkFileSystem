#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
// request types ie send these types of messages b4 senging info eg. if you want to initalize servers first send INIT and then the details of the server
#define READ "READ"
#define WRITE "WRITE"
#define COPYDIR "COPY_D"
#define COPYFILE "COPY_F"
#define GETINFO "GET_INFO"
#define CREATEDIR "CREATE_D"
#define DELETEDIR "DELETE_D"
#define CREATEFILE "CREATE_F"
#define DELETEFILE "DELETE_F"
#define PERMISSION "PERMISSION"

#define SSAliveSignal -7 // heartbeat message
#define NM_IP "127.0.0.1"

#define MAX_PATHS 25
#define INITSERVERS 4 // Initial number of servers before clients starts sending requests
#define MAX_CLIENTS 10
// #define MAX_HASH_KEY 10
#define TIMEOUT_SECONDS 10
#define MAX_NUM_PATHS 1000
#define MAX_PATH_LENGTH 512
#define MAX_BUFFER_SIZE 1024
#define MAX_CLIENT_THREADS 10
#define MAX_STORAGE_SERVERS 10

#define NM_PORT 9000        // Naming Server Port
#define SS_PORT_NM 9001     // Naming Server Port for Storage Servers
#define CLIENT_PORT_NM 9002 // Naming Server Port for Clients
#define CLIENT_PORT_SS 9003 // Storage Server Port for Clients

// #define NM_PORT 9090        // Naming Server Port
// #define SS_PORT_NM 9091     // Naming Server Port for Storage Servers
// #define CLIENT_PORT_NM 9092 // Naming Server Port for Clients
// #define CLIENT_PORT_SS 9093 // Storage Server Port for Clients

typedef struct SS SS;
typedef struct Client Client;
typedef struct Node *PtrToNode;
typedef struct HashTable HashTable;

struct Node // node for linked list for hash table
{
    char *path;
    struct Node *next;
};

struct storageServer
{
    int port_nm;
    int isAlive;
    int serverID;
    int numFiles;
    int port_client;
    int numDirectories;
    char ip_address[50];
    char currentDirectory[MAX_PATH_LENGTH];
    char files[MAX_PATHS][MAX_PATH_LENGTH];
    char directory[MAX_PATHS][MAX_PATH_LENGTH];
};

struct HashTable
{
    PtrToNode files[MAX_STORAGE_SERVERS];
    PtrToNode directory[MAX_STORAGE_SERVERS];
    struct storageServer SSData; // data of storage servers
};

struct SS
{
    int SSSocket;                 // socket descriptor
    struct sockaddr_in SSAddress; // client address
};

struct Client
{
    int clientSocket;                 // socket descriptor
    int port;                         // client port
    struct sockaddr_in clientAddress; // client address
};

struct Request
{
    int socket;
    char buffer[MAX_BUFFER_SIZE];
    struct storageServer *storage_server;
};

struct FileInfo
{
    off_t size;         // File size
    mode_t permissions; // File permissions
};

// Hashing (Part of Naming Server)
int getHash(char *path);
void destroyHashTable(PtrToNode nodePtr);
int findPath(struct Node **hashtable, char *path);
void createHashTable(PtrToNode *hashTable, char *path);
void updateHashServer(struct HashTable *hashTable, struct storageServer *SSData);
void initializeHashServer(struct HashTable *hashTable, struct storageServer *SSData);

// Naming Server
void stripper(char *path);
void *ackChecker(void *arg);
void *throbbingHeart(void *arg);
void *clientListener(void *args);
void *handleClientRequests(void *arg);
void *storageServerListener(void *args);
void *handleServerInitialization(void *fd);
void SBI_CREATE_DELETE(char *Operation, char *filePath);
void SBI_READ_WRITE_GETINFO(char *Operation, char *filePath);
void SBI_COPY(char *Operation, char *srcPath, char *destPath);

//log part
#define LOG_FILE "log.txt"
void get_timestamp(char *timestamp, size_t size);
void log_entry(const char *message);

// Client
void display();
void *client_thread(void *arg);
int create_file(int client_port, char *file_path, char *response);
int delete_file(int client_port, char *file_path, char *response);
int delete_directory(int client_port, char *dir_path, char *response);
int create_directory(int client_port, char *dir_path, char *response);
int communicate_with_nm(char *request, char *filePath, char *ss_ip, int *ss_port);
int create_delete_copy_file(int client_port, char *action, char *path, char *response);
int copy_file(int client_port, char *source_path, char *destination_path, char *response);
int communicate_with_ss(char *ss_ip, int ss_port, char *request, char *file_path, int ack);
int copy_directory(int client_port, char *source_path, char *destination_path, char *response);
int communicate_with_nm_and_recv(int client_port, char *filePath, char *request, char *response);
int communicate_with_nm_and_recv_copy(int client_port, char *srcPath, char *destPath, char *request, char *response);

// Storage Server
void getfulldir(char *dirname);
void *handle_CREATEDIR(void *arg);
void *handle_DELETEDIR(void *arg);
void *handle_CREATEFILE(void *arg);
void *handle_DELETEFILE(void *arg);
void *nm_request_handler(void *arg);
void *client_request_handler(void *arg);
void get_file_path(char *file_path, int size);
void handle_rwp(int nm_socket, char *req);
void tree_search(char *basepath, struct storageServer *server);
void initializeStorageServer(struct storageServer storage_server, int serverID);

// global variables for Naming Server
SS ss[MAX_STORAGE_SERVERS];
Client client[MAX_CLIENTS];
HashTable hashedData[MAX_STORAGE_SERVERS];
struct storageServer SSs[MAX_STORAGE_SERVERS];

sem_t joinLocks[MAX_STORAGE_SERVERS];
sem_t writeLocks[MAX_STORAGE_SERVERS];

int numServersInit;
int isJoined[MAX_STORAGE_SERVERS];

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;  // mutex for naming server, which is used to lock the naming server
pthread_cond_t initnext = PTHREAD_COND_INITIALIZER; // condition variable for naming server, which is used to signal the next server to initialize
pthread_cond_t initdone = PTHREAD_COND_INITIALIZER; // condition variable for naming server, which is used to signal that all servers have been initialized

// global variables for Storage Server
struct ThreadArgs1
{
    int nm_socket;
    char filepath[MAX_PATH_LENGTH];
    struct storageServer *storage_server;
};

sem_t thread_semaphore;
pthread_t client_threads1[MAX_CLIENT_THREADS]; // Array to hold client threads
pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;



// Error codes:
#define BADREAD "ERR BADREAD: Error reading file"
#define SENDERR "ERR SENDERR: Error sending file content"
#define RECVERR "ERR RECVERR: Error receiving data"
#define FOPENERR "ERR FOPENERR: Error opening file"
#define GETFINFO "ERR GETFINFO: Error getting file information"
#define SENDFINFO "ERR SENDFINFO: Error sending file information"

