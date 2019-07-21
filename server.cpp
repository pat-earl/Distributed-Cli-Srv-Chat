/******************************************************************
*	Author:				Patrick Earl
*	Filename:			server.cpp
*	Purpose:			LOOKUP_SERVER provides a directory to allow
*                   for clients to communicate with one another.
*
*                   Using readers writers alg to protect the 
*                   critcal section (In this case is the shared memory)
*******************************************************************/

/**
 * @file server.cpp
 * @author Patrick Earl 
 * @brief LOOKUP_SERVER provides a directory to allow for clients to communicate
 * @version 0.1
 * @date 2018-12-10
 * 
 */

// Includes
#include <iostream>
#include <bits/stdc++.h> 
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <stdlib.h>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <semaphore.h>
#include <vector>

// Namespace
using namespace std;

// Macros
#define MAX_CLIENTS         10              // Max clients the server can handle
// #define HOST_ADDRESS        "156.12.127.18" // Acad's IP Address
#define HOST_ADDRESS        "127.0.0.1"  // localhost
#define HOST_PORT           15020           // Port to get to ACAD through the firewall

#define CLIENT_PORT_MIN     15200
#define CLIENT_PORT_MAX     15224

#define NUM_SEM             2

// Prototypes 
void init_ports();
int get_port();

void abnormal_shutdown(int = 0);
void client_handler(int, sockaddr_in);
void print_client_msg(int, string);

int name_in_use(string);
void send_struct_info(string, int);
void send_connected_clients(int);

void reader_lock();
void writer_lock();
void reader_unlock();
void writer_unlock();

// SEM IDS: 
// 0 - Reader
// 1 - Writer

void P(int);
void V(int);

vector<string> str_split(string, char = ' ');
string str_combine(int, vector<string>, char = ' ');
string to_lower(string);
int send_msg(int, string);
string recv_msg(int);
void handle_sigint(int sig);


// Stuctures 

/**
 * @brief Stores information about a connected client
 * 
 */
struct clientStruct {
    char name[20];
    sockaddr_in serverAddr; 
    timeval startTime;
    // Time of most recent lookup
    timeval lastLookupTime;
    bool inUse;
};

/**
 * @brief Stores an array of struct clientStruct and the number of connected
 *      clients
 * 
 */
struct DIR {
    clientStruct clientInfo[MAX_CLIENTS];
    int numClients; // Total clients in System
};

/**
 * @brief Stores information about a client port (Is it in use?)
 * 
 */
struct CLIENT_PORT {
    int port;
    bool inuse;
};

/**
 * @brief Struct to hold an array of CLIENT_PORTS
 * 
 */
struct CLIENT_PORTS {
    CLIENT_PORT ports[CLIENT_PORT_MAX-CLIENT_PORT_MIN];
};

/**
 * @brief Used for creating semaphores on different machines
 * 
 *  @author Dr. Spiegel
 */
union semun {
    int val;
    semid_ds* buf;
    unsigned short *array; /* Array for GETALL, SETALL */
    #if defined(_linux_)
        seminfo *__buf; /* Buffer for IPC_INFO */
    #endif
};

// Global Variables
void *shmptr; /*<: The pointer to the overall shared memory */
int shmid; /*<: The ID for shared memory */
int semid; /*<: The ID for the semaphore set */
int *readerscnt; /*<: Pointer to the reader count */
DIR *shared_dir; /*<: The pointer to the struct DIR in shared mem */
CLIENT_PORTS *shared_port; /*<: The pointer for the struct CLIENT_PORT in shared mem */

int num_fds = 0;

