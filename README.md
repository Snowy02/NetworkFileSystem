
# FINAL PROJECT: NETWORK FILE SYSTEM


## Team Information

**Team Number:** 72

**Team Members:**
- **Gaurav Bhole**
  - *Student ID:* 2021113015

- **Chaitrika**
  - *Student ID:* 2022101076

- **Aditya Kulkarni**
  - *Student ID:* 2023121005




## PROJECT OVERVIEW

The Network File System (NFS) is a sophisticated distributed file system designed to streamline file sharing and access across a network of interconnected clients. This project meticulously implements a robust and scalable NFS architecture, consisting of key components:

1. **Naming Server (NM):**
   - Functions as the central coordination point, managing the directory structure and crucial information about file locations.
   - Orchestrates seamless communication between clients and storage servers, ensuring precise and efficient file access.

2. **Storage Servers (SS):**
   - Form the cornerstone of the NFS by overseeing the physical storage and retrieval of files and folders.
   - Dynamically add entries to the Naming Server, allowing the system to adapt to changes with seamless integration.

3. **Clients:**
   - Represent the diverse systems or users seeking file access within the network file system.
   - Interact with the NFS through the Naming Server, initiating a spectrum of file-related operations, including reading, writing, creating, deleting, and copying files.

This implementation offers a dynamic and efficient file management solution, empowering users with a comprehensive suite of file operations.


## SUPPORTED OPERATIONS

### File Operations:

1. **Write (File/Folder):**
   - Clients can actively create and update the content of files and folders within the NFS, ensuring a dynamic repository.

2. **Read (File):**
   - Reading operations empower clients to retrieve the contents of files stored within the NFS, granting seamless data access.

3. **Delete (File/Folder):**
   - Clients retain the ability to remove files and folders from the network file system, contributing to efficient space management.

4. **Create (File/Folder):**
   - The NFS allows clients to generate new files and folders, facilitating the expansion and organization of the file system.

5. **List (All Files and Folders in a Folder):**
   - Navigating the NFS structure becomes effortless for clients as they can retrieve comprehensive listings of files and subfolders within a specified directory.

6. **Get Additional Information:**
   - Clients can access a wealth of supplementary information about specific files, including details such as file size, access rights, timestamps, and other metadata.
## FUNCTIONS IMPLEMENTED

### Naming Server (NM):

#### Request Handling:

- `void *ackChecker(void *arg);`
- `void *throbbingHeart(void *arg);`
- `void *clientListener(void *args);`
- `void *handleClientRequests(void *arg);`
- `void *storageServerListener(void *args);`
- `void *handleServerInitialization(void *fd);`

#### File Operations:

- `void SBI_CREATE_DELETE(char *Operation, char *filePath);`
- `void SBI_READ_WRITE_GETINFO(char *Operation, char *filePath);`
- `void SBI_COPY(char *Operation, char *srcPath, char *destPath);`

### Hashing Part of Naming Server (NM):

- `int getHash(char *path);`
- `void destroyHashTable(PtrToNode nodePtr);`
- `int findPath(PtrToNode *hashTable, char *path);`
- `void createHashTable(PtrToNode *hashTable, char *path);`
- `void updateHashServer(struct HashTable *hashTable, struct storageServer *SSData);`
- `void initializeHashServer(struct HashTable *hashTable, struct storageServer *SSData);`

### Storage Server (SS):

- `void *handle_CREATEDIR(void *arg);`
- `void *handle_DELETEDIR(void *arg);`
- `void *handle_CREATEFILE(void *arg);`
- `void *handle_DELETEFILE(void *arg);`
- `void *nm_request_handler(void *arg);`
- `void *client_request_handler(void *arg);`
- `void handle_rwp(int nm_socket, char *req);`
- `void tree_search(char *basepath, struct storageServer *server);`
- `void initializeStorageServer(struct storageServer storage_server, int serverID);`

### Client:

- `void *client_thread(void *arg);`
- `void display();`
- `void get_file_path(char *file_path, int size);`
- `int create_file(int client_port, char *file_path, char *response);`
- `int delete_file(int client_port, char *file_path, char *response);`
- `int delete_directory(int client_port, char *dir_path, char *response);`
- `int create_directory(int client_port, char *dir_path, char *response);`
- `int copy_file(int client_port, char *source_path, char *destination_path, char *response);`
- `int copy_directory(int client_port, char *source_path, char *destination_path, char *response);`
- `int communicate_with_ss(char *ss_ip, int ss_port, char *request, char *response);`
- `int communicate_with_nm(char *request, char *filePath, char *ss_ip, int *ss_port);`
- `int create_delete_copy_file(int client_port, char *action, char *path, char *response);`
- `int communicate_with_nm_and_recv(int client_port, char *filePath, char *request, char *response);`
- `int communicate_with_nm_and_recv_copy(int client_port, char *srcPath, char *destPath, char *request, char *response);`
## OTHER KEY IMPORTANT FEATURES

1. **ERROR CODES:**
   The NFS employs a comprehensive system of clear and descriptive error codes to facilitate effective communication of issues with users. Specific error codes are defined for various scenarios, enhancing communication between the NFS and clients. Here is a list of some error codes:

- **BADREAD:** "ERR BADREAD: Error reading file"
- **SENDERR:** "ERR SENDERR: Error sending file content"
- **RECVERR:** "ERR RECVERR: Error receiving data"
- **FOPENERR:** "ERR FOPENERR: Error opening file"
- **GETFINFO:** "ERR GETFINFO: Error getting file information"
- **SENDFINFO:** "ERR SENDFINFO: Error sending file information"


2. **BOOKKEEPING:**
   The NFS incorporates robust bookkeeping features, meticulously logging every request and acknowledgment from clients and Storage Servers. Detailed logs include IP addresses and ports, providing thorough traceability for debugging and system monitoring.

3. **MULTIPLE CLIENTS:**
   - **Concurrent Access:**
     The NFS ensures seamless concurrent access for multiple clients with an efficient acknowledgment mechanism, ensuring a smooth user experience.
   - **Read and Write Control:**
     While multiple clients can read the same file simultaneously, exclusive control is maintained for write operations. Only one client at a time is allowed to modify a file.

4. **EFFICIENT SEARCH IN NAMING SERVERS:**
   The Naming Server optimizes the search process when serving client requests. By leveraging efficient data structures such as Hashmaps, the NFS swiftly identifies the correct Storage Server (SS) for a given request. This optimization significantly enhances response times, particularly in systems with a large number of files and folders.
