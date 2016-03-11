/*
 * rpc.h
 *
 * This file implements all of the rpc related functions.
 */

#define _BSD_SOURCE


#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "defs.h"
#include "debug.h"
#include "procedure_list.h"
#include "int_queue.h"
#include "rpc.h"


// Definitions for Cache implementation
#define		CACHE_SIZE		64
struct locations {
	//proc_sig proc_sig_arr[CACHE_SIZE];
	location loc_arr[CACHE_SIZE];
	unsigned next_index;
	unsigned num_valid_entries;
};

// Cache
static struct locations loc_cache;




// these are modified only in register and init stages.
// Multiple threads accessing these data is ok during execution of rpcExecute()
static struct proc_node *proc_list = NULL;
static location my_loc;
static int listening_sock;
static int binder_sock;

// this is critical region
static intQueue intQ;

// worker threads to be able to handle multiple requests at a time
static pthread_t worker_threads[NUM_THREADS];
static int num_created_threads = 0;



/* 	
	Server side function to create a listening socket to
	be able to accept connection request from the clients
*/
int create_listening_sock(){
	DEBUG("create_listening_sock is called");
	
	// set up a listening socket for clients
	struct sockaddr_in server;
	memset(&server, 0, sizeof(struct sockaddr_in));
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = 0;
	
	// create the socket
	listening_sock = socket (AF_INET, SOCK_STREAM, 0);
	
	if (listening_sock == -1) {
		ERROR("ERROR: could not create a listening socket! Exiting...");
		return ERR_RPC_SERVER_SOCK_FAILED;
	}
	
	// bind the socket to the sockaddr
	int ret = bind(listening_sock, (struct sockaddr *) &server, sizeof(server));
	if (ret != 0){
		ERROR("ERROR: could not bind the listening socket! Exiting...");
		return ERR_RPC_SERVER_SOCK_FAILED;
	}
	
	// start listening to be able to accept connection requests
	listen (listening_sock, SOMAXCONN);
	
	DEBUG("create_listening_sock is returning. listening_sock=%d", listening_sock);
	return ERR_RPC_SUCCESS;
}


/* 	
	Server side function to create a socket to the binder to be
	able to register procedures and receive termination signal
*/
int create_binder_sock(){
	DEBUG("create_binder_sock is called");
	
	// create a socket to connect to the binder
	struct sockaddr_in binder;
	binder_sock = socket (AF_INET, SOCK_STREAM, 0);
	if (binder_sock == -1) {
		ERROR("ERROR: could not create a binder_socket!!! Exiting...");
		return ERR_RPC_BINDER_SOCK_FAILED;
	}
	
	// get the hostname of the binder from the environment variable
	char *env = getenv("BINDER_ADDRESS");
	if (env == NULL) {
		ERROR("ERROR: NULL POINTER in env[addr] variable! Exiting...");
		return ERR_RPC_ENV_ADDR_NULL;
	}
	struct hostent *he = gethostbyname(env);
	memcpy (&binder.sin_addr, he->h_addr_list[0], he->h_length);
	
	binder.sin_family = AF_INET;
	
	// get the port number of the binder from the environment variable
	env = getenv("BINDER_PORT");
	if (env == NULL) {
		ERROR("ERROR: NULL POINTER in env[port] variable! Exiting...");
		return ERR_RPC_ENV_PORT_NULL;
	}
	binder.sin_port = htons (atoi(env));
	
	// connect to the binder
	if (connect (binder_sock, (struct sockaddr *)&binder, sizeof(binder)) < 0) {
		ERROR("ERROR: Could not connect to the binder! Exiting...");
		return ERR_RPC_BINDER_SOCK_FAILED;
	}
	
	DEBUG("create_binder_sock is returning. binder_sock=%d", binder_sock);
	return ERR_RPC_SUCCESS;
}


/*
	Server side function to initialize my_loc that contains
	the hostname and port number of the server
*/
int setup_my_loc(){
	DEBUG("setup_my_loc is called");
	// choose a hostname type and fill in the hostname field in my_loc
	my_loc.s_id.addr_type = ADDR_TYPE_HOSTNAME;
	
	// store the hostname in hostname field
	gethostname (my_loc.s_id.addr.hostname, MAX_HOSTNAME_SIZE*sizeof(char));
	my_loc.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';
	DEBUG("SERVER_ADDRESS %s", my_loc.s_id.addr.hostname);
	
	// fill in the port field in my_loc
	int addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in s;
	memset(&s, 0, sizeof(struct sockaddr_in));
	if (getsockname (listening_sock, (struct sockaddr *)&s, (socklen_t*)&addrlen) < 0){
		DEBUG("returning  ERR_RPC_SERVER_SOCK_FAILED");
		return ERR_RPC_SERVER_SOCK_FAILED;
	}
	my_loc.s_port = s.sin_port;
	
	DEBUG("SERVER_PORT %d", ntohs(my_loc.s_port));
	DEBUG("setup_my_loc is returning");
	return ERR_RPC_SUCCESS;
}


/*
	
*/
unsigned size_of_arg (int arg_type) {
	unsigned size = 0;
	switch ((arg_type >> 16) & 0xff) {
		case ARG_CHAR:
			size = sizeof(char);
			break;
		case ARG_SHORT:
			size = sizeof(short);
			break;
		case ARG_INT:
			size = sizeof(int);
			break;
		case ARG_LONG:
			size = sizeof(long);
			break;
		case ARG_DOUBLE:
			size = sizeof(double);
			break;
		case ARG_FLOAT:
			size = sizeof(float);
			break;
		default:
			ERROR("Unknown argument type!");
			break;
	}
	// if the last byte is 0, then it is scalar otherwise an array of length [last byte]
	if ((arg_type & 0xff) == 0) return size;
	return size*(arg_type & 0xff);
}



