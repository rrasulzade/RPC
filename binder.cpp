#include <iostream>
#include <iomanip> 
#include <string>
#include <vector>
#include <algorithm>

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

	// cout << "==Sig==1 " << lhs.procInfo.proc_name << " " << rhs.procInfo.proc_name << endl;

	if(str_compare != 0) {
		return false;
	}
	if(str_compare == 0){
		if(lhs.procInfo.argLen != rhs.procInfo.argLen || 
		   	(lhs.procInfo.argLen == rhs.procInfo.argLen && 
			memcmp(lhs.procInfo.argTypes, rhs.procInfo.argTypes, (lhs.procInfo.argLen+1)*sizeof(int)) != 0) )
			return false;

	}

	return true;
}


bool operator< (const ProcSignature& lhs, const ProcSignature& rhs){
	int str_compare = strcmp(lhs.procInfo.proc_name, rhs.procInfo.proc_name);

	// cout << "==Sig==2 " << str_compare << " " <<  lhs.procInfo.proc_name << " " << rhs.procInfo.proc_name << endl;

	if(str_compare > 0) {
		return false;
	}
	if(str_compare == 0){
		if(lhs.procInfo.argLen > rhs.procInfo.argLen || 
		   	(lhs.procInfo.argLen == rhs.procInfo.argLen && 
			memcmp(lhs.procInfo.argTypes, rhs.procInfo.argTypes, (lhs.procInfo.argLen+1)*sizeof(int)) >= 0) ){
				return false;
		}

	}

	return true;
}


///////////////////////////////// ProcLocation  ///////////////////////////////////////


bool operator==(const ProcLocation& lhs, const ProcLocation& rhs){
	// cout << "==Loc==1 " << lhs.locationInfo.s_id.addr.hostname << ":" << lhs.locationInfo.s_port 
	     // << "  " << rhs.locationInfo.s_id.addr.hostname << ":" << rhs.locationInfo.s_port << endl;

	if(lhs.locationInfo.s_port != rhs.locationInfo.s_port || 
		memcmp(lhs.locationInfo.s_id.addr.hostname, rhs.locationInfo.s_id.addr.hostname, MAX_HOSTNAME_SIZE) != 0){
		return false;
	}
	return true;
}


// bool operator< (const ProcLocation& lhs, const ProcLocation& rhs){

	// cout << "==Loc==2 " << lhs.locationInfo.s_id.addr.hostname << " " << rhs.locationInfo.s_id.addr.hostname << endl;


	// if(lhs.prior_id >= rhs.prior_id){
	// 	return false;
	// }
	// return true;
// }



///////////////////////////////////  Binder  ////////////////////////////////////////////



Binder::Binder(void){
	// next_prior_id = 0;
}

Binder::~Binder(void){
	map<ProcSignature, list<ProcLocation> >::iterator map_it = sig_to_location.begin();
    for(; map_it != sig_to_location.end(); map_it++){
    	delete [] map_it->first.procInfo.argTypes;
    }

}



// receive message, 
int Binder::handle_message(int sockFD){
    unsigned int msg_len;
    int msg_type;
    char *msg;

   

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

   	try{
		msg = new char[msg_len+1];					  		   	
	}catch(bad_alloc& e){
		int code = ERR_BINDER_OUT_OF_MEMORY;
		if(msg_type == REGISTER)
			sendREGISTER(sockFD, REGISTER_FAILURE, code);
		else if(msg_type == LOC_REQUEST)		
	    	sendLOC(sockFD, LOC_FAILURE, NULL, &code);
	}

	status = recv(sockFD, msg, msg_len, 0);				    
	if(status < 0){  
    	cerr << "ERROR: on receiving message" << endl;
    	return status;
   	}
   	msg[msg_len] = '\0';  

   	 // msg_type = *(int*)(msg);

    // connection is closed
   	if(status == 0){
   		delete msg;
   		return status;
   	}

   	switch(msg_type){
   		case REGISTER:
   			proc_registration(sockFD, msg);
   			break;
   		case LOC_REQUEST:
   			proc_location_request(sockFD, msg);
   			break;
   		case TERMINATE:
   			terminateServers();
   			status = TERMINATE_ALL;								 
   			break;
   		default:
   			status = sendResult(sockFD, UNKNOWN, ERR_RPC_UNEXPECTED_MSG_TYPE); 	
   	}

    delete msg;
    return status;
}


