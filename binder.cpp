#include <iostream>
#include <string>
#include <vector>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>

#include "binder.h"

using namespace std;


// converts the first letter of each word in the text to uppercase
void capitalize(char* text) {
    for (int i = 0; i < strlen(text); i++) {
        if (i == 0 || text[i-1] == ' ' || text[i-1] == '\t') {
            text[i] = toupper(text[i]);
        } else {
            text[i] = tolower(text[i]);
        }
    }
}


// convert binary to decimal representation
int representInDecimal(unsigned char *buf){
    return ((unsigned long int)buf[0] << 24) | ((unsigned long int)buf[1] << 16) |
           ((unsigned long int)buf[2] << 8)  |  buf[3];
}


//////////////////////////////////////////////////////////////////////////////////////////


bool operator==(const ProcSignature& lhs, const ProcSignature& rhs) {
	int str_compare = strcmp(lhs.procInfo.proc_name, rhs.procInfo.proc_name);
	if(str_compare != 0 || lhs.procInfo.argLen != rhs.procInfo.argLen){
		return false;
	}else{
		// for (int i = 0; i < lhs.procInfo.argLen; ++i){
			if(memcmp(lhs.procInfo.argTypes, rhs.procInfo.argTypes, (lhs.procInfo.argLen+1)*sizeof(int)) != 0){
				return false;
			}
		// }
	}
	return true;
}


bool operator< (const ProcSignature& lhs, const ProcSignature& rhs){
	int str_compare = strcmp(lhs.procInfo.proc_name, rhs.procInfo.proc_name);
	if(str_compare >= 0 || lhs.procInfo.argLen != rhs.procInfo.argLen){
		return false;
	}else{
		// for (int i = 0; i < lhs.procInfo.argLen; ++i){
			if(memcmp(lhs.procInfo.argTypes, rhs.procInfo.argTypes, (lhs.procInfo.argLen+1)*sizeof(int)) >= 0){
				return false;
			}
		// }
	}
	return true;
}


//////////////////////////////////////////////////////////////////////////////////////////


bool operator==(const ProcLocation& lhs, const ProcLocation& rhs){
	if(lhs.prior_id != rhs.prior_id){
		return false;
	}
	return true;
}


bool operator< (const ProcLocation& lhs, const ProcLocation& rhs){
	if(lhs.prior_id >= rhs.prior_id){
		return false;
	}
	return true;
}



//////////////////////////////////////////////////////////////////////////////////////////



Binder::Binder(void){
	next_prior_id = 0;
}

Binder::~Binder(void){
	map<ProcSignature, set<ProcLocation> >::iterator map_it = sig_to_location.begin();
    for(; map_it != sig_to_location.end(); map_it++){
    	delete [] map_it->first.procInfo.argTypes;
    }

}


// receive message, 
int Binder::handle_message(int sockFD){
    int msg_len;
    int msg_type;
    char *msg;

    // msg_type = *(int*)(msg+4);

    int status = recv(sockFD, &msg_len, sizeof(msg_len), 0);
    if(status < 0){
    	cerr << "ERROR: on receiving message length" << endl;
    	return status;
   	}

   	status = recv(sockFD, &msg_type, sizeof(msg_type), 0);
   	if(status < 0){
    	cerr << "ERROR: on receiving message type" << endl;
    	return status;
   	}

	//int msg_len = representInDecimal((unsigned char*)length);
	msg = new char[msg_len];					  		   	// << +1 is already done ????
	status = recv(sockFD, msg, msg_len, 0);				    // << +1 is already done ????
	if(status < 0){  
    	cerr << "ERROR: on receiving message" << endl;
    	return status;
   	}
   	// msg[msg_len] = '\0';   // already null terminated

    // connection is closed
   	if(status == 0){
   		delete msg;
   		return status;
   	}

   	
   	switch(msg_type){
   		case REGISTER:
   			proc_registration(msg_len, msg);
   			break;
   		case LOC_REQUEST:
   			break;
   		case EXECUTE:
   			break;
   		case TERMINATE:
   			terminateServers();
   			status = TERMINATE_ALL;								 
   			break;
   	}

    delete msg;
    return status;
}


void Binder::proc_registration(int msg_len, char * message){
	ProcSignature proc;
	ProcLocation loc;
	
	memset(&proc, 0, sizeof(ProcSignature));
	memset(&loc, 0, sizeof(ProcLocation));

	// read server identifier and server port
	loc.prior_id = next_prior_id;
	loc.locationInfo.s_id = *(server_identifier*) message;
	loc.locationInfo.s_port = *(unsigned short *)(message + sizeof(server_identifier));
	
	char *p = message + sizeof(server_identifier) + sizeof(unsigned short);

	// read procedure name
	strncpy(proc.procInfo.proc_name, p, MAX_PROC_NAME_SIZE);
	proc.procInfo.proc_name[MAX_PROC_NAME_SIZE] = '\0';

	p += (MAX_PROC_NAME_SIZE + 1);
 	
 	// find length of argTypes
    int argLen = *(int*) p; 			//(msg_len - sizeof(server_identifier) - sizeof(unsigned short) - (MAX_PROC_NAME_SIZE + 1)) / 4;
    proc.procInfo.argLen = argLen;
	proc.procInfo.argTypes = new int[argLen+1];

	// read argTypes
	p += sizeof(int);
	memcpy(proc.procInfo.argTypes, p, (argLen+1)*sizeof(int));

    map<ProcSignature, set<ProcLocation> >::iterator map_it;
    map_it = sig_to_location.find(proc);

    // if this server has already registered this procedure, 
    // override the previous procedure
    // otherwise, add this server to procedure location set 
    if(map_it != sig_to_location.end()){
    	map_it->second.insert(loc);
    	
    }else{
    	// no such entry is found, add new one to database
   		set<ProcLocation> loc_set;
   		loc_set.insert(loc); 
    	sig_to_location.insert(pair<ProcSignature, set<ProcLocation> >(proc, loc_set));
    }

    next_prior_id++;

}


