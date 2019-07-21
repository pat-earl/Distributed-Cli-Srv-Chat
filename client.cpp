/******************************************************************
*	Author:				Patrick Earl
*	Filename:			client.cpp
*	Purpose:			Allow for communication between clients and
*               connecting to server to prepare for client2client 
*               communications
*******************************************************************/

/**
 * @file client.cpp
 * @author Patrick Earl
 * @brief Allow for communication between clients and
 *          connecting to server to prepare for client2client 
 *          communications
 * @version 0.1
 * @date 2018-12-10
 * 
 */

// Includes
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <arpa/inet.h>
#include <termios.h>
#include <semaphore.h>
#include <vector>

// Namespace
using namespace std; 

// Macros
#define MAX_CLIENTS     5               // Max clients on a local machine
// #define LOOKUP_SERVER   "156.12.127.18" // Acad's IP address 
#define LOOKUP_SERVER   "127.0.0.1"     // Localhost
#define LOOKUP_SRV_PORT 15020

// Prototypes
void client_messages(int, int);
void send_client_message(int, int, string, string);

    // From "util.cpp"
int send_msg(int, string, const sockaddr_in* = NULL);
string recv_msg(int);

vector<string> str_split(string, char = ' ');
string str_combine(int, vector<string>, char = ' ');
string to_lower(string);

// Global vars
sockaddr_in udp_server; 
int shmid;      // Shared memory fd
void *shmptr;   // Shared memory ptr
string nickname;

// Stucts
/**
 * @brief The struct that was going to be used for client message
 * 
 */
struct LOCAL_INFO 
{
    string Name;
    timeval startTime;
    timeval lastMsgTime;
    int numMsg;
    pid_t pid;
};

/**
 * @brief The struct that was goning to be used to store an array of LOCAL_INFO
 * 
 */
struct LOCAL_DIR 
{
    LOCAL_INFO localInfo[MAX_CLIENTS];
    int numClients;
    int totalMsgs;
};


