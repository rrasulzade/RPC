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


//////////////////////////////////  ProcSignature  ///////////////////////////////////////////////


bool operator==(const ProcSignature& lhs, const ProcSignature& rhs) {
	int str_compare = strcmp(lhs.procInfo.proc_name, rhs.procInfo.proc_name);

	if(str_compare != 0) {
		return false;
	}
	if(str_compare == 0){
		if(lhs.procInfo.argLen != rhs.procInfo.argLen || (lhs.procInfo.argLen == rhs.procInfo.argLen && 
			memcmp(lhs.procInfo.argTypes, rhs.procInfo.argTypes, (lhs.procInfo.argLen+1)*sizeof(int)) != 0) )
			return false;

	}

	return true;
}


bool operator< (const ProcSignature& lhs, const ProcSignature& rhs){
	int str_compare = strcmp(lhs.procInfo.proc_name, rhs.procInfo.proc_name);
	
	if(str_compare > 0) {
		return false;
	}
	if(str_compare == 0){
		if(lhs.procInfo.argLen > rhs.procInfo.argLen || (lhs.procInfo.argLen == rhs.procInfo.argLen && 
			memcmp(lhs.procInfo.argTypes, rhs.procInfo.argTypes, (lhs.procInfo.argLen+1)*sizeof(int)) >= 0) ){
				return false;
		}

	}

	return true;
}


///////////////////////////////// ProcLocation  ///////////////////////////////////////


bool operator==(const ProcLocation& lhs, const ProcLocation& rhs){
	if(lhs.locationInfo.s_port != rhs.locationInfo.s_port || 
		memcmp(lhs.locationInfo.s_id.addr.hostname, rhs.locationInfo.s_id.addr.hostname, MAX_HOSTNAME_SIZE) != 0){
		return false;
	}
	return true;
}



///////////////////////////////////////// Helper Functions  /////////////////////////////////////////////////

// loop until requires amount of data is not received
int recvMsg(int sockFD, char data[], unsigned int msg_len){
	if (msg_len <= 0) {
		return 0;
	}

	unsigned int bytesRemain = msg_len;
	int received = 0;
	char *msg = data;

	while (bytesRemain > 0) {
		received = recv(sockFD, msg, bytesRemain, 0);
		if (received <= 0) {
			cerr << "ERROR: on receiving message" << endl;
			return received;
		}
		msg += received;
		bytesRemain -= received;	 
	}
	
	return msg_len;
}

// send LOC_SUCCESS with location or
// LOC_FAILURE, REGISTER_FAILURE, REGISTER_SUCCESS, TERMINATE and UNKNOWN
// message types with return code
int sendResult(int sockFD, int type, void* data){ 
	__INFO("");

	unsigned int msg_len = 0, total_len = sizeof(msg_len) + sizeof(type);
	int status = 0;
	char* msg;
	if(type == LOC_SUCCESS){
		msg_len = sizeof(location);
		total_len += msg_len;
		msg = new char[total_len+1];
		memset(msg, 0, total_len);	

		memcpy(msg, &msg_len, sizeof(msg_len));
		memcpy(msg+sizeof(msg_len), &type, sizeof(type));		
		memcpy(msg+sizeof(msg_len)+sizeof(type), data, sizeof(location));
	}else{
		msg_len = sizeof(int);
		total_len += msg_len;
		msg = new char[total_len+1];
		memset(msg, 0, total_len);

		memcpy(msg, &msg_len, sizeof(msg_len));
		memcpy(msg+sizeof(msg_len), &type, sizeof(type));
		memcpy(msg+sizeof(msg_len)+sizeof(type), data, sizeof(int));	
	}

	msg[total_len] = '\0';
	status = send(sockFD, msg, total_len, 0);

	delete []msg;
	return status;
}



///////////////////////////////////  Binder  ////////////////////////////////////////////

// Binder constructor
Binder::Binder(void){}


// Binder detructor
Binder::~Binder(void){
	__INFO("");

	map<ProcSignature, list<ProcLocation> >::iterator map_it = sig_to_location.begin();
    for(; map_it != sig_to_location.end(); map_it++){
    	delete [] map_it->first.procInfo.argTypes;
    }

}


