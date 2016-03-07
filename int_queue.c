/*
	Queue implementation for sockets
*/


#include <stdlib.h>
#include <pthread.h>
#include "debug.h"
#include "int_queue.h"


/*
pthread_mutex_lock(&Qmutex);
pthread_cond_wait (&CV, &Qmutex);
*/


// mutex for inserting/removing data from/into queue
pthread_mutex_t Qmutex = PTHREAD_MUTEX_INITIALIZER;

// For syncronization when the queue is empty
pthread_cond_t CV		= PTHREAD_COND_INITIALIZER;


void queue_init (intQueue *Q){
	pthread_mutex_lock(&Qmutex);
	Q->size = 0;
	Q->head = Q->tail = NULL;
	pthread_mutex_unlock(&Qmutex);
}

int queue_empty (intQueue *Q){
	pthread_mutex_lock(&Qmutex);
	int ret = (Q->size == 0);
	pthread_mutex_unlock(&Qmutex);
	return ret;
}

int queue_push (intQueue *Q, int data){
	pthread_mutex_lock(&Qmutex);
	struct node *newnode = malloc(sizeof(struct node));
	if (newnode == NULL) {
		pthread_mutex_unlock(&Qmutex);
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
	pthread_cond_signal (&CV);
	pthread_mutex_unlock(&Qmutex);
	return ERR_QUEUE_SUCCESS;
}

int queue_front (intQueue *Q, int *data){
	pthread_mutex_lock(&Qmutex);
	if (Q->size == 0) {
		pthread_mutex_unlock(&Qmutex);
		return ERR_QUEUE_ERROR;
	}
	
	*data = Q->head->data;
	pthread_mutex_unlock(&Qmutex);
	return ERR_QUEUE_SUCCESS;
}

int queue_pop (intQueue *Q, int *data){
	pthread_mutex_lock(&Qmutex);
	struct node *del_node = Q->head;
	
	if (Q->size == 0) {
		pthread_cond_wait (&CV, &Qmutex);
		DEBUG("queue_pop():  worker is woken up!");
		if (Q->size == 0) return ERR_QUEUE_ERROR;
	}
	else if (Q->size == 1) {
		Q->head = Q->tail = NULL;
	}
	else {
		Q->head = Q->head->next;
	}
	
	*data = del_node->data;
	free(del_node);
	Q->size--;
	pthread_mutex_unlock(&Qmutex);
	return ERR_QUEUE_SUCCESS;
}

int queue_reset (intQueue *Q){
	pthread_mutex_lock(&Qmutex);
	struct node *cur = Q->head, *node_del;
	while (cur != NULL){
		node_del = cur;
		cur = cur->next;
		free(node_del);
	}
	queue_init (Q);
	pthread_mutex_unlock(&Qmutex);
	return 0;
}
