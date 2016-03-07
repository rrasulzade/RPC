/*
 * rpc.h
 *
 * This file implements all of the rpc related functions.
 */

#define _BSD_SOURCE

#define	NUM_THREADS		128

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



// these are modified only in register and init stages.
// Multiple threads accessing these data is ok during execution of rpcExecute()
struct proc_node *proc_list = NULL;
location my_loc;
int listening_sock;
int binder_sock;

// this is critical region
intQueue intQ;

// worker threads to be able to handle multiple requests at a time
pthread_t worker_threads[NUM_THREADS];




// Build the FD_SET for the select system call. Returns highest sock
int select_list_setup (fd_set *fds) {
	FD_ZERO(fds);
	FD_SET(listening_sock, fds);
	FD_SET(binder_sock, fds);
	return ((listening_sock > binder_sock) ? listening_sock : binder_sock);
}


int create_listening_sock(){
	// set up the sockets
	struct sockaddr_in server;
	memset(&server, 0, sizeof(struct sockaddr_in));
	
	// set up a listening socket for clients
	server.sin_family = AF_INET;
	server.sin_addr.s_addr = INADDR_ANY;
	server.sin_port = 0;
	listening_sock = socket (AF_INET, SOCK_STREAM, 0);
	
	if (listening_sock == -1) {
		DEBUG("ERROR: could not create a listening socket! Exiting...\n");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	// bind the socket to the sockaddr
	int ret = bind(listening_sock, (struct sockaddr *) &server, sizeof(server));
	if (ret != 0){
		DEBUG("ERROR: could not bind the listening socket! Exiting...\n");
		return ERR_RPC_SOCKET_FAILED;
	}
	// start listening to be able to accept connection requests
	listen (listening_sock, SOMAXCONN);
	
	return ERR_RPC_SUCCESS;
}

int create_binder_sock(){
	// create a socket to connect to the binder
	struct sockaddr_in binder;
	binder_sock = socket (AF_INET, SOCK_STREAM, 0);
	if (binder_sock == -1) {
		DEBUG("ERROR: could not create a binder_socket!!! Exiting...\n");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	char *env = getenv("BINDER_ADDRESS");
	if (env == NULL) {
		DEBUG("ERROR: NULL POINTER in env[addr] variable! Exiting...\n");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	struct hostent *he = gethostbyname(env);
	
	memcpy (&binder.sin_addr, he->h_addr_list[0], he->h_length);
	
	binder.sin_family = AF_INET;
	env = getenv("BINDER_PORT");
	if (env == NULL) {
		DEBUG("ERROR: NULL POINTER in env[port] variable! Exiting...\n");
		return -1;
	}
	binder.sin_port = htons (atoi(env));
	
	if (connect (binder_sock, (struct sockaddr *)&binder, sizeof(binder)) < 0) {
		DEBUG("ERROR: Could not connect to the binder! Exiting...\n");
		return -1;
	}
	
	return ERR_RPC_SUCCESS;
}


int setup_my_loc(){
	// choose a hostname type and fill in the hostname field in my_loc
	my_loc.s_id.addr_type = ADDR_TYPE_HOSTNAME;
	gethostname (my_loc.s_id.addr.hostname, MAX_HOSTNAME_SIZE*sizeof(char));
	my_loc.s_id.addr.hostname[MAX_HOSTNAME_SIZE] = '\0';
	DEBUG("SERVER_ADDRESS %s\n", my_loc.s_id.addr.hostname);
	
	// fill in the port field in my_loc
	int addrlen = sizeof(struct sockaddr_in);
	struct sockaddr_in s;
	memset(&s, 0, sizeof(struct sockaddr_in));
	getsockname (listening_sock, (struct sockaddr *)&s, (socklen_t*)&addrlen);
	my_loc.s_port = s.sin_port;
	DEBUG("SERVER_PORT %d\n", ntohs(my_loc.s_port));
	return ERR_RPC_SUCCESS;
}

unsigned size_of_arg (int arg_type) {
	DEBUG("size_of_arg is called");
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
			DEBUG("Unknown argument type!\n");
			break;
	}
	DEBUG("size_of_arg is returning");
	if ((arg_type & 0xff) == 0) return size;
	return size*(arg_type & 0xff);
}

unsigned find_args_size (unsigned arglen, int* argTypes, unsigned *arg_sizes) {
	DEBUG("find_args_size is called");
	unsigned total_arg_size = 0;
	for (unsigned i = 0; i < arglen; i++) {
		arg_sizes[i] = size_of_arg (argTypes[i]);
		total_arg_size += arg_sizes[i];
	}
	DEBUG("find_args_size is returning");
	return total_arg_size;
}

int unmarshall_args_no_alloc (unsigned arglen, int *argTypes, char *args_buff, void **args) {
	DEBUG("unmarshall_args_no_alloc is called    arglen=%d", arglen);
	unsigned size_arg = 0;
	for (unsigned i = 0; i < arglen; i++){
		size_arg = size_of_arg (argTypes[i]);
		DEBUG("args=%p  args[i]=%p  argTypes[i]=%x  size_arg=%d", (void*)args, args[i], argTypes[i], size_arg);
		DEBUG ("args_buff=%p", args_buff);
		memcpy(args[i], args_buff, size_arg);
		DEBUG("here");
		args_buff += size_arg;
	}
	DEBUG("unmarshall_args_no_alloc is returning");
	return ERR_RPC_SUCCESS;
}

int unmarshall_args (unsigned arglen, int *argTypes, char *args_buff, void **args) {
	DEBUG("unmarshall_args is called    arglen=%d", arglen);
	unsigned size_arg = 0;
	for (unsigned i = 0; i < arglen; i++){
		size_arg = size_of_arg (argTypes[i]);
		args[i] = malloc(size_arg);
		if (args[i] == NULL) {
			// cleanup before returning error
			for (unsigned j = 0; j < i; j++) free(args[j]);
			DEBUG("unmarshall_args is returning NULL");
			return ERR_RPC_OUT_OF_MEMORY;
		}
		memcpy(args[i], args_buff, size_arg);
		args_buff += size_arg;
	}
	DEBUG("unmarshall_args is returning succesfully");
	return ERR_RPC_SUCCESS;
}

int marshall_args (unsigned arglen, int *argTypes, char *args_buff, void **args) {
	DEBUG("marshall_args is called");
	unsigned size_arg = 0;
	for (unsigned i = 0; i < arglen; i++){
		size_arg = size_of_arg (argTypes[i]);
		memcpy(args_buff, args[i], size_arg);
		args_buff += size_arg;
	}
	DEBUG("marshall_args is returning");
	return ERR_RPC_SUCCESS;
}

void destroy_args (unsigned arglen, void **args, void **args_origin) {
	for (unsigned i = 0; i < arglen; i++) {
		// deallocate the memory allocated in skeleton
		if (args[i] != args_origin[i]) free(args[i]);
		
		// eallocate the memory allocated for procedure invocation
		free(args_origin[i]);
	}
	free(args);
	free(args_origin);
}

int send_error_msg (char *reply_buff, int error, int sock) {
	*((unsigned *)reply_buff) = sizeof(int);
	*((int *)(reply_buff+sizeof(unsigned))) = EXECUTE_FAILURE;
	*((int *)(reply_buff+sizeof(unsigned)+sizeof(int))) = error;
	
	//send the message
	int ret = send (sock, reply_buff, sizeof(unsigned)+(2*sizeof(int)), 0);
	
	if (ret < 0){
		DEBUG("ERROR: SENDING reply (EXECUTE_FAILURE) to the client failed!");
		return ERR_RPC_SOCKET_FAILED;
	}
	return ERR_RPC_SUCCESS;
}

int handle_request (int sock){
	// receive the data and reply with the response
	unsigned msg_len = 0;
	int bytesRcvd = 0, msg_type = 0, response_len = 0, ret_code = ERR_RPC_SUCCESS;
	
	bytesRcvd = recv(sock, &msg_len, 4, 0);
	if (bytesRcvd < 0) {
		DEBUG("ERROR: bytes rcvd is negative!");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	bytesRcvd = recv(sock, &msg_type, 4, 0);
	if (bytesRcvd < 0) {
		DEBUG("ERROR: bytes rcvd is negative!");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	char *msg_req = malloc(sizeof(unsigned) + sizeof(int) + msg_len);
	if (msg_req == NULL) return ERR_RPC_OUT_OF_MEMORY;
	
	char *msg_offset = msg_req+sizeof(unsigned)+sizeof(int);
	bytesRcvd = recv(sock, msg_offset, msg_len, 0);
	if (bytesRcvd < 0) {
		DEBUG("ERROR: bytes rcvd is negative!");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	if (msg_type != EXECUTE){
		DEBUG("Unknown request type from the client");
		send_error_msg (msg_req, ERR_RPC_UNEXPECTED_MSG_TYPE, sock);
		free(msg_req);
		return ERR_RPC_UNEXPECTED_MSG_TYPE;
	}
	
	// unmarshal the message to find skel corresponding to name and argTypes
	proc_sig sig;
	memset (&sig, 0, sizeof(proc_sig));
	strncpy(sig.proc_name, msg_offset, MAX_PROC_NAME_SIZE);
	sig.proc_name[MAX_PROC_NAME_SIZE] = '\0';
	
	msg_offset += (MAX_PROC_NAME_SIZE+1)*sizeof(char);
	sig.argLen = *((unsigned *)msg_offset);
	
	msg_offset += sizeof(unsigned);
	sig.argTypes = (int*)msg_offset;
	
	skel f = find_skel (&proc_list, &sig);
	if (f == NULL) {
		send_error_msg (msg_req, ERR_RPC_UNREGISTERED_PROC, sock);
		free(msg_req);
		return ERR_RPC_UNREGISTERED_PROC;
	}
	void **args = (void**)malloc(sig.argLen*(sizeof(void*)));
	if (args == NULL) {
		send_error_msg (msg_req, ERR_RPC_OUT_OF_MEMORY, sock);
		free(msg_req);
		return ERR_RPC_OUT_OF_MEMORY;
	}
	void **args_origin = (void**)malloc(sig.argLen*(sizeof(void*)));
	if (args_origin == NULL) {
		send_error_msg (msg_req, ERR_RPC_OUT_OF_MEMORY, sock);
		free(msg_req);
		free(args);
		return ERR_RPC_OUT_OF_MEMORY;
	}
	
	msg_offset += (sig.argLen+1)*sizeof(int);
	ret_code = unmarshall_args(sig.argLen, sig.argTypes, msg_offset, args);
	if (ret_code == ERR_RPC_OUT_OF_MEMORY){
		send_error_msg (msg_req, ERR_RPC_OUT_OF_MEMORY, sock);
		free(msg_req);
		free(args);
		free(args_origin);
		return ERR_RPC_OUT_OF_MEMORY;
	}
	// copy original args before envoking the procedure to avoid memory leaks
	memcpy(args_origin, args, sig.argLen*(sizeof(void*)));
	
	DEBUG("Invoking the proc!  name:%s    skeleton:%p", sig.proc_name, (void*)f);
	ret_code = f(sig.argTypes, args);
	DEBUG ("Result of function call:%d    args[0]=%d", ret_code, *(int*)args[0]);
	
	if (ret_code) {	// function returned error
		send_error_msg (msg_req, ERR_RPC_PROC_EXEC_FAILED, sock);
		free(msg_req);
		destroy_args (sig.argLen, args, args_origin);
		return ERR_RPC_PROC_EXEC_FAILED;
	}
	response_len = sizeof(unsigned)+sizeof(int)+msg_len;
	*((unsigned *)msg_req) = msg_len;
	*((int *)(msg_req+sizeof(unsigned))) = EXECUTE_SUCCESS;
	msg_offset = msg_req+sizeof(unsigned)+sizeof(int)+(MAX_PROC_NAME_SIZE+1)*sizeof(char)
				+sizeof(unsigned)+(sig.argLen+1)*sizeof(int);
	// copy args
	marshall_args (sig.argLen, sig.argTypes, msg_offset, args);
	
	//send the success message
	ret_code = send (sock, msg_req, response_len, 0);
	if (ret_code < 0){
		DEBUG("ERROR: SENDING reply (EXECUTE_FAILURE) to the client failed!");
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
	int sock = 0, ret_code = 0;
	
	while (1){
		ret_code = queue_pop (&intQ, &sock);
		if (ret_code != ERR_QUEUE_SUCCESS) continue;
		
		
		// if sock == -1, then termination signal has been sent.
		if (sock == -1) break;
		ret_code = handle_request(sock);
	}
	
	return ERR_RPC_SUCCESS;
}

int rpcInit() {
	proc_list = NULL;
	memset (&my_loc, 0, sizeof(location));
	queue_init(&intQ);
	
	int ret = create_listening_sock();
	if (ret != 0) return ret;
	
	ret = create_binder_sock();
	if (ret != 0){
		close(listening_sock);
		return ret;
	}
	
	setup_my_loc();
	
	for (int i = 0; i < NUM_THREADS; i++){
		int ret = pthread_create (&worker_threads[i], NULL, worker_code, NULL);
		if (ret < 0) {
			DEBUG("ERROR: Could not create a worker thread! Exiting...\n");
			return -1;
		}
	}
	return ERR_RPC_SUCCESS;
}

int send_termination_request (){
	char *msg = malloc(8*sizeof(char));
	if (msg == NULL) return ERR_RPC_OUT_OF_MEMORY;
	memset(msg, 0, 8*sizeof(char));
	(*(int *)(msg+sizeof(unsigned))) = TERMINATE;
	int ret = send (binder_sock, msg, sizeof(unsigned)+sizeof(int), 0);
	free(msg);
	
	if (ret < 0){
		DEBUG("ERROR: SENDING termination request to the binder failed!");
		return ERR_RPC_BINDER_NOT_FOUND;
	}
	return ERR_RPC_SUCCESS;
}

int rpcTerminate(){
	int ret;
	ret = create_binder_sock();
	if (ret != ERR_RPC_SUCCESS) return ret;
	
	ret = send_termination_request();
	close(binder_sock);
	return ret;
}

int accept_new_connection () {
	// accept a new connection request to the listening socket
	struct sockaddr_in client;
	int addrlen = sizeof(struct sockaddr_in);
	int new_sock = accept(listening_sock, (struct sockaddr *)&client, (socklen_t*)&addrlen);
	if (new_sock < 0) return ERR_RPC_SOCKET_FAILED;
	
	// Accept is successful. Push the socket into the queue so that the worker threads can serve
	return queue_push (&intQ, new_sock);
}

int check_termination_protocol(int *termination){
	// rcv msg from binder and check if it is termination request.
	// If yes terminate by setting *termination to 1, resume execution otherwise.
	int bytesRcvd, msg_type; unsigned msg_len;
	bytesRcvd = recv(binder_sock, &msg_len, 4, 0);
	bytesRcvd = recv(binder_sock, &msg_type, 4, 0);
	if (bytesRcvd < 0) {
		DEBUG("ERROR: bytes rcvd is negative!");
	}
	if (msg_type == TERMINATE) *termination = 1;
	return ERR_RPC_SUCCESS;
}

// process sockets that are active
void process_sockets (fd_set *fds, int *termination) {
	if (FD_ISSET (listening_sock, fds)) {
		accept_new_connection ();
	}
	if (FD_ISSET (binder_sock, fds)) {
		check_termination_protocol (termination);
	}
}

int terminate_worker_threads (){
	// send termination signal to worker threads
	for (int i = 0; i < NUM_THREADS; i++){
		queue_push (&intQ, -1);
	}
	
	// wait for workers' termination
	for (int i = 0; i < NUM_THREADS; i++){
		pthread_join (worker_threads[i], NULL);
	}
	return ERR_RPC_SUCCESS;
}

int rpcExecute(){
	// if not reqgistered any procedure by calling rpcRegister(), return an error
	if (proc_list == NULL) return ERR_RPC_EXEC_BEFORE_REG;
	
	int termination = 0;
	int highest_sock, num_active_socks;
	fd_set fds;
	
	while (1){
		highest_sock = select_list_setup (&fds);
		num_active_socks = select(highest_sock+1, &fds, 0, 0, 0);
		if (num_active_socks < 0) {
			DEBUG("ERROR: Select returned error! No active sockets! Exiting...\n");
			return ERR_RPC_SOCKET_INACTIVE;
		}
		else if (num_active_socks != 0) {
			process_sockets (&fds, &termination);
			if (termination) break;
		}
	}
	
	// cleanup
	close(listening_sock);
	close(binder_sock);
	
	terminate_worker_threads ();
	delete_all (&proc_list);
	
	// if there is something left in the queue, then clean
	queue_reset (&intQ);
	return 0;
}

void modify_argTypes (unsigned arglen, int* argTypes){
	DEBUG("modify_argTypes is called");
	for (unsigned i = 0; i < arglen; i++){
		if (argTypes[i] & 0xff) {
			argTypes[i] &= 0xffffff01;		// set the length of the array to 1 for simplicity
		}
	}
	DEBUG("modify_argTypes is returning");
}

int rpcRegister(char* name, int* argTypes, skeleton f) {
	DEBUG("rpcRegister is called!  name:%s    argTypes:%p    skeleton:%p", name, (void*)argTypes, (void*)f);
	// find arglen
	unsigned arglen;
	for (arglen = 0; argTypes[arglen] != 0; arglen++);
	
	// prepare the register request to the binder
	unsigned msg_len = sizeof(location) + (MAX_PROC_NAME_SIZE+1)*sizeof(char) + (1+arglen+1)*sizeof(int);
	char *msg = malloc(sizeof(unsigned) + sizeof(int) + msg_len);
	if (msg == NULL) return ERR_RPC_OUT_OF_MEMORY;
	
	memset (msg, 0, sizeof(unsigned) + sizeof(int) + msg_len);
	char *msg_offset = msg;
	*((unsigned *) msg_offset) = msg_len;
	
	msg_offset += sizeof(unsigned);
	*((int *) msg_offset) = REGISTER;
	
	msg_offset += sizeof(int);
	memcpy(msg_offset, &my_loc, sizeof(location));
	
	msg_offset += sizeof(location);
	strncpy(msg_offset, name, MAX_PROC_NAME_SIZE);
	
	msg_offset += (MAX_PROC_NAME_SIZE+1)*sizeof(char);
	*((unsigned *) msg_offset) = arglen;
	
	msg_offset += sizeof(unsigned);
	memcpy(msg_offset, argTypes, (arglen+1)*sizeof(int));
	
	// modify argTypes such that the array size is always stored as 1
	int *modified_argTypes = (int*)msg_offset;
	//modify_argTypes (arglen, modified_argTypes);
	
	// send msg to the binder and receive the result
	if (send (binder_sock, msg, sizeof(unsigned)+sizeof(int)+msg_len, 0) < 0) {
		DEBUG("ERROR: Sending register msg to the binder failed!");
		free(msg);
		return ERR_RPC_BINDER_NOT_FOUND;
	}
	
	// receive the result from the binder
	int bytesRcvd, rcv_len, msg_type, reason_code;
	bytesRcvd = recv(binder_sock, &rcv_len, 4, 0);
	bytesRcvd = recv(binder_sock, &msg_type, 4, 0);
	bytesRcvd = recv(binder_sock, &reason_code, 4, 0);
	if (bytesRcvd < 0) {
		DEBUG("ERROR: bytes rcvd is negative!");
	}
	if (msg_type == REGISTER_FAILURE){
		free(msg);
		return reason_code;
	}
	if (msg_type != REGISTER_SUCCESS){
		free(msg);
		return ERR_RPC_UNEXPECTED_MSG_TYPE;
	}
	
	// prepare everything needed for registration
	proc_sig sig;
	memset (&sig, 0, sizeof(proc_sig));
	strncpy(sig.proc_name, name, MAX_PROC_NAME_SIZE);
	sig.proc_name[MAX_PROC_NAME_SIZE] = '\0';
	sig.argLen = arglen;
	sig.argTypes = modified_argTypes;
	
	//register procedure in the local database
	DEBUG("Updating the proc_list");
	int ret = update (&proc_list, &sig, f);
	DEBUG("Update returned: %d", ret);
	free(msg);
	return ret;
}

void simulated_send_error_msg (char *msg_response, char *reply_buff, int error) {
	*((unsigned *)reply_buff) = sizeof(int);
	*((int *)(reply_buff+sizeof(unsigned))) = EXECUTE_FAILURE;
	*((int *)(reply_buff+sizeof(unsigned)+sizeof(int))) = error;
	//send the message
	memcpy (msg_response, reply_buff, sizeof(unsigned)+sizeof(int)+sizeof(int));
}

int client_to_server (char *msg_req, char *msg_response) {
	DEBUG("client_to_server is called");
	unsigned msg_len = *((unsigned *)msg_req);
	int msg_type = *((int *)(msg_req+sizeof(unsigned)));
	
	char *reply_buff = malloc(sizeof(unsigned) + sizeof(int) + msg_len);
	if (reply_buff == NULL) return ERR_RPC_OUT_OF_MEMORY;
	
	memset(reply_buff, 0, sizeof(unsigned) + sizeof(int) + msg_len);
	
	if (msg_type != EXECUTE){
		DEBUG("Unknown request type from the client");
		simulated_send_error_msg (msg_response, reply_buff, ERR_RPC_UNEXPECTED_MSG_TYPE);
		free(reply_buff);
		return ERR_RPC_UNEXPECTED_MSG_TYPE;
	}
	
	// unmarshal the message to find skel corresponding to name and argTypes
	char *msg_offset = msg_req+sizeof(unsigned)+sizeof(int);
	proc_sig sig;
	memset (&sig, 0, sizeof(proc_sig));
	strncpy(sig.proc_name, msg_offset, MAX_PROC_NAME_SIZE);
	sig.proc_name[MAX_PROC_NAME_SIZE] = '\0';
	
	msg_offset += (MAX_PROC_NAME_SIZE+1)*sizeof(char);
	sig.argLen = *((unsigned *)msg_offset);
	
	msg_offset += sizeof(unsigned);
	sig.argTypes = (int*)msg_offset;
	
	skel f = find_skel (&proc_list, &sig);
	if (f == NULL) {
		simulated_send_error_msg (msg_response, reply_buff, ERR_RPC_UNREGISTERED_PROC);
		free(reply_buff);
		return ERR_RPC_UNREGISTERED_PROC;
	}
	void **args = (void**)malloc(sig.argLen*(sizeof(void*)));
	if (args == NULL) {
		simulated_send_error_msg (msg_response, reply_buff, ERR_RPC_OUT_OF_MEMORY);
		free(reply_buff);
		return ERR_RPC_OUT_OF_MEMORY;
	}
	void **args_origin = (void**)malloc(sig.argLen*(sizeof(void*)));
	if (args_origin == NULL) {
		simulated_send_error_msg (msg_response, reply_buff, ERR_RPC_OUT_OF_MEMORY);
		free(reply_buff);
		free(args);
		return ERR_RPC_OUT_OF_MEMORY;
	}
	
	msg_offset += (sig.argLen+1)*sizeof(int);
	int response_len = 0, ret = unmarshall_args(sig.argLen, sig.argTypes, msg_offset, args);
	if (ret == ERR_RPC_OUT_OF_MEMORY){
		simulated_send_error_msg (msg_response, reply_buff, ERR_RPC_OUT_OF_MEMORY);
		free(reply_buff);
		free(args);
		free(args_origin);
		return ERR_RPC_OUT_OF_MEMORY;
	}
	// copy original args before envoking the procedure to avoid memory leaks
	memcpy(args_origin, args, sig.argLen*(sizeof(void*)));
	
	DEBUG("Invoking the proc!  name:%s    skeleton:%p", sig.proc_name, (void*)f);
	ret = f(sig.argTypes, args);
	DEBUG ("Result of function call:%d    args[0]=%d", ret, *(int*)args[0]);
	
	if (ret) {	// function returned error
		simulated_send_error_msg (msg_response, reply_buff, ERR_RPC_PROC_EXEC_FAILED);
		free(reply_buff);
		destroy_args (sig.argLen, args, args_origin);
		return ERR_RPC_PROC_EXEC_FAILED;
	}
	response_len = sizeof(unsigned)+sizeof(int)+msg_len;
	memcpy (reply_buff, msg_req, response_len);
	*((int *)(reply_buff+sizeof(unsigned))) = EXECUTE_SUCCESS;
	msg_offset = reply_buff+sizeof(unsigned)+sizeof(int)+(MAX_PROC_NAME_SIZE+1)*sizeof(char)
				+sizeof(unsigned)+(sig.argLen+1)*sizeof(int);
	// copy args
	marshall_args (sig.argLen, sig.argTypes, msg_offset, args);
	
	//send the success message
	memcpy (msg_response, reply_buff, response_len);
	/*
	*/
	// free dynamic memory allocated
	free(reply_buff);
	destroy_args (sig.argLen, args, args_origin);
	DEBUG("client_to_server is returning succesfully");
	return ERR_RPC_SUCCESS;
}

int locate_server(char *msg, unsigned total_len, location *server_loc){
	
	if (send (binder_sock, msg, total_len, 0) < 0) {
		DEBUG("ERROR: Sending register msg to the binder failed!");
		return ERR_RPC_BINDER_NOT_FOUND;
	}
	
	unsigned msg_len = 0;
	int bytesRcvd = 0, msg_type = 0, ret_code = ERR_RPC_SUCCESS;
	
	bytesRcvd = recv(binder_sock, &msg_len, 4, 0);
	if (bytesRcvd < 0) {
		DEBUG("ERROR: bytes rcvd is negative!");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	bytesRcvd = recv(binder_sock, &msg_type, 4, 0);
	if (bytesRcvd < 0) {
		DEBUG("ERROR: bytes rcvd is negative!");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	char *rcv_buff = malloc(msg_len*sizeof(char));
	if (rcv_buff == NULL) return ERR_RPC_OUT_OF_MEMORY;
	
	bytesRcvd = recv(binder_sock, &rcv_buff, msg_len, 0);
	if (bytesRcvd < 0) {
		DEBUG("ERROR: bytes rcvd is negative!");
		free(rcv_buff);
		return ERR_RPC_SOCKET_FAILED;
	}
	
	if (msg_type == LOC_FAILURE) {
		ret_code = *((int *)(rcv_buff));
	}
	else if (msg_type != LOC_SUCCESS) {
		ret_code = ERR_RPC_UNEXPECTED_MSG_TYPE;
	}
	else {
		memcpy(server_loc, rcv_buff, sizeof(location));
	}
	
	free(rcv_buff);
	return ret_code;
}

int create_server_connection (int *server_sock, location *server_loc){
	// create a socket to connect to the binder
	struct sockaddr_in server;
	*server_sock = socket (AF_INET, SOCK_STREAM, 0);
	if (*server_sock == -1) {
		DEBUG("ERROR: Client could not create a server_socket!!! Exiting...\n");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	struct hostent *he = gethostbyname(server_loc->s_id.addr.hostname);
	memcpy (&server.sin_addr, he->h_addr_list[0], he->h_length);
	server.sin_family = AF_INET;
	server.sin_port = server_loc->s_port;
	
	if (connect (*server_sock, (struct sockaddr *)&server, sizeof(server)) < 0) {
		DEBUG("ERROR: Could not connect to the server! Exiting...\n");
		return ERR_RPC_SOCKET_FAILED;
	}
	
	return ERR_RPC_SUCCESS;
}

int send_request (int *server_sock, char *msg, unsigned total_msg_len){
	int ret = send (*server_sock, msg, total_msg_len, 0);
	if (ret < 0) return ERR_RPC_SOCKET_FAILED;
	return ERR_RPC_SUCCESS;
}

int rcv_response (int *server_sock, char *msg_response, unsigned total_msg_len){
	int bytesRcvd = recv(*server_sock, &msg_response, total_msg_len, 0);
	if (bytesRcvd < 0) return ERR_RPC_SOCKET_FAILED;
	return ERR_RPC_SUCCESS;
}

int handle_server_response (void** args, char *msg_response){
	DEBUG("handle_server_response is called");
	DEBUG("msg_response memory- start:%p", msg_response);
	// unsigned msg_len = *((unsigned *)msg_response);
	int msg_type = *((int *)(msg_response+sizeof(unsigned)));
	if (msg_type == EXECUTE_FAILURE) {
		DEBUG("handle_server_response returns failure");
		return *((int *)(msg_response+sizeof(unsigned)+sizeof(int)));
	}
	else if (msg_type != EXECUTE_SUCCESS){
		DEBUG("handle_server_response returns unexpected response type");
		return ERR_RPC_UNEXPECTED_MSG_TYPE;		// unexpected response type
	}
	char *msg_offset = msg_response+sizeof(unsigned)+sizeof(int)+(MAX_PROC_NAME_SIZE+1)*sizeof(char);
	unsigned arglen = *((unsigned *)(msg_offset));
	DEBUG("unmarshall_args_no_alloc args_buff=%p", (msg_offset+sizeof(unsigned)+(arglen+1)*sizeof(int)));
	unmarshall_args_no_alloc   (arglen, (int *)(msg_offset+sizeof(unsigned)),
								msg_offset+sizeof(unsigned)+(arglen+1)*sizeof(int), args);
	
	DEBUG("handle_server_response returned successfully");
	return ERR_RPC_SUCCESS;
}

int rpcCall(char* name, int* argTypes, void** args) {
	// find arglen
	unsigned arglen;
	for (arglen = 0; argTypes[arglen] != 0; arglen++);
	
	// create an array of size arglen to indicate each arg size
	unsigned *arg_sizes = malloc(arglen*sizeof(unsigned));
	if (arg_sizes == NULL) return ERR_RPC_OUT_OF_MEMORY;
	memset (arg_sizes, 0, arglen*sizeof(unsigned));
	
	// analyze argTypes to find arg sizes
	unsigned total_args_size = find_args_size (arglen, argTypes, arg_sizes);
	
	// prepare the location request
	unsigned loc_req_len = (MAX_PROC_NAME_SIZE+1)*sizeof(char) + (1+arglen+1)*sizeof(int);
	
	// allocate additional space for args for later use
	unsigned total_msg_len = sizeof(unsigned) + sizeof(int) + loc_req_len + total_args_size;
	char *msg = malloc(total_msg_len);
	if (msg == NULL) {
		free(arg_sizes);
		return ERR_RPC_OUT_OF_MEMORY;
	}
	memset (msg, 0, total_msg_len);
	
	char *msg_offset = msg;
	*((unsigned *) msg_offset) = loc_req_len;
	
	msg_offset += sizeof(unsigned);
	*((int *) msg_offset) = LOC_REQUEST;
	
	msg_offset += sizeof(int);
	strncpy(msg_offset, name, MAX_PROC_NAME_SIZE);
	
	msg_offset += (MAX_PROC_NAME_SIZE+1)*sizeof(char);
	*((unsigned *) msg_offset) = arglen;
	
	msg_offset += sizeof(unsigned);
	memcpy(msg_offset, argTypes, (arglen+1)*sizeof(int));
	
	// send the location request to the binder to locate a server for procedure call
	int ret;
	ret = create_binder_sock();
	if (ret != ERR_RPC_SUCCESS) {
		free(arg_sizes);
		free(msg);
		return ret;
	}
	location server_loc;
	memset(&server_loc, 0, sizeof(location));
	ret = locate_server(msg, sizeof(unsigned) + sizeof(int) + loc_req_len, &server_loc);
	close(binder_sock);
	
	if (ret != ERR_RPC_SUCCESS) {
		free(arg_sizes);
		free(msg);
		return ret;
	}
	// SUCCESSFULLY located the server. Prepare the execution request
	msg_offset = msg;
	*((unsigned *) msg_offset) = loc_req_len + total_args_size;
	
	msg_offset += sizeof(unsigned);
	*((int *) msg_offset) = EXECUTE;
	
	msg_offset += sizeof(int) + (MAX_PROC_NAME_SIZE+1)*sizeof(char) + sizeof(unsigned) + (arglen+1)*sizeof(int);
	for (unsigned i = 0; i < arglen; i++) {
		memcpy(msg_offset, args[i], arg_sizes[i]);
		msg_offset += arg_sizes[i];
	}
	
	char *msg_response = malloc(total_msg_len);
	if (msg_response == NULL) {
		free(arg_sizes);
		free(msg);
		return ERR_RPC_OUT_OF_MEMORY;
	}
	memset (msg_response, 0, total_msg_len);
	
	DEBUG("msg_response memory- start:%p  end:%p  size:%d", msg_response, msg_response+total_msg_len, total_msg_len);
	
	// send the request to the server
	int server_sock = 0;
	ret = create_server_connection (&server_sock, &server_loc);
	if (ret != ERR_RPC_SUCCESS) {
		free(arg_sizes);
		free(msg);
		free(msg_response);
		return ret;
	}
	
	ret = send_request (&server_sock, msg, total_msg_len);
	if (ret != ERR_RPC_SUCCESS) {
		free(arg_sizes);
		free(msg);
		free(msg_response);
		return ret;
	}
	
	ret = rcv_response (&server_sock, msg_response, total_msg_len);
	if (ret != ERR_RPC_SUCCESS) {
		free(arg_sizes);
		free(msg);
		free(msg_response);
		return ret;
	}
	
	/*client_to_server (msg, msg_response);		// simulation*/
	
	// handle the response received
	ret = handle_server_response (args, msg_response);
	
	free(arg_sizes);
	free(msg);
	free(msg_response);
	
	return ret;
}