unsigned find_args_size (unsigned arglen, int* argTypes, unsigned *arg_sizes) {
	//DEBUG("find_args_size is called");
	unsigned total_arg_size = 0;
	for (unsigned i = 0; i < arglen; i++) {
		arg_sizes[i] = size_of_arg (argTypes[i]);
		total_arg_size += arg_sizes[i];
	}
	//DEBUG("find_args_size is returning");
	return total_arg_size;
}



unsigned find_args_total_size (unsigned arglen, int* argTypes) {
	//DEBUG("find_args_size is called");
	unsigned total_arg_size = 0;
	for (unsigned i = 0; i < arglen; i++) {
		total_arg_size += size_of_arg (argTypes[i]);
	}
	//DEBUG("find_args_size is returning");
	return total_arg_size;
}



int unmarshall_args_no_alloc (unsigned arglen, int *argTypes, char *args_buff, void **args) {
	//DEBUG("unmarshall_args_no_alloc is called    arglen=%d", arglen);
	unsigned size_arg = 0;
	for (unsigned i = 0; i < arglen; i++){
		size_arg = size_of_arg (argTypes[i]);
		memcpy(args[i], args_buff, size_arg);
		args_buff += size_arg;
	}
	//DEBUG("unmarshall_args_no_alloc is returning");
	return ERR_RPC_SUCCESS;
}



int unmarshall_args (unsigned arglen, int *argTypes, char *args_buff, void **args) {
	//DEBUG("unmarshall_args is called    arglen=%d", arglen);
	unsigned size_arg = 0;
	for (unsigned i = 0; i < arglen; i++){
		size_arg = size_of_arg (argTypes[i]);
		args[i] = malloc(size_arg);
		if (args[i] == NULL) {
			// cleanup before returning error
			for (unsigned j = 0; j < i; j++) free(args[j]);
			ERROR("unmarshall_args is returning RR_RPC_OUT_OF_MEMORY");
			return ERR_RPC_OUT_OF_MEMORY;
		}
		memcpy(args[i], args_buff, size_arg);
		args_buff += size_arg;
	}
	//DEBUG("unmarshall_args is returning succesfully");
	return ERR_RPC_SUCCESS;
}



int marshall_args (unsigned arglen, int *argTypes, char *args_buff, void **args) {
	//DEBUG("marshall_args is called");
	unsigned size_arg = 0;
	for (unsigned i = 0; i < arglen; i++){
		size_arg = size_of_arg (argTypes[i]);
		memcpy(args_buff, args[i], size_arg);
		args_buff += size_arg;
	}
	//DEBUG("marshall_args is returning");
	return ERR_RPC_SUCCESS;
}



void destroy_args (unsigned arglen, void **args, void **args_origin) {
	//DEBUG("destroy_args is called");
	for (unsigned i = 0; i < arglen; i++) {
		// deallocate the memory allocated in skeleton
		if (args[i] != args_origin[i]) free(args[i]);
		
		// deallocate the memory allocated for procedure invocation
		free(args_origin[i]);
	}
	free(args);
	free(args_origin);
	//DEBUG("destroy_args is returning");
}



int send_error_msg (char *reply_buff, int error, int sock) {
	DEBUG("send_error_msg is called");
	*((unsigned *)reply_buff) = sizeof(int);
	*((int *)(reply_buff+sizeof(unsigned))) = EXECUTE_FAILURE;
	*((int *)(reply_buff+sizeof(unsigned)+sizeof(int))) = error;
	
	//send the message
	int ret = send (sock, reply_buff, sizeof(unsigned)+(2*sizeof(int)), 0);
	
	if (ret < 0){
		ERROR("ERROR: SENDING reply (EXECUTE_FAILURE) to the client failed!");
		return ERR_RPC_SOCKET_FAILED;
	}
	DEBUG("send_error_msg is returning");
	return ERR_RPC_SUCCESS;
}



char *proc_sig_unmarshal (proc_sig *sig, char *msg_offset){
	memset (sig, 0, sizeof(proc_sig));
	strncpy(sig->proc_name, msg_offset, MAX_PROC_NAME_SIZE);
	sig->proc_name[MAX_PROC_NAME_SIZE] = '\0';
	
	msg_offset += (MAX_PROC_NAME_SIZE+1)*sizeof(char);
	sig->argLen = *((unsigned *)msg_offset);
	
	msg_offset += sizeof(unsigned);
	sig->argTypes = (int*)msg_offset;
	return msg_offset;
}


int recv_loop (int sock, void *buff, unsigned recvlen){
	int bytesRcvd = 0, totalBytesRcvd = 0;
	
	while (totalBytesRcvd < recvlen){
		bytesRcvd = recv(sock, buff+totalBytesRcvd, recvlen-totalBytesRcvd, 0);
		
		if (bytesRcvd <= 0) {
			ERROR("returning error. bytes rcvd is  <= 0 !");
			return bytesRcvd;
		}
		
		totalBytesRcvd += bytesRcvd;
	}
	
	return totalBytesRcvd;
}


