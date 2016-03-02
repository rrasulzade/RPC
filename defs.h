#ifndef		__DEFS_H__
#define		__DEFS_H__


#include <netinet/in.h>


#define		MAX_PROC_NAME_SIZE		64
#define		MAX_HOSTNAME_SIZE		256

#define		ADDR_TYPE_IP			0
#define		ADDR_TYPE_HOSTNAME		1



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


typedef union {
	struct in_addr sin_addr;
	char hostname[MAX_HOSTNAME_SIZE+1];
} IP_or_HOST;


typedef struct {
	int addr_type;		// can be	ADDR_TYPE_IP or ADDR_TYPE_HOSTNAME
	IP_or_HOST addr;
} server_identifier;


// FOR Binder
typedef struct {
	char proc_name [MAX_PROC_NAME_SIZE+1];	// NULL terminated
	int *argTypes;							// NULL terminated
} proc_sig;


typedef struct {
	server_identifier s_id;
	unsigned short s_port;
} location;


#endif
