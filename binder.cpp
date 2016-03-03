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

#include "debug.h"


using namespace std;


// converts the first letter of each word in the text to uppercase
void capitalize(char* text) {
    for (unsigned int i = 0; i < strlen(text); i++) {
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


//////////////////////////////////  ProcSignature  ///////////////////////////////////////////////


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


///////////////////////////////// ProcLocation  ///////////////////////////////////////


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



///////////////////////////////////  Binder  ////////////////////////////////////////////



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
   			proc_location_request(msg_len, msg);
   			break;
   		case TERMINATE:
   			terminateServers();
   			status = TERMINATE_ALL;								 
   			break;
   	}

    delete msg;
    return status;
}



void Binder::proc_location_request(int msg_len, char * message){
	ProcSignature proc;
	memset(&proc, 0, sizeof(ProcSignature));

	strncpy(proc.procInfo.proc_name, message, MAX_PROC_NAME_SIZE);
	proc.procInfo.proc_name[MAX_PROC_NAME_SIZE] = '\0';

	message += (MAX_PROC_NAME_SIZE + 1);

	// find length of argTypes
    int argLen = *(int*) message; 
    proc.procInfo.argLen = argLen;
	proc.procInfo.argTypes = new int[argLen+1];

	// read argTypes
	message += sizeof(int);
	memcpy(proc.procInfo.argTypes, message, (argLen+1)*sizeof(int));

	map<ProcSignature, set<ProcLocation> >::iterator map_it;
    map_it = sig_to_location.find(proc);

    try{
    	if(map_it != sig_to_location.end()){
    		ProcLocation server_location = roundRobinServer(map_it->second);
    		cout << "send LOC_SUCCESS" << endl;
    	}else{
    		throw failure();
    	}
    }catch(failure& e){
    	cout << "send LOC_FAILURE" << endl; 
    }


}

ProcLocation Binder::roundRobinServer(set<ProcLocation>& loc_set){
	if (loc_set.size() == 1){
		return *(loc_set.begin());
	}

	set<ProcLocation>::iterator set_it;
	list<ProcLocation>::iterator list_it = server_queue.begin();
	for(;list_it != server_queue.end(); list_it++){
		set_it = loc_set.find(*list_it);
		if(set_it != loc_set.end()){
			list_it = server_queue.erase(list_it);
			server_queue.push_back(*set_it);
			break;
		}
	}

	return *set_it;
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
    int argLen = *(int*) p; 
    proc.procInfo.argLen = argLen;
	proc.procInfo.argTypes = new int[argLen+1];

	// read argTypes
	p += sizeof(int);
	memcpy(proc.procInfo.argTypes, p, (argLen+1)*sizeof(int));

    map<ProcSignature, set<ProcLocation> >::iterator map_it;
    map_it = sig_to_location.find(proc);

    debug("Data Parsed");

    // if this server has already registered this procedure, 
    // override the previous procedure
    // otherwise, add this server to procedure location set
    try{ 
	    if(map_it != sig_to_location.end()){
	    	set<ProcLocation>::iterator set_it = map_it->second.begin();
	    	for(; set_it != map_it->second.end(); set_it++){
	    		ProcLocation tmp = *set_it;
	    		if (memcmp(&loc, &tmp, sizeof(ProcLocation)) == 0){
	    			throw failure();
	    		}
	    	}

	    	if(set_it == map_it->second.end()){
	    		map_it->second.insert(loc);
	    		addToServerQueue(map_it->second);
	    	}
	    }else{
	    	// no such entry is found, add new one to database
	   		set<ProcLocation> loc_set;
	   		loc_set.insert(loc); 
	    	sig_to_location.insert(pair<ProcSignature, set<ProcLocation> >(proc, loc_set));

	    	debug("Add new entry");	    		
	    }


	    next_prior_id++;
	    // send SUCCESS  <msg_type + msg_size + REGISTER_SUCCESS >
	}catch(failure& e){
		// send FAILURE  <msg_type + msg_size + REGISTER_FAILURE >
	}
}



void Binder::addToServerQueue(set<ProcLocation>& loc_set){
	set<ProcLocation>::iterator set_it = loc_set.begin();
	for(; set_it != loc_set.end(); set_it++){
		server_queue.push_back(*set_it);
	}
}