// send TERMINATE message to all servers to stop
void Binder::terminateServers(){
	int type = (int) TERMINATE;
	while(!server_queue.empty()){
		int status = send(server_queue.front().locationInfo.s_port, &type, sizeof(type), 0);
		server_queue.pop();
		if(status < 0){
			// cerr << "ERROR: on terminating servers"<< endl; 			//	??????? 
			// exit(-1);
		}
	}
}


// create socket, then bind binder_addr to socket
// then listen for socket connections
void Binder::setup_socket(){
    sockaddr_in binder_sockaddr, sock_in;
    int status;
    int len = sizeof(sockaddr_in);
    
    memset(&binder_sockaddr, 0, sizeof(sockaddr_in));
    binder_sockaddr.sin_family = AF_INET;
    binder_sockaddr.sin_addr.s_addr = INADDR_ANY;
    binder_sockaddr.sin_port = 0;
    
    // create a socket
    binder_sockFD = socket(AF_INET, SOCK_STREAM, 0);
    if (binder_sockFD < 0) {
        cerr << "ERROR: server socket cannot initialized" << endl;
        exit(-1);
    }
    
    // bind the binder socket to the current IP address on port
    status = bind(binder_sockFD, (struct sockaddr*)&binder_sockaddr, (socklen_t)len);
    if(status < 0){
        cerr << "ERROR: on binding" << endl;
        exit(-1);
    }
    
    // Listen for SOMAXCONN = 128 clients
    listen(binder_sockFD, SOMAXCONN);
    
    // get hostname and print
    char hostname[64];
    gethostname(hostname, sizeof(hostname));
    cout << "BINDER_ADDRESS " << hostname << endl;
    
    // get binder port number and print
    getsockname(binder_sockFD, (struct sockaddr *)&sock_in, (socklen_t *)&len);
    cout << "BINDER_PORT " << ntohs(sock_in.sin_port) << endl;
}


void Binder::start(){
	sockaddr_in connected_sockaddr;
    int connected_sockFD , status;
    int len = sizeof(sockaddr_in);
    bool stop = false;

    setup_socket();

    // save socket of each connection in this vector
    vector<int> connections;

    // initially add binder socket to the vector
	connections.push_back(binder_sockFD);
    
    while (!stop) {

    	// setup socket file descriptor and find max value of fd so far 
        fd_set readfds;
	    FD_ZERO(&readfds);
	    int max_fd = binder_sockFD;
	    for(int i=0; i < connections.size(); i++){
	    	FD_SET(connections[i], &readfds);
	    	if(connections[i] > max_fd)
	    		max_fd = connections[i];
	    }

        // block here and wait for connection
        int active_clients = select( max_fd + 1 , &readfds , NULL , NULL , NULL);

        if (active_clients < 0){
            cerr << "ERROR: on select()"<< endl;
            exit(-1);
        }
        
        // no updates
        if (active_clients == 0) {
            continue;
        }

        // check all existing connections
        for(int i = 0; i < connections.size(); i++){
            // there is an incoming connection
            if(FD_ISSET(connections[i], &readfds)){

                if(connections[i] == binder_sockFD){
                    // connect to client, accept the data
                    connected_sockFD = accept(binder_sockFD, (sockaddr*)&connected_sockaddr, (socklen_t *)&len);
                    if (connected_sockFD < 0) {
                        cerr << "ERROR: on accept()" << endl;
                        exit(-1);
                    }
                    cout << "New conncetion " << connected_sockFD << endl;
                    // add new socket to connections vector and fd set
                    connections.push_back(connected_sockFD);
                    max_fd = max_fd < connected_sockFD ? connected_sockFD : max_fd;
                }else{
                    // already accepted, receive msg and capitalaze text
                    cout << "handle_message " << connected_sockFD << endl;
                    status = handle_message(connections[i]);

                    // TERMINATE msg is received 
                    if (status == TERMINATE_ALL){
                    	stop = true;
                    	break;
                    }

                    // erroneous msg is received or connection is closed
                    if (status <= 0 ) {
                        close(connections[i]);
                        FD_CLR(connections[i], &readfds);
                        connections.erase(connections.begin() + i);
                        continue;
                    }
                }
            }
        }
    }
    
    close(binder_sockFD);
}



//////////////////////////////////////////////////////////////////////////////////////////////////////////


int main(int argc, const char * argv[]) {
    Binder binder;
    binder.start();
    return 0;
}