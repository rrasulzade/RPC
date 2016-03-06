/*
	Queue implementation for sockets
*/


#include <stdlib.h>
#include "debug.h"
#include "int_queue.h"



void queue_init (intQueue *Q){
	Q->size = 0;
	Q->head = Q->tail = NULL;
}

int queue_empty (intQueue *Q){
	return Q->size == 0;
}

int queue_push (intQueue *Q, int data){
	struct node *newnode = malloc(sizeof(struct node));
	if (newnode == NULL) {
		return ERR_QUEUE_ERROR;
	}
	
	newnode->data = data;
	newnode->next = NULL;
	
	if (Q->size == 0) {
		if (Q->head != NULL || Q->tail != NULL){
			DEBUG("WARNING!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!! (head != NULL || tail != NULL),  BUT size = 0");
		}	
		Q->head = Q->tail = newnode;
	}
	else {
		Q->tail->next = newnode;
		Q->tail = newnode;
	}
	
	Q->size++;
	return ERR_QUEUE_SUCCESS;
}

int queue_front (intQueue *Q, int *data){
	if (Q->size == 0) {
		return ERR_QUEUE_ERROR;
	}
	
	*data = Q->head->data;
	return ERR_QUEUE_SUCCESS;
}

int queue_pop (intQueue *Q){
	struct node *del_node = Q->head;
	
	if (Q->size == 0) {
		return ERR_QUEUE_ERROR;
	}
	else if (Q->size == 1) {
		Q->head = Q->tail = NULL;
	}
	else {
		Q->head = Q->head->next;
	}
	
	free(del_node);
	Q->size--;
	return ERR_QUEUE_SUCCESS;
}

int queue_reset (intQueue *Q){
	struct node *cur = Q->head, *node_del;
	while (cur != NULL){
		node_del = cur;
		cur = cur->next;
		free(node_del);
	}
	queue_init (Q);
	return 0;
}
