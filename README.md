# Advanced Unix Programming - Distributed Chat Server
## Author: Patrick Earl

---
### Developed for a course at Kutztown University for my graduate course in Advanced Unix Programming. 
### Developed with C++
---

### Description:
Develop a client and server application to allow clients to chat with each other. In place of a normal client-server-client messaging system, clients register with the server (See *LOOKUP_SERVER* in Project3.pdf). The server then acts as a directory in which clients will query and get returned a socket structure holding the needed information to send data to their requested client

Both the clients and server use shared memory. The server is a multi-processed server, where each process handles communication with the clients after the initial socket setup. Information about the clients is stored within the shared memory and access is protected using the Readers/Writers algorithm implemented using semaphores. I never got to implementing the clients to use share memory, 

More details about how the project was spec'd, refer to Project3.pdf

End grade was an "A"

---

### How to run?
**Instructions made using a machine running Ubuntu 19.04**
1) make all
    - Builds the server and client
2) ./server
3) ./client (nickname)
    - nickname: The name of the client connecting to the server
4) **To send a message**:
    - \<nickname> message
    - If the client is registered in the server, the message is displayed to the client. Otherwise the requesting client is shown an error message
5) If all clients disconnect or a Ctrl-C is received, the server will clean up and shutdown. 

#### Extra:
python shm_clear.py will check for any shared memory and semaphores on the system opened under the current user. This was used during development to clean up SM and Semaphores. 

---

This project is not under active development and is hosted for archival purposes.   
Please do not use this for academic dishonesty. Granted this was developed for a level-500 graduate course, I expect better of my graduate counter-parts ;-)

Questions/Comments/Concerns? Open up an an issue or contact me via my email listed on my github. Thanks :-)