// Main
int main(int argc, char *argv[])
{
    // Variables

    // Sockets
    int cli_fd, srv_fd;
    sockaddr_in lookup_sock, listen_sock;

    string temp;
    vector<string> splitter;


    // Getting the nickname
    if(argc < 2) 
    {
        cout << "ERROR: Please pass a nickname for the client" << endl;
        cout << "Usage: " << argv[0] << " [nickname]" << endl;
        exit(EXIT_SUCCESS);
    }

    temp = argv[1];
    
    if((to_lower(temp).compare("list") == 0) || 
                (to_lower(temp).compare("all") == 0) || 
                (to_lower(temp).compare("quit") == 0))
    {
        cerr << "Sorry you can't use a keyword as a nickname" << endl;
        cerr << "Please try again" << endl;
        exit(EXIT_SUCCESS);
    }


    // Socket creation
    if((cli_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("ERROR: TCP Socket Creation");
        exit(EXIT_FAILURE);
    }

    if((srv_fd = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
    {
        perror("ERROR UDP Socket Creation");
        close(cli_fd);
        exit(EXIT_FAILURE);
    }

    // Set up the sockets
    lookup_sock.sin_family = AF_INET;
    lookup_sock.sin_addr.s_addr = inet_addr(LOOKUP_SERVER);
    lookup_sock.sin_port = htons(LOOKUP_SRV_PORT);
    
    // Port gets configured later
    udp_server.sin_family = AF_INET;
    udp_server.sin_addr.s_addr = htonl(INADDR_ANY);

    if(connect(cli_fd, (sockaddr *) &lookup_sock, sizeof(lookup_sock)) < 0)
    {
        perror("ERROR: Client connect");
        close(cli_fd);
        close(srv_fd);
        exit(EXIT_FAILURE);
    }

    send_msg(cli_fd, "HELLO " + temp);

    // Make sure client was accepted
    temp = recv_msg(cli_fd);
    if(temp.compare("ACCEPTED"))
    {
        // Error handling
        cout << temp << endl;
        close(cli_fd);
        close(srv_fd);
        exit(EXIT_FAILURE);
    }
    nickname = argv[1];

    // Get the UDP server sockaddr
    send_msg(cli_fd, "UDP");

    // Get port assigned from server and store it in 
    
    // Set up the UDP server
    bool binded = false;
    while(!binded) 
    {
        splitter = str_split(recv_msg(cli_fd));

        if(splitter[0] == "")
        {
            cerr << "No response from server, shutting down\n";
            close(cli_fd);
            exit(EXIT_FAILURE);
        }
        else if(splitter[0].compare("PORT"))
        {
            // Error Handling
            cout << str_combine(1, splitter) << endl;
            close(cli_fd);
            exit(EXIT_FAILURE);
        }

        int port = atoi(splitter[1].c_str());

        // ? This shouldn't need to be htons, check back later;
        udp_server.sin_port = ntohs(port);

        // Attempt to bind
        if(bind(srv_fd, (sockaddr *)&udp_server, sizeof(udp_server)) < 0)
        {
            if(errno == EADDRINUSE) // Address given is in use, try another
            {
                send_msg(cli_fd, "UDP");
                continue;
            }
            else // Some other error occured 
            {
                send_msg(cli_fd, "EXIT");
                perror("ERROR: UDP Bind");
                close(cli_fd);
                close(srv_fd);
                exit(EXIT_FAILURE);
            }
        }

        send_msg(cli_fd, "READY");
        binded = true;
    }

    client_messages(cli_fd, srv_fd);

    close(cli_fd);
    close(srv_fd);
    return 0;
}

/**
 * @brief Sets up select and handles all communication for the client
 * 
 * @param cli - The socket to talk to LOOKUP_SERVER
 * @param srv - The socket to listen on for UDP messages from other clients
 */
void client_messages(int cli, int srv)
{
    // Taken from termdemo.c
    int UserRead, RetVal;
    fd_set fdvar; 
    FILE *fp; 
    bool done = false;
    vector<string> splitter;
    string temp;
    char buf[128];

    // termios term, termsave;
    // fp = fopen(ctermid(NULL), "r+");
    // setbuf(fp, NULL);
    // UserRead = fileno(fp);

    while(!done) 
    {
        write(1, "> ", 2);

        FD_ZERO(&fdvar);
        FD_SET(0, &fdvar);
        FD_SET(cli, &fdvar);
        FD_SET(srv, &fdvar);

        // tcgetattr(UserRead, &termsave);
        // term = termsave;

        // term.c_lflag &= (ECHO);

        // tcsetattr(UserRead, TCSANOW, &term);
        select(32, &fdvar, (fd_set *)0, (fd_set *)0, NULL);
        // tcsetattr(UserRead, TCSADRAIN, &termsave);

        // UDP SERVER RECV
        if(FD_ISSET(srv, &fdvar))
        {
            splitter = str_split(recv_msg(srv));
            if(splitter[0] == "")
            {
                cout << "UDP SERVER CLOSED" << endl;
                close(srv);
                close(cli);
                exit(EXIT_FAILURE);
            }
            cout << "Message from " << splitter[0] << endl;
            cout << str_combine(1, splitter) << endl;
        }

        // MESSAGES FROM LOOKUP_SERVER
        if(FD_ISSET(cli, &fdvar))
        {
            splitter = str_split(recv_msg(cli));
            if(splitter[0] == "")
            {
                cout << "LOOKUP_SERVER close down" << endl;
                close(srv);
                close(cli);
                exit(EXIT_FAILURE);
            }

            if(splitter[0].compare("LIST") == 0)
            {
                cout << str_combine(1, splitter) << endl;
            }
        }

        // STDIN
        if(FD_ISSET(0, &fdvar))
        {
            if((RetVal = read(0, buf, sizeof(buf))) < 0)
            {
                fprintf(stderr, "Stdin: %d chars -- %s\n", RetVal, buf);
                write(1, buf, RetVal);
                sleep(1);
            }
            buf[RetVal-1] = '\0';
            splitter = str_split(string(buf));
            if((to_lower(splitter[0]).compare("quit")) == 0)
            {   
                send_msg(cli, "quit");
                sleep(1);
                done = true;
            }
            // user hit cntl-c?
            else if(RetVal == -1)
            {
                send_msg(cli, "quit");
                sleep(1);
                done = true;
            }

            else if((to_lower(splitter[0]).compare("list")) == 0)
            {
                cout << "Sorry this feature isn't implemented" << endl;
            }
            else if((to_lower(splitter[0]).compare("all")) == 0)
            {
                send_msg(cli, "all");
            }

            // User is sending a message
            else 
            {
                send_client_message(cli, srv, splitter[0], str_combine(1, splitter));
            }
        }

        // cout << endl <
    }
}

/**
 * @brief Sends a message to another client by requesting its socket
 * 
 * @param cli - The socket to the LOOKUP_SERVER
 * @param srv - The socket to the UDP server
 * @param nick - The nickname to send too
 * @param msg - The message
 */
void send_client_message(int cli, int srv, string nick, string msg) 
{
    // Get the struct from the LOOKUP_SVR to send the message to the client
    sockaddr_in cli_sock;
    vector<string> splitter;
    string buffer;
    char host[INET_ADDRSTRLEN];
    int port;

    send_msg(cli, "MSG " + nick);

    splitter = str_split(recv_msg(cli));

    if(splitter[0].compare("ERROR") == 0)
    {
        cerr << "Username not connected, please try again" << endl; 
        return;
    }

    if(splitter[0].compare("HOST") != 0)
    {
        cerr << "Unexpected response from server" << endl;
        exit(1);
    }
    strcpy(host, splitter[1].c_str());
    cli_sock.sin_family = AF_INET;
    inet_pton(AF_INET, host, &cli_sock.sin_addr.s_addr);

    splitter = str_split(recv_msg(cli));
    if(splitter[0].compare("PORT") != 0)
    {
        cerr << "Unexpected response from server" << endl;
        exit(1);
    }

    sscanf(splitter[1].c_str(), "%d", &port);
    cli_sock.sin_port = ntohs(port);

    int soc = sizeof(cli_sock);
    send_msg(srv, nickname + " " + msg, &cli_sock);
    return;
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

string to_lower(string str) 
{
    for(int i = 0; i < str.length(); i++) 
       str[i] = tolower(str[i]);
    return str;
}


/**
 * @brief Used for sending messages over a socket 
 * 
 * @param sockid - The socket to send on
 * @param msg - The message
 * @param udp - The sockaddr_in if its a UDP message - Default NULL
 * @return int - The return value of send
 */
int send_msg(int sockid, string msg, const sockaddr_in* udp) 
{
    int send_result;
    if(udp == NULL)
    {
        msg.append("||");
        int len = msg.length();
        send_result = send(sockid, msg.c_str(), len, 0);
    }
    else 
    {
        msg.append("||");
        int len = msg.length();
        int udp_size = sizeof(*udp);
        send_result = sendto(sockid, msg.c_str(), len, 0,
                (sockaddr *)&(*udp), udp_size);
    }
    if(send_result < 0)
    {
        perror("ERROR: Send Message");
    }
    return send_result;
}

/**
 * @brief Gets a message from the client or server
 * 
 * @param sockid - The socket to recieve from
 * @return string - The message
 */
string recv_msg(int sockid)
{
    char buf[512];
    string msg = "";
    int recved;
    int str_len = 0;

    // Read messages until NULL
    while((recved = recvfrom(sockid, buf, sizeof(buf), 0, NULL, 0)) > 0)
    {
        str_len += recved;
        msg.append(buf);
        if((msg[str_len-2] == '|') && (msg[str_len-1] == '|')) {
            msg.erase(str_len-2, msg.length());
            return msg;
        }
    }

    if(recved == -1)
        return NULL;

    // Check that the tailing "||" was recieved 
    if((msg[str_len-2] != '|') && (msg[str_len-1] != '|'))
        return "";

    // Remove the tailing "||" and garbage
    msg.erase(str_len-2, msg.length());

    // Return the message
    return msg;
}