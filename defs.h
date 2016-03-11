#ifndef		__DEFS_H__
#define		__DEFS_H__


#include <netinet/in.h>


#define		ERR_RPC_PROC_RE_REG				1
#define		ERR_RPC_SUCCESS					0
#define		ERR_RPC_OUT_OF_MEMORY			-1
#define		ERR_RPC_PROC_EXEC_FAILED		-2
#define		ERR_RPC_EXEC_BEFORE_REG			-3
#define		ERR_RPC_UNREGISTERED_PROC		-4
#define		ERR_RPC_NO_SERVER_AVAIL			-5
#define		ERR_RPC_UNEXPECTED_MSG_TYPE		-6
// #define		ERR_RPC_UNKNOWN_TERMINATION_SRC	-8
#define		ERR_RPC_SOCKS_INACTIVE			-9			// both listening and binder sock inactive in server
#define		ERR_RPC_SOCKET_FAILED			-10
#define		ERR_RPC_ENV_ADDR_NULL			-11
#define		ERR_RPC_ENV_PORT_NULL			-12
#define		ERR_RPC_HOSTENT_NULL			-13
#define		ERR_RPC_THREAD_NOT_CREATED		-14


#define		ERR_BINDER_OUT_OF_MEMORY		-15
#define		ERR_RPC_BINDER_SOCK_CLOSED		-17
#define		ERR_RPC_BINDER_SOCK_FAILED		-18
#define		ERR_RPC_SERVER_SOCK_FAILED		-19

#define 	ERR_BINDER_TERMINATE_SIG		-50

#define		ERR_RPC_INCOMPLETE_MSG			-99
// #define		ERR_RPC_ARR_LEN_NOT_ENOUGH		-100



typedef enum {
	REGISTER,
	REGISTER_SUCCESS,
	REGISTER_FAILURE,
	LOC_REQUEST_CACHE,
	LOC_REQUEST,
	LOC_SUCCESS,
	LOC_FAILURE,
	EXECUTE,
	EXECUTE_SUCCESS,
	EXECUTE_FAILURE,
	TERMINATE,
	UNKNOWN
} msg_type;





#define		MAX_PROC_NAME_SIZE		64
#define		MAX_HOSTNAME_SIZE		256

#define		ADDR_TYPE_IP			0
#define		ADDR_TYPE_HOSTNAME		1



typedef union {
	struct in_addr sin_addr;
	char hostname[MAX_HOSTNAME_SIZE+1];
} IP_or_HOST;


typedef struct {
	int addr_type;		// can be	ADDR_TYPE_IP or ADDR_TYPE_HOSTNAME
	IP_or_HOST addr;
} server_identifier;


typedef struct {
	server_identifier s_id;
	unsigned short s_port;
} location;


typedef struct {
	char proc_name [MAX_PROC_NAME_SIZE+1];	// NULL terminated
	unsigned int argLen;
	int *argTypes;							// NULL terminated
} proc_sig;



#endif