// send LOC_SUCCESS to client with the server location
int Binder::sendLOC_SUCC(int sockFD, msg_type type, ProcLocation* loc){
	int status = 0;
    unsigned int msg_len = sizeof(type) + sizeof(ProcLocation);
	char msg[msg_len+1];
	
	memcpy(msg, &type, sizeof(type));
	memcpy(msg+sizeof(type), loc, sizeof(ProcLocation));
	msg[msg_len] = '\0';

	status = send(sockFD, msg, msg_len, 0);

    return status;
}


// send LOC_FAILURE, REGISTER_FAILURE, REGISTER_SUCCESS and UNKNOWN
// message types with return code
int Binder::sendResult(int sockFD, msg_type type, int retCode){
	unsigned int msg_len = sizeof(type) + sizeof(retCode);
	char msg[msg_len+1];

	memcpy(msg, &type, sizeof(type));
	memcpy(msg+sizeof(type), &retCode, sizeof(retCode));
	msg[msg_len] = '\0';

	int status = send(sockFD, msg, msg_len, 0);

	return status;
}


// handle requests from client that needs the server location
void Binder::proc_location_request(int sockFD, char * message){
	ProcSignature proc;
	memset(&proc, 0, sizeof(ProcSignature));

	strncpy(proc.procInfo.proc_name, message, MAX_PROC_NAME_SIZE);
	proc.procInfo.proc_name[MAX_PROC_NAME_SIZE] = '\0';

	message += MAX_PROC_NAME_SIZE;

	// find length of argTypes
    int argLen = *(int*) message; 
    proc.procInfo.argLen = argLen;

    // handle memory allocation error 
    try{
		proc.procInfo.argTypes = new int[argLen+1];
	}catch(bad_alloc& e){
		cout << "send LOC_FAILURE  ERR_BINDER_OUT_OF_MEMORY" << endl; 

    	int code = ERR_BINDER_OUT_OF_MEMORY;
	    sendLOC(sockFD, LOC_FAILURE, NULL, &code);
	    return;
	}

	// read argTypes
	message += sizeof(int);
	memcpy(proc.procInfo.argTypes, message, (argLen+1)*sizeof(int));

	// change array length to 1 to indicate that this parameter is an array
	for(unsigned int i = 0; i < proc.procInfo.argLen; i++){
		if((proc.procInfo.argTypes[i] & 0x0000ffff) > 0)
			proc.procInfo.argTypes[i] = (proc.procInfo.argTypes[i] & 0xffff0000) + 0x00000001;
	}

	map<ProcSignature, list<ProcLocation> >::iterator map_it;
    map_it = sig_to_location.find(proc);

    try{
    	if(map_it != sig_to_location.end()){
    		ProcLocation server_location = roundRobinServer(map_it->second);
    		
    		cout << "send LOC_SUCCESS" << endl;
    		cout << proc.procInfo.proc_name <<  " Arglen: " <<  proc.procInfo.argLen <<" -> " << endl;
    		cout << "   "
				 << server_location.locationInfo.s_id.addr.hostname << " "			
	    	     << server_location.locationInfo.s_port << endl << endl;

           sendLOC(sockFD, LOC_SUCCESS, &server_location, NULL);     
           delete [] proc.procInfo.argTypes;     

    	}else{
    		throw failure();
    	}
    }catch(failure& e){
    	cout << "send LOC_FAILURE" << endl; 
    	cout << proc.procInfo.proc_name <<  " Arglen: " <<  proc.procInfo.argLen <<" -> " << endl;
    	
    	int code = ERR_RPC_NO_SERVER_AVAIL;
	    sendLOC(sockFD, LOC_FAILURE, NULL, &code);
	    delete [] proc.procInfo.argTypes;
    }


}

ProcLocation Binder::roundRobinServer(list<ProcLocation>& loc_set){
	if (loc_set.size() == 1){
		return *(loc_set.begin());
	}

	list<ProcLocation>::iterator set_it;
	list<ProcLocation>::iterator list_it = server_queue.begin();
	for(;list_it != server_queue.end(); list_it++){
		set_it = find(loc_set.begin(), loc_set.end(), *list_it);
		if(set_it != loc_set.end()){
			list_it = server_queue.erase(list_it);
			server_queue.push_back(*set_it);
			break;
		}
	}

	return *set_it;
}