int handle_request (int sock){
	DEBUG("handle_request is called");
	// receive the data and reply with the response
	unsigned msg_len = 0, total_len = 0;
	int bytesRcvd = 0, msg_type = 0, ret_code = ERR_RPC_SUCCESS;
	
	bytesRcvd = recv_loop(sock, &msg_len, 4);
	if (bytesRcvd < 0) {
		ERROR("bytes rcvd is negative!");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	bytesRcvd = recv_loop(sock, &msg_type, 4);
	if (bytesRcvd < 0) {
		ERROR("bytes rcvd is negative!");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	total_len = sizeof(unsigned) + sizeof(int) + msg_len;
	char *msg_req = malloc(total_len);
	if (msg_req == NULL) return ERR_RPC_OUT_OF_MEMORY;
	memset(msg_req, 0, total_len);
	
	char *msg_offset = msg_req+sizeof(unsigned)+sizeof(int);
	bytesRcvd = recv_loop(sock, msg_offset, msg_len);
	if (bytesRcvd < 0) {
		ERROR("bytes rcvd is negative!");
		free(msg_req);
		return ERR_RPC_SOCKET_FAILED;
	}
	
	if (msg_type != EXECUTE){
		send_error_msg (msg_req, ERR_RPC_UNEXPECTED_MSG_TYPE, sock);
		free(msg_req);
		ERROR("Unknown request type from the client");
		return ERR_RPC_UNEXPECTED_MSG_TYPE;
	}
	
	// unmarshal the message to find skel corresponding to name and argTypes
	proc_sig sig;
	msg_offset = proc_sig_unmarshal (&sig, msg_offset);
	
	skel f = find_skel (&proc_list, &sig);
	if (f == NULL) {
		send_error_msg (msg_req, ERR_RPC_UNREGISTERED_PROC, sock);
		free(msg_req);
		ERROR("Returning ERR_RPC_UNREGISTERED_PROC");
		return ERR_RPC_UNREGISTERED_PROC;
	}
	void **args 		= (void**)malloc(sig.argLen*(sizeof(void*)));
	void **args_origin 	= (void**)malloc(sig.argLen*(sizeof(void*)));
	if (args == NULL || args_origin == NULL) {
		send_error_msg (msg_req, ERR_RPC_OUT_OF_MEMORY, sock);
		free(msg_req);
		if (args) free(args);
		if (args_origin) free(args_origin);
		ERROR("Returning ERR_RPC_OUT_OF_MEMORY");
		return ERR_RPC_OUT_OF_MEMORY;
	}
	
	msg_offset += (sig.argLen+1)*sizeof(int);
	ret_code = unmarshall_args(sig.argLen, sig.argTypes, msg_offset, args);
	if (ret_code == ERR_RPC_OUT_OF_MEMORY){
		send_error_msg (msg_req, ERR_RPC_OUT_OF_MEMORY, sock);
		free(msg_req);
		free(args);
		free(args_origin);
		ERROR("Returning ERR_RPC_OUT_OF_MEMORY");
		return ERR_RPC_OUT_OF_MEMORY;
	}
	// copy original args before envoking the procedure to avoid memory leaks
	memcpy(args_origin, args, sig.argLen*(sizeof(void*)));
	
	DEBUG("Invoking the proc!  name:%s    skeleton:%p", sig.proc_name, (void*)f);
	ret_code = f(sig.argTypes, args);
	DEBUG ("Result of function call:%d    args[0]=%d", ret_code, *(int*)args[0]);
	
	if (ret_code) {	// function returned error
		ERROR("skeleton returned error! ret=%d", ret_code);
		send_error_msg (msg_req, ERR_RPC_PROC_EXEC_FAILED, sock);
		free(msg_req);
		destroy_args (sig.argLen, args, args_origin);
		ERROR("returning ERR_RPC_PROC_EXEC_FAILED");
		return ERR_RPC_PROC_EXEC_FAILED;
	}
	*((unsigned *)msg_req) = msg_len;
	*((int *)(msg_req+sizeof(unsigned))) = EXECUTE_SUCCESS;
	msg_offset = msg_req+sizeof(unsigned)+sizeof(int)+(MAX_PROC_NAME_SIZE+1)*sizeof(char)
				+sizeof(unsigned)+(sig.argLen+1)*sizeof(int);
	// copy args
	marshall_args (sig.argLen, sig.argTypes, msg_offset, args);
	
	//send the success message
	ret_code = send (sock, msg_req, total_len, 0);
	if (ret_code < 0){
		ERROR("SENDING reply (EXECUTE_FAILURE) to the client failed!");
		ret_code = ERR_RPC_SOCKET_FAILED;
	}
	else ret_code = ERR_RPC_SUCCESS;
	
	// free dynamic memory allocated
	free(msg_req);
	destroy_args (sig.argLen, args, args_origin);
	DEBUG("handle_request is returning...");
	return ret_code;
}



void *worker_code (void *ptr){
	DEBUG("worker_code is called...");
	int sock = 0, ret_code = 0;
	
	while (1){
		DEBUG("worker_code: popping socket...");
		ret_code = queue_pop (&intQ, &sock);
		DEBUG("worker_code: popped socket:%d", sock);
		if (ret_code != ERR_QUEUE_SUCCESS) continue;
		
		
		// if sock == -1, then termination signal has been sent.
		if (sock == -1) break;
		ret_code = handle_request(sock);
	}
	
	DEBUG("worker_code is returning...");
	return ERR_RPC_SUCCESS;
}



int rpcInit() {
	DEBUG("rpcInit() is called...");
	proc_list = NULL;
	memset (&my_loc, 0, sizeof(location));
	queue_init(&intQ);
	
	int ret = create_listening_sock();
	if (ret != ERR_RPC_SUCCESS){
		ERROR("rpcInit() is returning... listening_sock failed");
		return ret;
	}
	
	ret = create_binder_sock();
	if (ret != ERR_RPC_SUCCESS){
		close(listening_sock);
		ERROR("rpcInit() is returning... binder_sock failed");
		return ret;
	}
	
	setup_my_loc();
	
	for (int i = 0; i < NUM_THREADS; i++){
		int ret = pthread_create (&worker_threads[i], NULL, worker_code, NULL);
		if (ret < 0) {
			num_created_threads = i;
			ERROR("ERROR: Could not create a worker thread! Exiting...");
			return ERR_RPC_THREAD_NOT_CREATED;
		}
	}
	
	DEBUG("rpcInit() is returning...");
	return ERR_RPC_SUCCESS;
}



int send_termination_request (){
	DEBUG("send_termination_request() is called...");
	unsigned total_len = sizeof(unsigned)+2*sizeof(int);
	char *msg = malloc(total_len);
	if (msg == NULL) {
		ERROR("returning ERR_RPC_OUT_OF_MEMORY");
		return ERR_RPC_OUT_OF_MEMORY;
	}
	memset(msg, 0, total_len);
	*(unsigned *)(msg) = sizeof(int);
	*((int *)(msg+sizeof(unsigned))) = TERMINATE;
	*((int *)(msg+sizeof(unsigned)+sizeof(int))) = ERR_RPC_SUCCESS;
	int ret = send (binder_sock, msg, total_len, 0);
	free(msg);
	
	if (ret < 0){
		ERROR("ERROR: SENDING termination request to the binder failed!");
		return ERR_RPC_BINDER_SOCK_FAILED;
	}
	DEBUG("send_termination_request() is returning...");
	return ERR_RPC_SUCCESS;
}



int rpcTerminate(){
	DEBUG("rpcTerminate() is called...");
	int ret;
	ret = create_binder_sock();
	if (ret != ERR_RPC_SUCCESS) {
		ERROR("returning... create_binder_sock() failed!");
		return ret;
	}
	ret = send_termination_request();
	close(binder_sock);
	DEBUG("rpcTerminate() is returning...");
	return ret;
}



int accept_new_connection () {
	DEBUG("accept_new_connection () is called...");
	// accept a new connection request to the listening socket
	struct sockaddr_in client;
	int addrlen = sizeof(struct sockaddr_in);
	int new_sock = accept(listening_sock, (struct sockaddr *)&client, (socklen_t*)&addrlen);
	if (new_sock < 0) {
		ERROR("accept_new_connection () is returning...accept failed");
		return ERR_RPC_SERVER_SOCK_FAILED;
	}
	DEBUG("accept_new_connection () is returning...new_sock=%d", new_sock);
	// Accept is successful. Push the socket into the queue so that the worker threads can serve
	return queue_push (&intQ, new_sock);
}



int check_termination_protocol(int *termination){
	DEBUG("check_termination_protocol () is called...");
	// rcv msg from binder and check if it is termination request.
	// If yes terminate by setting *termination to 1, resume execution otherwise.
	int bytesRcvd, msg_type, reason_code; unsigned msg_len;
	bytesRcvd = recv_loop(binder_sock, &msg_len, 4);
	DEBUG("bytesRcvd=%d", bytesRcvd);
	if (bytesRcvd == 0){
		*termination = 1;
		DEBUG("returning ERR_RPC_BINDER_SOCK_CLOSED");
		return ERR_RPC_BINDER_SOCK_CLOSED;
	}
	if (bytesRcvd < 0) {
		ERROR("ERROR: bytes rcvd is negative!");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	bytesRcvd = recv_loop(binder_sock, &msg_type, 4);
	DEBUG("bytesRcvd=%d", bytesRcvd);
	if (bytesRcvd < 0) {
		ERROR("ERROR: bytes rcvd is negative!");
		return ERR_RPC_BINDER_SOCK_FAILED;
	}
	
	bytesRcvd = recv_loop(binder_sock, &reason_code, 4);
	DEBUG("bytesRcvd=%d", bytesRcvd);
	if (bytesRcvd < 0) {
		ERROR("ERROR: bytes rcvd is negative!");
		return ERR_RPC_BINDER_SOCK_FAILED;
	}
	
	if (msg_type != TERMINATE){
		DEBUG("returning ERR_RPC_UNEXPECTED_MSG_TYPE | msg_len=%d msg_type=%d reason_code=%d", msg_len, msg_type, reason_code);
		// for (volatile int i = 0; 1==1; i++);		// crash();
		return ERR_RPC_UNEXPECTED_MSG_TYPE;
	}
	else{
		*termination = 1;
		DEBUG("check_termination_protocol () is returning successfully...");
		return ERR_RPC_SUCCESS;
	}
}



// Build the FD_SET for the select system call. Returns highest sock
int select_list_setup (fd_set *fds) {
	DEBUG("select_list_setup is called");
	FD_ZERO(fds);
	FD_SET(listening_sock, fds);
	FD_SET(binder_sock, fds);
	DEBUG("select_list_setup is returning. highest_sock: %d", ((listening_sock > binder_sock) ? listening_sock : binder_sock));
	return ((listening_sock > binder_sock) ? listening_sock : binder_sock);
}



// process sockets that are active
static inline int process_sockets (fd_set *fds, int *termination) {
	DEBUG("process_sockets () is called...");
	int ret = ERR_RPC_SUCCESS;
	if (FD_ISSET (listening_sock, fds)) {
		/*ret = */accept_new_connection ();
	}
	if (FD_ISSET (binder_sock, fds)) {
		ret = check_termination_protocol (termination);
	}
	return ret;
	DEBUG("process_sockets () is returning...");
}



static inline int terminate_worker_threads (){
	DEBUG("terminate_worker_threads () is called...");
	// send termination signal to worker threads
	for (int i = 0; i < num_created_threads; i++){
		if (queue_push (&intQ, -1)) return ERR_RPC_OUT_OF_MEMORY;
	}
	
	// wait for workers' termination
	for (int i = 0; i < num_created_threads; i++){
		pthread_join (worker_threads[i], NULL);
	}
	DEBUG("terminate_worker_threads () is returning...");
	return ERR_RPC_SUCCESS;
}



int rpcExecute(){
	DEBUG("rpcExecute() is called...");
	// if not reqgistered any procedure by calling rpcRegister(), return an error
	if (proc_list == NULL){
		ERROR("rpcExecute() is returning Exec is called before reg...");
		return ERR_RPC_EXEC_BEFORE_REG;
	}
	int termination = 0, ret_code = ERR_RPC_SUCCESS;
	int highest_sock, num_active_socks;
	fd_set fds;
	
	while (1){
		highest_sock = select_list_setup (&fds);
		DEBUG("select() is called!");
		num_active_socks = select(highest_sock+1, &fds, 0, 0, 0);
		DEBUG("select() returned! num_active_socks=%d", num_active_socks);
		if (num_active_socks < 0) {
			ERROR("ERROR: Select returned error! No active sockets! Exiting...");
			ret_code = ERR_RPC_SOCKS_INACTIVE;
			break;
		}
		else if (num_active_socks != 0) {
			ret_code = process_sockets (&fds, &termination);
			if (termination || ret_code) break;
		}
	}
	
	// cleanup
	close(listening_sock);
	close(binder_sock);
	
	terminate_worker_threads ();
	delete_all (&proc_list);
	
	// if there is something left in the queue, then clean
	queue_reset (&intQ);
	DEBUG("rpcExecute() is returning...");
	return ret_code;
}



void modify_argTypes (unsigned arglen, int* argTypes){
	for (unsigned i = 0; i < arglen; i++){
		if (argTypes[i] & 0xff) {
			argTypes[i] &= 0xffffff01;		// set the length of the array to 1 for simplicity
		}
	}
}



static inline unsigned arglen (int *argTypes){
	unsigned arglen;
	for (arglen = 0; argTypes[arglen] != 0; arglen++);
	return arglen;
}

static inline void marshall_binder_reg_msg (char *msg, unsigned msg_len, char* name, int argLen, int* argTypes){
	// marshall the registration details into the message
	char *msg_offset = msg;
	*((unsigned *) msg_offset) = msg_len;
	
	msg_offset += sizeof(unsigned);
	*((int *) msg_offset) = REGISTER;
	
	msg_offset += sizeof(int);
	memcpy(msg_offset, &my_loc, sizeof(location));
	
	msg_offset += sizeof(location);
	strncpy(msg_offset, name, MAX_PROC_NAME_SIZE);
	
	msg_offset += (MAX_PROC_NAME_SIZE+1)*sizeof(char);
	*((unsigned *) msg_offset) = argLen;
	
	msg_offset += sizeof(unsigned);
	memcpy(msg_offset, argTypes, (argLen+1)*sizeof(int));
}

char *create_binder_reg_msg (char* name, int argLen, int* argTypes){
	unsigned msg_len = sizeof(location) + (MAX_PROC_NAME_SIZE+1)*sizeof(char) + (1+argLen+1)*sizeof(int);
	unsigned total_len = sizeof(unsigned) + sizeof(int) + msg_len;
	
	// allocate a space for the message
	char *msg = malloc(total_len);
	if (msg == NULL){
		ERROR("create_binder_reg_msg() is returning NULL...");
		return NULL;
	}
	memset (msg, 0, total_len);
	
	// marshall the registration details into the message
	marshall_binder_reg_msg (msg, msg_len, name, argLen, argTypes);
	
	// return dynamically allocated message
	return msg;
}



int binder_reg_send_rcv(char *msg, unsigned total_len){
	// send msg to the binder and receive the result
	if (send (binder_sock, msg, total_len, 0) < 0) {
		ERROR("ERROR: Sending register msg to the binder failed!");
		return ERR_RPC_BINDER_SOCK_FAILED;
	}
	
	// receive the result from the binder
	int bytesRcvd, rcv_len, msg_type, reason_code;
	
	// receive the length of the message
	bytesRcvd = recv_loop(binder_sock, &rcv_len, 4);
	if (bytesRcvd < 0) {
		ERROR("bytesRcvd=%d", bytesRcvd);
		return ERR_RPC_BINDER_SOCK_FAILED;
	}
	
	// receive the type of the message
	bytesRcvd = recv_loop(binder_sock, &msg_type, 4);
	if (bytesRcvd < 0) {
		ERROR("bytesRcvd=%d", bytesRcvd);
		return ERR_RPC_BINDER_SOCK_FAILED;
	}
	
	// receive the reason code for failure, warning or success
	bytesRcvd = recv_loop(binder_sock, &reason_code, 4);
	if (bytesRcvd < 0) {
		ERROR("bytesRcvd=%d", bytesRcvd);
		return ERR_RPC_BINDER_SOCK_FAILED;
	}
	
	if (msg_type == REGISTER_FAILURE){
		ERROR("rpcRegister() is returning... REGISTER_FAILURE");
		return reason_code;
	}
	
	else if (msg_type != REGISTER_SUCCESS){
		ERROR("rpcRegister() is returning... ERR_RPC_UNEXPECTED_MSG_TYPE");
		return ERR_RPC_UNEXPECTED_MSG_TYPE;
	}
	return ERR_RPC_SUCCESS;
}



int binder_registration(char* name, int argLen, int* argTypes){
	DEBUG("binder_registration is called");
	// prepare the register request to the binder
	unsigned msg_len = sizeof(location) + (MAX_PROC_NAME_SIZE+1)*sizeof(char) + (1+argLen+1)*sizeof(int);
	unsigned total_len = sizeof(unsigned) + sizeof(int) + msg_len;
	
	// create a message by marshalling function details
	char *msg = create_binder_reg_msg (name, argLen, argTypes);
	if (msg == NULL){
		ERROR("binder_registration() is returning OUT_OF_MEMORY...");
		return ERR_RPC_OUT_OF_MEMORY;
	}
	
	// send msg to the binder and receive the result
	int ret = binder_reg_send_rcv(msg, total_len);
	free(msg);
	if (ret) {
		ERROR("binder_reg_send_rcv returned ret=%d", ret);
	}
	
	DEBUG("binder_registration is returning ...");
	return ret;
}



int local_registration(char *name, int argLen, int *argTypes, skeleton f){
	// prepare everything needed for local registration
	proc_sig sig;
	memset (&sig, 0, sizeof(proc_sig));
	strncpy(sig.proc_name, name, MAX_PROC_NAME_SIZE);
	sig.proc_name[MAX_PROC_NAME_SIZE] = '\0';
	sig.argLen = argLen;
	sig.argTypes = argTypes;
	
	//register procedure in the local database
	int ret = update (&proc_list, &sig, f);
	if(ret < 0) { 		// failure
		ERROR("Update() returned with failure: %d", ret);
	}
	if(ret > 0) {		// warning
		WARNING("Update() returned with update: %d", ret);
	}
	return ret;
}



int rpcRegister(char *name, int *argTypes, skeleton f) {
	DEBUG("rpcRegister is called!  name:%s    argTypes:%p    skeleton:%p", name, (void*)argTypes, (void*)f);
	// find arglen
	unsigned argLen = arglen (argTypes);
	int ret = binder_registration(name, argLen, argTypes);
	if (ret == ERR_RPC_SUCCESS || ret == ERR_RPC_PROC_RE_REG){
		int ret2 = local_registration(name, argLen, argTypes, f);
		if (ret2 != ERR_RPC_SUCCESS && ret2 != ERR_RPC_PROC_RE_REG) {
		DEBUG("rpcRegister is returning ret2=%d!", ret2); 
			return ret2;
		}
	}
	DEBUG("rpcRegister is returning ret=%d!", ret); 
	return ret;
}

/*
// Definitions for Cache implementation
#define		CACHE_SIZE		64
struct locations {
	location loc_arr[CACHE_SIZE];
	unsigned next_index;
	unsigned num_valid_entries;
};

// Cache
static struct locations loc_cache;
*/

// INCOMPLETE
int cacheUpdate (char *rcv_buff, unsigned msg_len){
	location *dst_loc;
	unsigned num_new_locs = msg_len/sizeof(location);
	if (num_new_locs == 0) return ERR_RPC_INCOMPLETE_MSG;
	for (unsigned i = 0; i < num_new_locs; i++){
		dst_loc = &loc_cache.loc_arr[(loc_cache.next_index+i)%CACHE_SIZE];
		memcpy(dst_loc, rcv_buff+(i*sizeof(location)), sizeof(location));
		loc_cache.num_valid_entries++;
	}
	loc_cache.next_index = (loc_cache.next_index+num_new_locs)%CACHE_SIZE;
	
	return ERR_RPC_SUCCESS;
}

// INCOMPLETE
int locateFromCache(location *server_loc){
	
	return ERR_RPC_SUCCESS;
}

int binder_loc_req(char *msg, unsigned total_len, location *server_loc, int isCache){
	DEBUG("binder_loc_req() is called...");
	
	if (send (binder_sock, msg, total_len, 0) < 0) {
		ERROR("ERROR: Sending register msg to the binder failed!");
		return ERR_RPC_BINDER_SOCK_FAILED;
	}
	
	unsigned msg_len = 0;
	int bytesRcvd = 0, msg_type = 0, ret_code = ERR_RPC_SUCCESS;
	
	bytesRcvd = recv_loop(binder_sock, &msg_len, 4);
	if (bytesRcvd < 0) {
		ERROR("returning ERR_RPC_SOCKET_FAILED. bytesRcvd=%d", bytesRcvd);
		return ERR_RPC_SOCKET_FAILED;
	}
	
	bytesRcvd = recv_loop(binder_sock, &msg_type, 4);
	if (bytesRcvd < 0) {
		ERROR("returning ERR_RPC_SOCKET_FAILED. bytesRcvd=%d", bytesRcvd);
		return ERR_RPC_SOCKET_FAILED;
	}
	
	char *rcv_buff = malloc(msg_len*sizeof(char));
	if (rcv_buff == NULL){
		ERROR("returning OUT_OF_MEMORY");
		return ERR_RPC_OUT_OF_MEMORY;
	}
	memset(rcv_buff, 0, msg_len*sizeof(char));
	
	bytesRcvd = recv_loop(binder_sock, rcv_buff, msg_len);
	if (bytesRcvd < 0) {
		ERROR("returning ERR_RPC_SOCKET_FAILED. bytesRcvd=%d", bytesRcvd);
		free(rcv_buff);
		return ERR_RPC_SOCKET_FAILED;
	}
	
	if (msg_type == LOC_FAILURE) {
		ERROR("Received: LOC_FAILURE");
		ret_code = *((int *)(rcv_buff));
	}
	else if (msg_type != LOC_SUCCESS) {
		ERROR("Received: UNEXPECTED_MSG_TYPE");
		ret_code = ERR_RPC_UNEXPECTED_MSG_TYPE;
	}
	else {
		// received the location data successfully
		if (isCache) {	// INCOMPELTE AND NOT USED
			// copy the data received into the cache
			cacheUpdate (rcv_buff, msg_len);
			
			// store the next server location into the server_loc
			locateFromCache(server_loc);
		}
		else {
			// store the server location into the server_loc
			memcpy(server_loc, rcv_buff, sizeof(location));
		}
		DEBUG("Received: LOC_SUCCESS. host:%s\tport=%d", server_loc->s_id.addr.hostname, ntohs(server_loc->s_port));
	}
	
	free(rcv_buff);
	DEBUG("binder_loc_req() is returning... ret_code=%d", ret_code);
	return ret_code;
}



int create_server_connection (int *server_sock, location *server_loc){
	DEBUG("create_server_connection() is called...");
	// create a socket to connect to the binder
	struct sockaddr_in server;
	*server_sock = socket (AF_INET, SOCK_STREAM, 0);
	if (*server_sock == -1) {
		ERROR("ERROR: Client could not create a server_socket!!! Exiting...");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	struct hostent *he = gethostbyname(server_loc->s_id.addr.hostname);
	memcpy (&server.sin_addr, he->h_addr_list[0], he->h_length);
	server.sin_family = AF_INET;
	server.sin_port = server_loc->s_port;
	
	if (connect (*server_sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		ERROR("ERROR: Could not connect to the server! Exiting...");
		return ERR_RPC_SERVER_SOCK_FAILED;
	}
	
	DEBUG("create_server_connection() is returning... server_sock=%d", *server_sock);
	return ERR_RPC_SUCCESS;
}



int send_to_server (int *server_sock, char *msg, unsigned total_msg_len){
	DEBUG("send_to_server() is called...");
	int ret = send (*server_sock, msg, total_msg_len, 0);
	if (ret < 0){
		ERROR("send_to_server() is returning ERR_RPC_SOCKET_FAILED");
		return ERR_RPC_SERVER_SOCK_FAILED;
	}
	DEBUG("send_to_server() is returning... send => ret=%d", ret);
	return ERR_RPC_SUCCESS;
}




int rcv_from_server (int *server_sock, char *msg_response, unsigned total_msg_len){
	DEBUG("rcv_from_server() is called...");
	unsigned msg_len;
	int msg_type, bytesRcvd;
	
	bytesRcvd = recv_loop(*server_sock, &msg_len, sizeof(unsigned));
	if (bytesRcvd < 0) {
		ERROR("returning ERR_RPC_SOCKET_FAILED. bytesRcvd=%d", bytesRcvd);
		return ERR_RPC_SOCKET_FAILED;
	}
	
	bytesRcvd = recv_loop(*server_sock, &msg_type, sizeof(int));
	if (bytesRcvd < 0) {
		ERROR("returning ERR_RPC_SOCKET_FAILED. bytesRcvd=%d", bytesRcvd);
		return ERR_RPC_SOCKET_FAILED;
	}
	*((unsigned*)msg_response) = msg_len;
	*((int*)(msg_response+sizeof(unsigned))) = msg_type;
	
	bytesRcvd = recv_loop(*server_sock, msg_response+sizeof(unsigned)+sizeof(int), msg_len);
	if (bytesRcvd < 0){
		ERROR("rcv_from_server() is returning ERR_RPC_SOCKET_FAILED");
		return ERR_RPC_SERVER_SOCK_FAILED;
	}
	
	DEBUG("rcv_from_server() is returning... bytesRcvd=%d", bytesRcvd);
	return ERR_RPC_SUCCESS;
}



int handle_server_response (void** args, char *msg_response){
	DEBUG("handle_server_response is called!");
	// unsigned msg_len = *((unsigned *)msg_response);
	int msg_type = *((int *)(msg_response+sizeof(unsigned)));
	if (msg_type == EXECUTE_FAILURE) {
		ERROR("handle_server_response returns EXECUTE_FAILURE. reason_code=%d", *((int *)(msg_response+sizeof(unsigned)+sizeof(int))));
		return *((int *)(msg_response+sizeof(unsigned)+sizeof(int)));
	}
	else if (msg_type != EXECUTE_SUCCESS){
		ERROR("handle_server_response returns ERR_RPC_UNEXPECTED_MSG_TYPE");
		return ERR_RPC_UNEXPECTED_MSG_TYPE;		// unexpected response type
	}
	char *msg_offset = msg_response+sizeof(unsigned)+sizeof(int)+(MAX_PROC_NAME_SIZE+1)*sizeof(char);
	unsigned arglen = *((unsigned *)(msg_offset));
	unmarshall_args_no_alloc   (arglen, (int *)(msg_offset+sizeof(unsigned)),
								msg_offset+sizeof(unsigned)+(arglen+1)*sizeof(int), args);
	
	DEBUG("handle_server_response is returning...");
	return ERR_RPC_SUCCESS;
}



unsigned total_exec_msg_len (int* argTypes){
	// find argLen
	unsigned argLen = arglen (argTypes);
	
	// find the location request length
	unsigned loc_req_len = (MAX_PROC_NAME_SIZE+1)*sizeof(char) + (1+argLen+1)*sizeof(int);
	
	// find total msg_len including the header and args
	unsigned total_msg_len = sizeof(unsigned) + sizeof(int) + loc_req_len + find_args_total_size (argLen, argTypes);
	
	return total_msg_len;
}



void marshall_loc_req_msg(char* name, unsigned argLen, int* argTypes, unsigned loc_req_len, char *msg, int isCache){
	char *msg_offset = msg;
	*((unsigned *) msg_offset) = loc_req_len;
	
	msg_offset += sizeof(unsigned);
	*((int *) msg_offset) = LOC_REQUEST;
	
	if (isCache) *((int *) msg_offset) = LOC_REQUEST_CACHE;
	
	msg_offset += sizeof(int);
	strncpy(msg_offset, name, MAX_PROC_NAME_SIZE);
	
	msg_offset += (MAX_PROC_NAME_SIZE+1)*sizeof(char);
	*((unsigned *) msg_offset) = argLen;
	
	msg_offset += sizeof(unsigned);
	memcpy(msg_offset, argTypes, (argLen+1)*sizeof(int));
}



int locate_server(char* name, int* argTypes, void** args, char *msg, location *server_loc, int isCache) {
	// find arglen
	unsigned argLen = arglen (argTypes);
	
	// find the location request length
	unsigned loc_req_len = (MAX_PROC_NAME_SIZE+1)*sizeof(char) + (1+argLen+1)*sizeof(int);
	
	marshall_loc_req_msg(name, argLen, argTypes, loc_req_len, msg, isCache);
	
	char *msg_offset = msg 	+ sizeof(unsigned) + sizeof(int) + (MAX_PROC_NAME_SIZE+1)*sizeof(char)
							+ sizeof(unsigned) + (argLen+1)*sizeof(int);
	// marshall args for later use (execution request)
	marshall_args (argLen, argTypes, msg_offset, args);
	
	// send the location request to the binder to locate a server for procedure call
	int ret;
	ret = create_binder_sock();
	if (ret != ERR_RPC_SUCCESS) {
		ERROR("locate_server() is returning. create_binder_sock() failed...");
		return ret;
	}
	ret = binder_loc_req(msg, sizeof(unsigned) + sizeof(int) + loc_req_len, server_loc, isCache);
	close(binder_sock);
	
	if (ret != ERR_RPC_SUCCESS) {
		ERROR("locate_server() is returning. binder_loc_req() failed...");
		return ret;
	}
	
	
	return ERR_RPC_SUCCESS;
}



int proc_call_to_server (location *server_loc, char *msg, char *msg_response, unsigned total_msg_len){
	// send the request to the server
	int server_sock = 0;
	int ret = create_server_connection (&server_sock, server_loc);
	if (ret != ERR_RPC_SUCCESS) {
		ERROR("proc_call_to_server() is returning. create_server_connection() failed...");
		return ret;
	}
	
	ret = send_to_server (&server_sock, msg, total_msg_len);
	if (ret != ERR_RPC_SUCCESS) {
		ERROR("proc_call_to_server() is returning. send_to_server() failed...");
		return ret;
	}
	
	ret = rcv_from_server (&server_sock, msg_response, total_msg_len);
	if (ret != ERR_RPC_SUCCESS) {
		ERROR("proc_call_to_server() is returning. rcv_from_server() failed...");
		return ret;
	}
	
	return ERR_RPC_SUCCESS;
}


int rpcCallOptionCache(char* name, int* argTypes, void** args, int isCache){
	
	DEBUG("rpcCallOptionCache() is called...");
	
	location server_loc;
	memset(&server_loc, 0, sizeof(location));
	
	// find the length of the message needed
	unsigned total_msg_len = total_exec_msg_len(argTypes);
	
	// allocate space for request 'execute' and response (location request is a submsg of execute)
	char *msg			= malloc(total_msg_len);
	char *msg_response 	= malloc(total_msg_len);
	if (msg == NULL || msg_response == NULL) {
		if (msg) free(msg);
		if (msg_response) free(msg_response);
		ERROR("rpcCallOptionCache() is returning. OUT_OF_MEMORY...");
		return ERR_RPC_OUT_OF_MEMORY;
	}
	memset (msg, 0, total_msg_len);
	memset (msg_response, 0, total_msg_len);
	
	// find the server location for execution request
	int ret = locate_server(name, argTypes, args, msg, &server_loc, isCache);
	if(ret != ERR_RPC_SUCCESS){
		free(msg);
		free(msg_response);
		ERROR("rpcCallOptionCache() is returning. locate_server() failed...");
		return ret;
	}
	
	// SUCCESSFULLY located the server. Prepare the execution request for later use (modify type and length of the msg)
	char * msg_offset = msg;
	*((unsigned *) msg_offset) = total_msg_len - (sizeof(unsigned) + sizeof(int));
	
	msg_offset += sizeof(unsigned);
	*((int *) msg_offset) = EXECUTE;
	
	// connect and make a execute request from the server
	ret = proc_call_to_server(&server_loc, msg, msg_response, total_msg_len);
	if(ret != ERR_RPC_SUCCESS){
		free(msg);
		free(msg_response);
		ERROR("rpcCallOptionCache() is returning. proc_call_to_server() failed...");
		return ret;
	}
	
	// handle the response received
	ret = handle_server_response (args, msg_response);
	if(ret != ERR_RPC_SUCCESS){
		free(msg);
		free(msg_response);
		ERROR("rpcCallOptionCache() is returning. handle_server_response() failed...");
		return ret;
	}
	
	free(msg);
	free(msg_response);
	
	DEBUG("rpcCallOptionCache() is returning...ret=%d", ret);
	return ret;
}



int rpcCall(char* name, int* argTypes, void** args) {
	return rpcCallOptionCache(name, argTypes, args, 0);
}



int rpcCacheCall(char* name, int* argTypes, void** args) {
	return rpcCallOptionCache(name, argTypes, args, 1);
}
