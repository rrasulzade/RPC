#ifndef		__INT_QUEUE_H__
#define		__INT_QUEUE_H__


#define		ERR_QUEUE_ERROR				-1
#define		ERR_QUEUE_SUCCESS			0


struct node {
	int data;
	struct node *next;
};

typedef struct {
	int size;
	struct node *head;
	struct node *tail;
} intQueue;




void queue_init (intQueue *Q);

int queue_empty (intQueue *Q);

int queue_push (intQueue *Q, int data);

int queue_front (intQueue *Q, int *data);

int queue_pop (intQueue *Q, int *data);

int queue_reset (intQueue *Q);






#endif
