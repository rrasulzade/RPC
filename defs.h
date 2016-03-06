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
#define		ERR_RPC_BINDER_NOT_FOUND		-7
#define		ERR_RPC_UNKNOWN_TERMINATION_SRC	-8
#define		ERR_RPC_SOCKET_INACTIVE			-9
#define		ERR_RPC_SOCKET_FAILED			-10

#define		ERR_RPC_INCOMPLETE_MSG			-99
#define		ERR_RPC_ARR_LEN_NOT_ENOUGH		-100


#define		ERR_BINDER_REG_OVERRIDE			1
#define		ERR_BINDER_REG_SUCCESS			0
#define		ERR_BINDER_REG_FAILURE			-1


typedef enum {
	REGISTER,
	REGISTER_SUCCESS,
	REGISTER_FAILURE,
	LOC_REQUEST,
	LOC_SUCCESS,
	LOC_FAILURE,
	EXECUTE,
	EXECUTE_SUCCESS,
	EXECUTE_FAILURE,
	TERMINATE
} msg_type;


typedef enum {
	SUCCESS = 0
} err_code;



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


// FOR Binder
typedef struct {
	char proc_name [MAX_PROC_NAME_SIZE+1];	// NULL terminated
	unsigned int argLen;
	int *argTypes;							// NULL terminated
} proc_sig;



#endif