// Main 
int main(int argc, char *argv[])
{
    // Variables 
    string temp;
    vector<string> splitter;

    // Shared memory stuff
    // void *shmptr; // Pointer for the shared memory struct

    // Socket stuff
    int svr_fd, cli_fd, clilen;
    sockaddr_in srv_addr, cli_addr;

    // Processes
    int pid;

    // Semaphores
    semid_ds sems;

    // Semaphore setup
    if((semid = semget(getuid(), NUM_SEM, IPC_CREAT|IPC_EXCL|0600)) < 0)
    {
        perror("ERROR: Semaphore exists");
        cerr << "Semaphore set exists already, clear using 'make clearmem' or" << 
        "kill all running servers" << endl;
        abnormal_shutdown();
    }

    // Handle the int
    signal(SIGINT, handle_sigint);

    // Shared memory setup
    if((shmid = shmget(getuid(), 
            sizeof(int) + sizeof(struct CLIENT_PORTS) + sizeof(struct DIR), 
            IPC_CREAT|IPC_EXCL|0600)) < 0)
    {
        perror("ERROR: Shared memory");
        cerr << "Shared memory exists already, clear using 'make clearmem' or" <<
        "kill all running servers" << endl;
        abnormal_shutdown(svr_fd);
    }

    // Attack to the created shared memory 
    shmptr = shmat(shmid, 0, 0);
    if(shmptr == (void *) -1) 
    {
        perror("ERROR: Shared Memory Attach");
        abnormal_shutdown(svr_fd);
    }


    bzero(shmptr, sizeof(shmptr));


    readerscnt = (int *)shmptr;
    shared_port = (CLIENT_PORTS *) ((void *)shmptr + sizeof(int));
    shared_dir = (DIR *) + ((void *)shmptr + sizeof(int) + sizeof(struct CLIENT_PORTS));

    // Set up port array
    init_ports();


    // Create socket and setup listener (Big Ear)    
    if((svr_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("ERROR: Socket create");
        abnormal_shutdown(svr_fd);
    }
    num_fds += 1;

    srv_addr.sin_family = AF_INET;                  // AF_INET
    srv_addr.sin_addr.s_addr = inet_addr(HOST_ADDRESS);   // Listen on any address (convert to net-long)
    srv_addr.sin_port = htons(HOST_PORT);           // Convert port to net-short

    if(bind(svr_fd, (sockaddr *) &srv_addr, sizeof(srv_addr)) < 0)
    {
        perror("ERROR: Bind");
        abnormal_shutdown(svr_fd);
    }

    listen(svr_fd, 5);

    // Acceptor loop and forking location
    cout << "> LOOKUP-SERVER: Now accepting clients" << endl;
    while(true)  // Loop until the end of time
    {
        clilen = sizeof(cli_addr);
        if((cli_fd = accept(svr_fd, (sockaddr *) &cli_addr, (socklen_t *)&clilen)) < 0)
        {
            perror("ERROR: Socket Accept");
            abnormal_shutdown(svr_fd);
        }
        num_fds += 1; 

        if((pid = fork()) < 0)
        {
            perror("ERROR: Client fork");
            abnormal_shutdown(svr_fd);
        }
        else if(pid == 0) /* New client */
        {
            // If server is full, inform client and disconnect client
            if(shared_dir->numClients == 10)
            {
                send_msg(cli_fd, "SERVER FULL, please try again later!");
                close(cli_fd);
                exit(0);
            }  
            client_handler(cli_fd, cli_addr);
            break;
        }
        else /* Parent */ 
        {
        }
    }

    close(svr_fd);
    shmctl(shmid, IPC_RMID, NULL);
    semctl(semid, 0, IPC_RMID, 0);
    return 0;
}

/**
 * @brief Initalizes the ports for the server 
 * 
 */
void init_ports() 
{
    int port = CLIENT_PORT_MIN;
    for(int i = 0; i < (CLIENT_PORT_MAX - CLIENT_PORT_MIN); i++)
    {
        static_cast<CLIENT_PORTS *>(shared_port)->ports[i].port = htons(port);
        static_cast<CLIENT_PORTS *>(shared_port)->ports[i].inuse = false;
        port++;
    }
}

/**
 * @brief Returns an available port
 * 
 * @return int - Returns a port if free, -1 if no port free
 */
int get_port() 
{
    CLIENT_PORTS *ports = static_cast<CLIENT_PORTS *>(shared_port);
    writer_lock();
    for(int i = 0; i < (CLIENT_PORT_MAX - CLIENT_PORT_MIN); i++)
    {
        if(ports->ports[i].inuse == false)
        {
            ports->ports[i].inuse = true;
            writer_unlock();
            return ports->ports[i].port;
        }
    }
    writer_unlock();
    return -1;
}

/**
 * @brief Frees up the port
 * 
 * @param port The port to free up
 */
void free_port(int port)
{
    CLIENT_PORTS *ports = static_cast<CLIENT_PORTS *>(shared_port);
    
    writer_lock();
    for(int i = 0; i < (CLIENT_PORT_MAX - CLIENT_PORT_MIN); i++)
    {
        if(ports->ports[i].inuse == true && ports->ports[i].port == port)
        {
            ports->ports[i].inuse == false;
        }
    }
    writer_unlock();
}

// Client stuff'
/**
 * @brief Where a child lookup_server  oes to handle the client
 * 
 * @param cli_fd - The socket to talk to the client
 * @param cli_sock  - The client's sockaddr_in returned from accept
 */
void client_handler(int cli_fd, sockaddr_in cli_sock)
{
    // Begin client setup
    int clientid = -1;
    string buf;
    vector<string> splitter; 
    DIR *dirs = static_cast<DIR *>(shared_dir);

    string msg = recv_msg(cli_fd);    

    splitter = str_split(msg);


    // Make sure the format is followed:
    // "HELLO <nickname>" 
    if(splitter[0].compare("HELLO"))
    {
        print_client_msg(cli_fd, "Invalid setup message!");
        send_msg(cli_fd, "ERROR INVALID COMMAND!");
        shmdt(shmptr);
        close(cli_fd);
        exit(0);
    }

    // Check nickname isn't already in use and isn't too long
    // Limit the name length
    if(splitter[1].length() > 20)
    {
        print_client_msg(cli_fd, "Nickname too long");
        send_msg(cli_fd, "ERROR Nickname too long, please try again");
        shmdt(shmptr);
        close(cli_fd);
        exit(0);
    }

    if(name_in_use(splitter[1]) > -1) 
    {
        print_client_msg(cli_fd, "Nickname used");
        send_msg(cli_fd, "ERROR Nickname in use, please try another");
        shmdt(shmptr);
        close(cli_fd);
        exit(0);
    } 

    writer_lock();
    dirs->numClients += 1;

    for(int i = 0; i < MAX_CLIENTS; i++) 
    {
        if(dirs->clientInfo[i].inUse == false) 
        {
            clientid = i;
            dirs->clientInfo[i].inUse = true;
            strcpy(dirs->clientInfo[i].name, splitter[1].c_str());
            dirs->clientInfo[i].serverAddr = cli_sock;
            break;
        }
    }
    writer_unlock();

    // Just double check a client id was assigned
    if(clientid == -1)
    {
        print_client_msg(cli_fd, "Shared Memory Full? All clients in use");
        send_msg(cli_fd, "ERROR This error shouldn't occur. Shared memory claims all clients in use");
        shmdt(shmptr);
        close(cli_fd);
        exit(0);
    }

    print_client_msg(clientid, "CLIENT ACCEPTED!");
    send_msg(cli_fd, "ACCEPTED");

    // Wait for client request
    splitter = str_split(recv_msg(cli_fd));
    if(splitter[0].compare("UDP"))
    {
        print_client_msg(clientid, "CLIENT PROTOCOL ERROR: Didn't request UDP setup");
        send_msg(cli_fd, "ERROR 'UDP' was next expected message to continue setup");
        shmdt(shmptr);
        close(cli_fd);
        exit(0);
    }

    int port = get_port();
    if(port == -1) 
    {
        print_client_msg(clientid, "CLIENT PORT ISSUE: All ports in use?");
        send_msg(cli_fd, "ERROR All ports are in use...");
        shmdt(shmptr);
        close(cli_fd);
        exit(0);
    }

    char c_port[16];
    sprintf(c_port, "%d", port);
    send_msg(cli_fd, "PORT "+ string(c_port));

    // Loop until client is able to bind it's port
    while((msg = recv_msg(cli_fd)) != "READY")
    {
        int prev_port = ntohs(port); // Temp storage
        port = get_port(); // Get a new one
        free_port(prev_port); // Release the old one
        if(port == -1) 
        {
            print_client_msg(clientid, "CLIENT PORT ISSUE: All ports in use?");
            send_msg(cli_fd, "ERROR All ports are in use...");
            shmdt(shmptr);
            close(cli_fd);
            exit(0);
        }

        char c_port[16];
        sprintf(c_port, "%d", port);
        send_msg(cli_fd, "PORT "+ string(c_port));
    }
    writer_lock();
    dirs->clientInfo[clientid].serverAddr.sin_port = port;
    writer_unlock();

    // Loop until client exits
    bool end_client = false;
    while(!end_client)
    {
        splitter = str_split(recv_msg(cli_fd));
        // Client sent the quit command
        if(to_lower(splitter[0]).compare("quit") == 0)
        {
            print_client_msg(clientid, "Client quit");
            end_client = true;
        }

        // Client trying to message
        else if(to_lower(splitter[0]).compare("msg") == 0)
        {
            send_struct_info(splitter[1], cli_fd);
        }

        else if(to_lower(splitter[0]).compare("all") == 0)
        {
            send_connected_clients(cli_fd);
        }

        // Socket closed or something else
        else if(splitter.empty()) 
        {
            print_client_msg(clientid, "Client closed unexpectantly");
            end_client = true;
        }
    }

    // TODO: Client clean up
    writer_lock();
    dirs->clientInfo[clientid].inUse = false;
    dirs->numClients -= 1;
    writer_unlock();

    shmdt(shmptr);
    close(cli_fd);
    exit(0);
}

/**
 * @brief Checks if a name is use. 
 * 
 * @param name - The name to check
 * @return int 
 *      - Returns the index in DIRS->clientInfo that has the nextname
 *      - Returns -1 if name is not found
 */
int name_in_use(string name)
{
    string temp;
    reader_lock();
    for(int i = 0; i < MAX_CLIENTS; i++) 
    {
        if(static_cast<DIR *>(shared_dir)->clientInfo[i].inUse)
        {
            temp = static_cast<DIR *>(shared_dir)->clientInfo[i].name;
            if(to_lower(name).compare(to_lower(temp)) == 0) 
            {
                reader_unlock();
                return i;
            }
        }
    }
    reader_unlock();
    return -1;
}

/**
 * @brief When a client wants to send a function, this function will send over 
 *      the needed information for the client to so
 * 
 * @param nick - The nickname to find
 * @param cli  - The client socket to work with
 */
void send_struct_info(string nick, int cli)
{
    sockaddr_in temp;
    // Get the client id
    char buf[INET_ADDRSTRLEN];
    int clientid = name_in_use(nick);
    char port[16];

    if(clientid == -1)
    {
        send_msg(cli, "ERROR Invalid nickname");
        return;
    }

    reader_lock();
    temp = static_cast<DIR *>(shared_dir)->clientInfo[clientid].serverAddr;
    reader_unlock();

    inet_ntop(AF_INET, &temp.sin_addr.s_addr, buf, sizeof(buf));
    send_msg(cli, "HOST " + string(buf));
    usleep(5000);
    sprintf(port, "%i", temp.sin_port);
    send_msg(cli, "PORT " + string(port));

}

/**
 * @brief Sends a list of the connected clients in LOOKUP_SERVER
 * 
 * @param cli_fd - The client fd asking for the information
 */
void send_connected_clients(int cli_fd)
{
    string message = "Connected Clients:\n";
    DIR *dirs = static_cast<DIR *>(shared_dir);
    
    reader_lock();
    for(int i = 0; i < MAX_CLIENTS; i++)
    {   
        if(dirs->clientInfo[i].inUse == true)
        {
            message += '\t';
            message += dirs->clientInfo[i].name;
            message += '\n';
        }
    }
    reader_unlock();
    send_msg(cli_fd, "LIST " + message);
}

/**
 * @brief Used to print a message from a specfic child 
 * 
 * @param clientid - The index in DIR
 * @param msg - The message to print
 */
void print_client_msg(int clientid, string msg) 
{
    cout << "Client " << clientid << ":" << endl;
    cout << msg << endl;
}

/**
 * @brief Used to shutdown the server when something unexpected has happened
 * 
 * @param soc Pass the socket for the TCP server
 */
void abnormal_shutdown(int soc) {
    if(shmid != 0) {
        shmdt(shmptr);
        shmctl(shmid, IPC_RMID, NULL); // Delete the shared memory 
    }
    if(semid != 0) {
        semctl(semid, 0, IPC_RMID, NULL);
    }
    if(soc != 0)
        close(soc); // Close the server socket
    
    
    exit(EXIT_FAILURE); // Exit the program with a failure code
}

/**
 * @brief Handles the event handler
 * 
 * @param sig - Which singal was passed
 */
void handle_sigint(int sig)
{
    if(sig == SIGINT)
    {
        // Close out all fd (sockets in this case)
        for(int i = 3; i < num_fds+3; i++)
        {
            cout << "i: " << i << endl;
            close(i);
        }
        shmdt(shmptr);
        shmctl(shmid, IPC_RMID, NULL);
        semctl(semid, 0, IPC_RMID, 0);
        exit(0);
    }
}

// Semaphores
/**
 * @brief Looks the CS to allow for a reader to access it
 *      Locks writers from accessing
 * 
 */
void reader_lock()
{
    int *cnt = static_cast<int *>(readerscnt);
    P(0); // Lock the reader sem
    
    (*cnt)++;
    if(*cnt == 1)
        P(1); // Lock out the writers
    V(0);
}

/**
 * @brief Unlocks the CS after a reader gets it
 *      Unlocks writers if last reader 
 */
void reader_unlock()
{
    int *cnt = static_cast<int *>(readerscnt);

    P(0); // lock the reader sem

    (*cnt)--;
    if(*cnt == 0)
        V(1); // Writers can access it again

    V(0);
}

/**
 * @brief Locks the CS for writers
 * 
 */
void writer_lock()
{
    P(1); // Get the writer lock
}

/**
 * @brief Unlocks the CS for the writers
 * 
 */
void writer_unlock()
{
    V(1); // Unlock the writer
}

/**
 * @brief Grabs the lock for the semaphore 
 * 
 * @param id - Which semaphore to lock
 */
void P(int id)
{
    sembuf sem_lock = {id, -1, 0};
    semop(semid, &sem_lock, sizeof(struct sembuf));
}

/**
 * @brief Lets the lock go for the semaphore 
 * 
 * @param id - Which semaphore to let go 
 */
void V(int id) 
{
    sembuf sem_unlock = {id, 1, 0};
    semop(semid, &sem_unlock, sizeof(struct sembuf));
}

// Util functions
/**
 * @brief Takes a string and splits into a string vector
 * 
 * @param str - The string to split
 * @param token - The token to split on
 *          Default: ' '
 * @return vector<string> - The split string 
 */
vector<string> str_split(string str, char token)
{
    vector<string> rtn;
    int pos;

    if(str.empty())
    {
        return rtn;
    }

    // Loop until no spaces are found
    while((pos = str.find(token)) != string::npos)
    {
        rtn.push_back(str.substr(0, pos)); // Push the word into the vector
        str.erase(0, pos+1); // Erase it from the string
    }
    // Once here, either the string has no spaces or at the last word
    rtn.push_back(str);
    return rtn; // Return the vector
}

/**
 * @brief Takes a split vector string and combines it back together
 * 
 * @param pos The position to start combining the vector at
 * @param vec - The vector to combine
 * @param token - The token to join on
 * @return string - The combined string
 */
string str_combine(int pos, vector<string> vec, char token)
{
    string rtn;
    // Start loop at given position and put the string back together
    for(int i = pos; i < vec.size(); i++) 
        rtn.append(vec[i] + token);
    return rtn;
}

/**
 * @brief Takes a string and makes it all lower case
 * 
 * @param str - The string
 * @return string - The lowercase string
 */
string to_lower(string str) 
{
    for(int i = 0; i < str.length(); i++) 
       str[i] = tolower(str[i]);
    return str;
}

// Message Sending

/**
 * @brief Used to send a message to a client | Creates a crappy form of a packet
 * 
 * @param sockid - The sock to send on
 * @param msg - The message to send
 * @return int - The result of the send
 */
int send_msg(int sockid, string msg) 
{
    msg.append("||");
    int len = msg.length();
    return send(sockid, msg.c_str(), len, 0);
}

/**
 * @brief Recieves a message from a client - Handles the same packet
 * 
 * @param sockid - The socket to recieve on
 * @return string - The message recieved from the client
 */
string recv_msg(int sockid)
{
    char buf[512];
    string msg = "";
    int recved;
    int str_len = 0;

    // Read messages until NULL
    while((recved = recv(sockid, buf, sizeof(buf), 0)) > 0)
    {
        str_len += recved;
        msg.append(buf);
        if((msg[str_len-2] == '|') && (msg[str_len-1] == '|')) {
            msg.erase(str_len-2, msg.length());
            return msg;
        }
    }

    // Sock closed
    if(recved == -1)
        return "";

    // Check that the tailing "||" was recieved 
    if((msg[str_len-2] != '|') && (msg[str_len-1] != '|'))
        return "";

    // Remove the tailing "||" and garbage
    msg.erase(str_len-2, msg.length());

    // Return the message
    return msg;
}