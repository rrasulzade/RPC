#include <stdlib.h>
#include <string.h>
#include "procedure_list.h"
#include "debug.h"



void proc_sig_copy (proc_sig *dst, proc_sig *src) {
	memcpy (dst->proc_name, src->proc_name, (MAX_PROC_NAME_SIZE+1)*sizeof(char));
	dst->argLen = src->argLen;
	memcpy (dst->argTypes, src->argTypes, (dst->argLen+1)*sizeof(int));
}

int compare_argTypes (int *dst_argTypes, int *src_argTypes, unsigned argLen){
	for (unsigned i = 0; i < argLen; i++){
		if (!(((dst_argTypes[i] & 0xffff0000) == (src_argTypes[i] & 0xffff0000)) &&
			 (((dst_argTypes[i] & 0x0000ffff) == (src_argTypes[i] & 0x0000ffff)) ||
			  (((dst_argTypes[i] & 0x0000ffff) != 0) && ((src_argTypes[i] & 0x0000ffff) != 0)))))
			return 1;
			
	}
	return ERR_LIST_SUCCESS;
}

// returns 0 on success, -1 when fails
int insert (struct proc_node **head, proc_sig *sig, skel f){
	// DEBUG("Insert is called");
	// create a new node
	struct proc_node *newnode = malloc(sizeof(struct proc_node));
	if (newnode == NULL) {
		// DEBUG("Insert is returning with error");
		return ERR_LIST_OUT_OF_MEMORY;
	}
	memset (newnode, 0, sizeof(struct proc_node));
	newnode->sig.argTypes = malloc((sig->argLen+1)*sizeof(int));
	if (newnode->sig.argTypes == NULL){
		free(newnode);
		// DEBUG("Insert is returning with error");
		return ERR_LIST_OUT_OF_MEMORY;
	}
	proc_sig_copy(&(newnode->sig), sig);
	newnode->f = f;
	
	// insert to the front
	newnode->next = *head;
	*head = newnode;
	// DEBUG("Insert is returning with success");
	return ERR_LIST_SUCCESS;
}

//returns skel on success, NULL on error
skel find_skel (struct proc_node **head, proc_sig *sig) {
	// DEBUG("find_skel is called");
	for (struct proc_node *cur = *head; cur != NULL; cur = cur->next) {
		if (!memcmp (cur->sig.proc_name, sig->proc_name, (MAX_PROC_NAME_SIZE+1)*sizeof(char)) &&
			cur->sig.argLen == sig->argLen &&
			!compare_argTypes (cur->sig.argTypes, sig->argTypes, sig->argLen))
			/*!memcmp (cur->sig.argTypes, sig->argTypes, (sig->argLen+1)*sizeof(int)))*/ {
			// DEBUG("find_skel is returning succesfully");
			return cur->f;
		}
	}
	// DEBUG("find_skel is returning NULL");
	return NULL;
}

int update (struct proc_node **head, proc_sig *sig, skel f) {
	// DEBUG("Update is called");
	if (head == NULL || sig == NULL) return ERR_LIST_INVALID_ARG;		// invalid argument
	for (struct proc_node *cur = *head; cur != NULL; cur = cur->next) {
		if (!memcmp (cur->sig.proc_name, sig->proc_name, (MAX_PROC_NAME_SIZE+1)*sizeof(char)) &&
			cur->sig.argLen == sig->argLen &&
			!compare_argTypes (cur->sig.argTypes, sig->argTypes, sig->argLen))
			/*!memcmp (cur->sig.argTypes, sig->argTypes, (sig->argLen+1)*sizeof(int)))*/ {
			cur->f = f;
			// DEBUG("Update is returning with updating old proc_sig");
			return ERR_LIST_UPDATED;
		}
	}
	// DEBUG("Update is returning by calling insert to insert new value");
	return insert (head, sig, f);
}

int delete_all (struct proc_node **head) {
	DEBUG("delete_all() is called!");

	struct proc_node *cur = *head;
	while (cur != NULL) {
		free(cur->sig.argTypes);
		*head = cur->next;
		free(cur);
		cur = *head;
	}
	*head = NULL;
	
	DEBUG("delete_all() is returning...");
	return 0;
}
