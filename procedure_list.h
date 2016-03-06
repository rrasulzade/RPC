#ifndef		__PROC_LIST_H__
#define		__PROC_LIST_H__



#define		ERR_LIST_INVALID_ARG	-2
#define		ERR_LIST_OUT_OF_MEMORY	-1
#define		ERR_LIST_SUCCESS		0
#define		ERR_LIST_UPDATED		1


#include "defs.h"


typedef int (*skel)(int *, void **);

struct proc_node {
	proc_sig sig;
	skel f;
	struct proc_node *next;
};



int insert (struct proc_node **head, proc_sig *sig, skel f);

int update (struct proc_node **head, proc_sig *sig, skel f);

skel find_skel (struct proc_node **head, proc_sig *sig);

int delete_all (struct proc_node **head);





#endif
