/******************************************************************
*	Author:				Patrick Earl
*	Filename:			util.h
*	Purpose:			Provides the functions to the client and 
*               server to send messages. Taken from the pipes
*               This is meant for TCP sockets
*******************************************************************/

#include <sys/types.h>
#include <sys/socket.h>
#include <string>

using namespace std;

int send_msg(int sockid, string msg) 
{
    msg.append("||");
    int len = msg.length();
    return send(sockid, msg.c_str(), len, 0);
}

string recv_msg(int sockid)
{
    char buf[512];
    string msg = "";

    // Read messages until NULL
    while((recv(sockid, buf, sizeof(buf), 0)) > 0)
    {
        msg.append(buf);
    }

    // Check that the tailing "||" was recieved 
    int len = msg.length();
    if((msg[len-1] != '|') && (msg[len] != '|'))
        return "";

    // Remove the tailing "||" and return the message
    msg.erase(len-2, len);
    return msg;
}