// send TERMINATE message to all servers to stop
void Binder::terminateServers(){
	int type = (int) TERMINATE;
	list<ProcLocation>::iterator list_it = server_queue.begin();

	for(; list_it != server_queue.end(); list_it++){
		int status = send(list_it->locationInfo.s_port, &type, sizeof(type), 0);
		if(status < 0){
			cerr << "ERROR: on terminating servers"<< endl; 			//	??????? 
			exit(-1);
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
	    for(unsigned int i=0; i < connections.size(); i++){
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
        for(unsigned int i = 0; i < connections.size(); i++){
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



////////////////////////////////////////////  MAIN   ///////////////////////////////////////////////////


// int main(int argc, const char * argv[]) {
//     Binder binder;
//     binder.start();
//     return 0;
// }



////////////////////////////////////////////  TEST   ///////////////////////////////////////////////////


void Binder::printMap(){
	debug("print");
	map<ProcSignature, set<ProcLocation> >::iterator map_it = sig_to_location.begin();
	for(; map_it != sig_to_location.end(); map_it++){
		cout << map_it->first.procInfo.proc_name << " -> ";
		set<ProcLocation>:: iterator set_it = map_it->second.begin();
		for(; set_it != map_it->second.end(); set_it++){
			cout << set_it->locationInfo.s_id.addr.hostname << endl;
			cout << set_it->prior_id << endl;
	    	cout << set_it->locationInfo.s_port << endl;
		}
		cout << endl;
	}
}



int main(int argc, const char * argv[]) {
    Binder binder;
  	
  	proc_sig f;
  	char nameF[] = "f";
  	strncpy(f.proc_name, nameF, MAX_PROC_NAME_SIZE);
  	f.proc_name[MAX_PROC_NAME_SIZE] = '\0';
  	f.argLen = 2;
  	f.argTypes = new int[f.argLen+1];

  	f.argTypes[0] = (1 << ARG_OUTPUT) | (ARG_INT << 16);           
    f.argTypes[1] = (1 << ARG_INPUT)  | (ARG_INT << 16) | 10;
    f.argTypes[2] = 0; 


  	proc_sig g;
  	char nameG[] = "g";
  	strncpy(g.proc_name, nameG, MAX_PROC_NAME_SIZE);
  	g.proc_name[MAX_PROC_NAME_SIZE] = '\0';
  	g.argLen = 3;
  	g.argTypes = new int[g.argLen+1];

  	g.argTypes[0] = (1 << ARG_OUTPUT) | (ARG_INT << 16);
  	g.argTypes[1] = (1 << ARG_INPUT) | (ARG_INT << 16);
  	g.argTypes[2] = (1 << ARG_INPUT) | (ARG_INT << 16);
  	g.argTypes[3] = 0;


  	proc_sig h;
  	char nameH[] = "h";
  	strncpy(h.proc_name, nameH, MAX_PROC_NAME_SIZE);
  	h.proc_name[MAX_PROC_NAME_SIZE] = '\0';
  	h.argLen = 1;
  	h.argTypes = new int[h.argLen+1];

  	h.argTypes[0] = (1 << ARG_OUTPUT) | (1 << ARG_INPUT) | (ARG_LONG << 16) | 11;
  	h.argTypes[1] = 0;



  	location serverA;
  	serverA.s_port = 555;
  	serverA.s_id.addr_type = ADDR_TYPE_HOSTNAME;
  	strncpy(serverA.s_id.addr.hostname, "serverA-002-RR", MAX_HOSTNAME_SIZE);
  	serverA.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';


  	location serverB;
  	serverB.s_port = 444;
  	serverB.s_id.addr_type = ADDR_TYPE_HOSTNAME;
  	strncpy(serverB.s_id.addr.hostname, "serverB-002-RR", MAX_HOSTNAME_SIZE);
  	serverB.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';


  	location serverC;
  	serverC.s_port = 333;
  	serverC.s_id.addr_type = ADDR_TYPE_HOSTNAME;
  	strncpy(serverC.s_id.addr.hostname, "serverC-002-RR", MAX_HOSTNAME_SIZE);
  	serverC.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';


  	int requestLen = sizeof(location) + sizeof(proc_sig);
  	char *request1 = new char[requestLen+1];

  	memcpy(request1, &serverA, sizeof(location));
  	request1 += sizeof(location);

  	memcpy(request1, &f, sizeof(proc_sig));

  	request1[requestLen] = '\0';

  	binder.proc_registration(requestLen, request1);
  	binder.printMap();


  	// delete [] request1;
    return 0;
}