// handle requests from Server and Client
// send appropriate result back to sender
int Binder::handle_message(int sockFD){
    unsigned int msg_len = 0;
    int msgType;
    int status;
    char *msg;
     
    // receive message len
    char m_len[sizeof(unsigned int)];
    status = recvMsg(sockFD, m_len, sizeof(unsigned int));
    if(status <= 0) return status;
	memcpy(&msg_len, m_len, sizeof(unsigned int));

   	// receive message type
   	char m_type[sizeof(int)];
    status = recvMsg(sockFD, m_type, sizeof(int));
    if(status <= 0) return status;
	memcpy(&msgType, m_type, sizeof(int));

   	// handle memory allocation error
   	try{
		msg = new char[msg_len+1];					  		   	
	}catch(bad_alloc& e){
		int code = ERR_BINDER_OUT_OF_MEMORY;
	    status = sendResult(sockFD, msgType, &code);
	    return status;
	}	

	// receive message
	status = recvMsg(sockFD, msg, msg_len);
    if(status <= 0){
    	delete [] msg;
     	return status;
     }
   	msg[msg_len] = '\0';  


   	switch(msgType){
   		case REGISTER:
   			proc_registration(sockFD, msg);
   			printMap();
   			break;
   		case LOC_REQUEST:
   			proc_location_request(sockFD, msg);
   			break;
   		case LOC_REQUEST_CACHE:
   			proc_cache_request(sockFD, msg);
   			break;	
   		case TERMINATE:
   			status = terminateServers();							 
   			break;
   		default:
   		{
   			int code =  ERR_RPC_UNEXPECTED_MSG_TYPE;
   			status = sendResult(sockFD, UNKNOWN, &code); 
   		}
   	}

    delete [] msg;
    return status;
}



void Binder::proc_cache_request(int sockFD, char * message){
	__INFO("");

	ProcSignature proc;
	memset(&proc, 0, sizeof(ProcSignature));

	// copy procedure name from message
	strncpy(proc.procInfo.proc_name, message, MAX_PROC_NAME_SIZE);
	proc.procInfo.proc_name[MAX_PROC_NAME_SIZE] = '\0';

	message += (MAX_PROC_NAME_SIZE+1)*sizeof(char);

	// find length of argTypes
    int argLen = *(int*) message; 
    proc.procInfo.argLen = argLen;

    // handle memory allocation error 
    try{
		proc.procInfo.argTypes = new int[argLen+1];
	}catch(bad_alloc& e){
		cerr << "New failed: ERR_BINDER_OUT_OF_MEMORY" << endl; 
    	int code = ERR_BINDER_OUT_OF_MEMORY;
	    sendResult(sockFD, LOC_FAILURE, &code);
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
    	if(map_it != sig_to_location.end() && map_it->second.size() > 0){



           delete [] proc.procInfo.argTypes;     

    	}else{
    		throw failure();
    	}
    }catch(failure& e){
    	cerr << "LOC_FAILURE: ERR_RPC_NO_SERVER_AVAIL" << endl; 
    	cout << proc.procInfo.proc_name <<  " Arglen: " <<  proc.procInfo.argLen <<" -> " << endl;
    	
    	int code = ERR_RPC_NO_SERVER_AVAIL;
	    sendResult(sockFD, LOC_FAILURE, &code);
	    delete [] proc.procInfo.argTypes;
    }


}


