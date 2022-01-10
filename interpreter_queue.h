#ifndef INTERPRETER_QUEUE_H_
#define INTERPRETER_QUEUE_H_

#include <stdbool.h>
#include <sys/types.h>

typedef struct ListElement {
  char *value;
  struct ListElement *next;
} ListElement;

typedef struct Queue{
  ListElement *head;
  ListElement *tail;
  int size;
} Queue;

/* Creates a queue
 * On success, return a pointer to a queue
 * On error, return NULL */
Queue *QueueNew();

/* Deletes elements of queue
 * No effect if queue is NULL */
void QueueFree(Queue *queue);

/* Inserts a string s at head of queue
 * Return false if queue is NULL or memory allocation failed */
bool QueueInsertHead(Queue *queue, char *s);

/* Inserts a string s at tail of queue
 * Return false if queue is NULL or memory allocation failed */
bool QueueInsertTail(Queue *queue, char *s);

/* Removes an element from queue
 * Return false if queue is NULL or empty */
bool QueueRemoveHead(Queue *queue);

/* Return number of elements in queue
 * Return 0 if queue is NULL or empty */
int QueueSize(Queue *queue);

/* Reverse queue 
 * No effect if queue is NULL or empty*/
void QueueReverse(Queue *queue);

/* Sort elements of queue in ascending order
 * No effect if queue is NULL or empty */
void QueueSort(Queue *queue);
#endif 