void Binder::proc_registration(int sockFD, char * message){
	ProcSignature proc;
	ProcLocation loc;
	
	memset(&proc, 0, sizeof(ProcSignature));
	memset(&loc, 0, sizeof(ProcLocation));

	// read server identifier and server port
	memcpy(&loc.locationInfo, message, sizeof(location));

	char *p = message + sizeof(location);

	// read procedure name
	strncpy(proc.procInfo.proc_name, p, MAX_PROC_NAME_SIZE);
	proc.procInfo.proc_name[MAX_PROC_NAME_SIZE] = '\0';

	// debug(proc.procInfo.proc_name);

	p += MAX_PROC_NAME_SIZE;
 	
 	// find length of argTypes
    int argLen = *(int*) p; 
    proc.procInfo.argLen = argLen;

    // handle memory allocation error 
	try{
		proc.procInfo.argTypes = new int[argLen+1];
	}catch(bad_alloc& e){
		cout << "send REG_FAILURE  ERR_BINDER_OUT_OF_MEMORY" << endl; 

    	int code = ERR_BINDER_OUT_OF_MEMORY;
	    sendREGISTER(sockFD, REGISTER_FAILURE, code);
	    return;
	}

	// read argTypes
	p += sizeof(int);
	memcpy(proc.procInfo.argTypes, p, (argLen+1)*sizeof(int));

	// if any of the argTypes contains array length, change that length to 1
	// to indicate that this argType requires array
	// otherwise, length is 0 for scalar paramaters 
	for(unsigned int i = 0; i < proc.procInfo.argLen; i++){
		if((proc.procInfo.argTypes[i] & 0x0000ffff) > 0)
			proc.procInfo.argTypes[i] = (proc.procInfo.argTypes[i] & 0xffff0000) + 0x00000001;
	}

    map<ProcSignature, list<ProcLocation> >::iterator map_it;
    map_it = sig_to_location.find(proc);

    // debug("Data Parsed");

    // if this server has already registered this procedure, 
    // override the previous procedure
    // otherwise, add this server to procedure location set
    try{ 
	    if(map_it != sig_to_location.end()){
	    	
	    	// debug("Already registered");	    

	    	list<ProcLocation>::iterator set_it = map_it->second.begin();
	    	for(; set_it != map_it->second.end(); set_it++){
	    		if (loc == *set_it){
	    			throw failure();
	    		}
	    	}

	    	if(set_it == map_it->second.end()){
	    		map_it->second.push_back(loc);
	    		// debug("New server added to set");
	    		if(map_it->second.size() > 2)
	    			addToServerQueue(loc);
	    		else if(map_it->second.size() == 2)	
	    			addToServerQueue(map_it->second);
	    	}

	    	// this proc is already in DB 
	        delete [] proc.procInfo.argTypes;
	 
	    }else{
	    	// no such entry is found, add new one to database
	   		list<ProcLocation> loc_set;
	   		loc_set.push_back(loc); 
	    	sig_to_location.insert(pair<ProcSignature, list<ProcLocation> >(proc, loc_set));

	    	// debug("Add new entry");	    		
	    }

	    cout << "send SUCCESS  <msg_type + msg_size + ERR_RPC_SUCCESS >" << endl;
	    int code = ERR_RPC_SUCCESS;
        sendREGISTER(sockFD, REGISTER_SUCCESS, code);

	}catch(failure& e){
		cout << "send FAILURE  <msg_type + msg_size + ERR_RPC_PROC_RE_REG >" << endl;
		int code = ERR_RPC_PROC_RE_REG;
        sendREGISTER(sockFD, REGISTER_FAILURE, code);
	}
}

void Binder::addToServerQueue(ProcLocation& loc){
	list<ProcLocation>::iterator list_it = server_queue.begin();
	for(; list_it != server_queue.end(); list_it++){
		if (*list_it == loc)
			return;
	}

	server_queue.push_back(loc);
}

void Binder::addToServerQueue(list<ProcLocation>& loc_set){
	list<ProcLocation>::iterator set_it = loc_set.begin();
	list<ProcLocation>::iterator list_it = server_queue.begin();
	bool found;
	for(; set_it != loc_set.end(); set_it++){
		found = false;
		for(; list_it != server_queue.end(); list_it++){
			if(*set_it == *list_it){
				found = true;
				break;
			}
		}
		if(!found)
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
    char hostname[MAX_HOSTNAME_SIZE+1];
	gethostname(hostname, MAX_HOSTNAME_SIZE);
	hostname[MAX_HOSTNAME_SIZE] = '\0';
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