// handle requests from client that needs the server location
void Binder::proc_location_request(int sockFD, char* message){
	__INFO("");

	ProcSignature proc;
	memset(&proc, 0, sizeof(ProcSignature));

	// copy procedure name from message
	strncpy(proc.procInfo.proc_name, message, MAX_PROC_NAME_SIZE);
	proc.procInfo.proc_name[MAX_PROC_NAME_SIZE] = '\0';

	message += (MAX_PROC_NAME_SIZE+1)*sizeof(char);

	// find length of argTypes
    int argLen = *(int*) message; 
    proc.procInfo.argLen = argLen;

    // handle memory allocation error 
    try{
		proc.procInfo.argTypes = new int[argLen+1];
	}catch(bad_alloc& e){
		cerr << "New failed: ERR_BINDER_OUT_OF_MEMORY" << endl; 
    	int code = ERR_BINDER_OUT_OF_MEMORY;
	    sendResult(sockFD, LOC_FAILURE, &code);
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

    // no need to this procedure after find is called
    delete [] proc.procInfo.argTypes; 

    try{
    	if(map_it != sig_to_location.end() && map_it->second.size() > 0){
    		ProcLocation server_location = roundRobinServer(map_it->second);
            sendResult(sockFD, LOC_SUCCESS, &(server_location.locationInfo)); 
    	}else{
    		throw failure();
    	}
    }catch(failure& e){
    	cerr << "LOC_FAILURE: ERR_RPC_NO_SERVER_AVAIL" << endl; 
    	int code = ERR_RPC_NO_SERVER_AVAIL;
	    sendResult(sockFD, LOC_FAILURE, &code);
    }


}


// simplified Round Robin algorithm to choose the next available server
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


// registers procedure that is sent by the server
// send REGISTER_SUCCESS if procedure has not already registered,
// otherwise, send REGISTER_FAILURE to the server
void Binder::proc_registration(int sockFD, char * message){
	ProcSignature proc;
	ProcLocation loc;
	
	memset(&proc, 0, sizeof(ProcSignature));
	memset(&loc, 0, sizeof(ProcLocation));

	// read server identifier and server port
	memcpy(&loc.locationInfo, message, sizeof(location));
	loc.socketFD = sockFD;

	cout << "SERVER SockFD " << sockFD << endl;

	char *p = message + sizeof(location);

	// read procedure name
	strncpy(proc.procInfo.proc_name, p, MAX_PROC_NAME_SIZE);
	proc.procInfo.proc_name[MAX_PROC_NAME_SIZE] = '\0';

	p += (MAX_PROC_NAME_SIZE+1)*sizeof(char);
 	
 	// find length of argTypes
    int argLen = *(int*) p; 
    proc.procInfo.argLen = argLen;

    // handle memory allocation error 
	try{
		proc.procInfo.argTypes = new int[argLen+1];
	}catch(bad_alloc& e){
		cout << "new failed: ERR_BINDER_OUT_OF_MEMORY" << endl; 

    	int code = ERR_BINDER_OUT_OF_MEMORY;
	    sendResult(sockFD, REGISTER_FAILURE, &code);
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

    // if this server has already registered this procedure, 
    // override the previous procedure
    // otherwise, add this server to procedure location set
    try{ 
	    if(map_it != sig_to_location.end()){
	    	list<ProcLocation>::iterator set_it = map_it->second.begin();
	    	for(; set_it != map_it->second.end(); set_it++){
	    		if (loc == *set_it){
	    			throw failure();
	    		}
	    	}

	    	if(set_it == map_it->second.end()){
	    		map_it->second.push_back(loc);
	    		if(map_it->second.size() > 2)
	    			addToServerQueue(loc);
	    		else if(map_it->second.size() == 2)	
	    			addToServerQueue(map_it->second);
	    	}

	    	// this procedure is already in DB 
	        delete [] proc.procInfo.argTypes;
	 
	    }else{
	    	// no such entry is found, add new one to database
	   		list<ProcLocation> loc_set;
	   		loc_set.push_back(loc); 
	    	sig_to_location.insert(pair<ProcSignature, list<ProcLocation> >(proc, loc_set));    		
	    }

	    cout << "send SUCCESS  <msg_type + msg_size + ERR_RPC_SUCCESS >" << endl;
	    if(getServerIndex(sockFD) < 0){
		    server_sockets.push_back(sockFD);
	    }

	    int code = ERR_RPC_SUCCESS;
        // sendResult(sockFD, REGISTER_SUCCESS, NULL, &code);
        sendResult(sockFD, REGISTER_SUCCESS, &code);

	}catch(failure& e){
		cout << "send FAILURE  <msg_type + msg_size + ERR_RPC_PROC_RE_REG >" << endl;
		 int code = ERR_RPC_PROC_RE_REG;
        // sendResult(sockFD, REGISTER_FAILURE, NULL, &code);
		 sendResult(sockFD, REGISTER_FAILURE, &code);
	}
}


// adds the given server location to the server queue, if they are
// not on the queue
void Binder::addToServerQueue(ProcLocation& loc){
	__INFO("");

	list<ProcLocation>::iterator list_it = server_queue.begin();
	for(; list_it != server_queue.end(); list_it++){
		if (*list_it == loc)
			return;
	}

	server_queue.push_back(loc);
}


// adds the first and the second servers in the set of locations 
// mapped by the same procedure to the server queue if they are not 
// on the queue
void Binder::addToServerQueue(list<ProcLocation>& loc_set){
	__INFO("");

	list<ProcLocation>::iterator set_it = loc_set.begin();
	list<ProcLocation>::iterator list_it = server_queue.begin();
	for(; set_it != loc_set.end(); set_it++){
		for(; list_it != server_queue.end() && !(*set_it == *list_it); list_it++);
		if(list_it == server_queue.end())
			server_queue.push_back(*set_it);
	}
}



// send TERMINATE message to all servers to stop
int Binder::terminateServers(){
	__INFO("");

	int type = TERMINATE;
	vector<int>::iterator v_it = server_sockets.begin();

	printList();

	for(; v_it != server_sockets.end(); v_it++){
		int code = ERR_RPC_SUCCESS;
		// int status = sendResult(*v_it, type, NULL, &code);
		int status = sendResult(*v_it, type, &code);
		
		cout << "TERMINATE sent to " << *v_it << endl;

		if(status < 0){
			cerr << "ERROR: on terminating servers"<< endl; 			
		}
	}
	return ERR_BINDER_TERMINATE_SIG;
}


// check whether the given socket is in vector of active server sockets
// if so, return its index
// otherwise return -1
int Binder::getServerIndex(int socketFD){
	for(unsigned int i = 0; i < server_sockets.size(); i++){
		if(server_sockets[i] == socketFD){
			return i;
		}
	}
	return -1;
}


// check whether the given socket is in vector of active server sockets
// if so erase that entry from the vector, server queue for Round Robin
// and proc -> server mapping table
void Binder::removeServer(int socketFD){
	int i = getServerIndex(socketFD);
	if(i >= 0){
		// remove server socket from vector of active sockets
		server_sockets.erase(server_sockets.begin() + i);
        
        // remove server from procedure -> server identifier mapping table
		map<ProcSignature, list<ProcLocation> >::iterator map_it = sig_to_location.begin();
  		for(; map_it != sig_to_location.end(); map_it++){
    		list<ProcLocation>::iterator set_it = map_it->second.begin();
    		for(; set_it != map_it->second.end() ; set_it++){
    			if(set_it->socketFD == socketFD){
    				// cout << "FOUND" << endl;
    				set_it = map_it->second.erase(set_it);
    				// printMap();
    				// cout << "REMOVED" << endl;
    			}
    		}
		}

		// remove server from server queue for Round Robin
		list<ProcLocation>::iterator queue_it = server_queue.begin();
		for(; queue_it != server_queue.end(); queue_it++){
			if(queue_it->socketFD == socketFD){
				queue_it = server_queue.erase(queue_it);
			}
		}
	}
}




// create socket, then bind binder_addr to socket
// then listen for socket connections
void Binder::setup_socket(){
	__INFO("");

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
        return;
    }
    
    // bind the binder socket to the current IP address on port
    status = bind(binder_sockFD, (struct sockaddr*)&binder_sockaddr, (socklen_t)len);
    if(status < 0){
        cerr << "ERROR: on binding" << endl;
        return;
    }
    
    // Listen for SOMAXCONN = 128 clients
    listen(binder_sockFD, SOMAXCONN);
    
    // get hostname and print
    char hostname[MAX_HOSTNAME_SIZE+1];
	gethostname(hostname, MAX_HOSTNAME_SIZE);
	hostname[MAX_HOSTNAME_SIZE] = '\0';
    cout << "export BINDER_ADDRESS=" << hostname << endl;
    
    // get binder port number and print
    getsockname(binder_sockFD, (struct sockaddr *)&sock_in, (socklen_t *)&len);
    cout << "export BINDER_PORT=" << ntohs(sock_in.sin_port) << endl;
}



void Binder::start(){
	__INFO("");

	sockaddr_in connected_sockaddr;
    int connected_sockFD , status;
    int len = sizeof(sockaddr_in);
    bool stop = false;

    setup_socket();

    // save socket of each connection in this vector
    vector<int> connections;

    // initially add binder socket to the vector
	connections.push_back(binder_sockFD);
    int max_fd = binder_sockFD;
    
    // continue if TERMINATE msg is not received 
    while (!stop) {
    	// setup socket file descriptor and find max value of fd so far 
        fd_set readfds;
	    FD_ZERO(&readfds);

	    for(unsigned int i=0; i < connections.size(); i++){
	    	FD_SET(connections[i], &readfds);
	    	if(connections[i] > max_fd){
	    		max_fd = connections[i];
	    	}
	    }

        // block here and wait for connection
        int active_clients = select( max_fd + 1 , &readfds , NULL , NULL , NULL);
        if (active_clients < 0){
            cerr << "ERROR: on select()"<< endl;
            return;
        }
        
        // no updates
        if (active_clients == 0) {
            continue;
        }

        // check all existing connections
        unsigned int i = 0; 
        while(i < connections.size()){
            // there is an incoming connection
            if(FD_ISSET(connections[i], &readfds)){
                if(connections[i] == binder_sockFD){
                    // connect to client, accept the data
                    connected_sockFD = accept(binder_sockFD, (sockaddr*)&connected_sockaddr, (socklen_t *)&len);
                    if (connected_sockFD < 0) {
                        cerr << "ERROR: on accept()" << endl;
                        return;
                    }

                    cout << "New conncetion " << connected_sockFD << endl;

                    // add new socket to connections vector and fd set
                    connections.push_back(connected_sockFD);
                    max_fd = max_fd < connected_sockFD ? connected_sockFD : max_fd;
                }else{
                    // already accepted, receive msg and capitalaze text
                    cout << "handle_message " << connections[i] << endl;
                    status = handle_message(connections[i]);

                    // TERMINATE msg is received 
                    if (status == ERR_BINDER_TERMINATE_SIG){
                    	cout << "Terminate all servers are done " << endl;
                    	stop = true;
                    	break;
                    }

                    // erroneous msg is received or connection is closed
                    if (status <= 0 ) {
                    	cout << "Close connection socket " << connections[i] << " index " << i << endl;
                        close(connections[i]);
                        FD_CLR(connections[i], &readfds);
                     
                        // if this is server socket, remove from vector of active server socket
                        // and remove from proc -> server mapping table
                        removeServer(connections[i]);

                        // erase this socket from connections list
                        connections.erase(connections.begin() + i);
                        continue;
                    }
                }
            }
            i++;
        }
    }
    
    close(binder_sockFD);
}




void Binder::printMap(){
  // debug("print");
  map<ProcSignature, list<ProcLocation> >::iterator map_it = sig_to_location.begin();
  for(; map_it != sig_to_location.end(); map_it++){
    cout << map_it->first.procInfo.proc_name <<  " Arglen: " <<  map_it->first.procInfo.argLen <<" -> " << endl;
    list<ProcLocation>:: iterator set_it = map_it->second.begin();
    for(; set_it != map_it->second.end(); set_it++){
      cout << "   "
         << set_it->locationInfo.s_id.addr.hostname << " "      
             << ntohs(set_it->locationInfo.s_port) << endl;
    }
    cout << endl;
  }
}


void Binder::printList(){
  list<ProcLocation>::iterator list_it = server_queue.begin();
  for(;list_it != server_queue.end(); list_it++){
    cout << "   "
       << list_it->locationInfo.s_id.addr.hostname << " "     
           << ntohs(list_it->locationInfo.s_port) << endl;
  }

  cout << endl << endl;

  cout << "Socket Vector:" << endl; 
  for(unsigned int i = 0; i < server_sockets.size(); i++){
  		cout << "   " << server_sockets[i] << endl;
  }

}


////////////////////////////////////////////  MAIN   ///////////////////////////////////////////////////


int main(int argc, const char * argv[]) {
    Binder binder;
    binder.start();
    return 0;
}



