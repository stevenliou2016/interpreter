#include "queue.h"
#include <stdlib.h>
#include <string.h>

/* Create empty queue.
 * On success, return a pointer to the queue
 * On error, return NULL */
queue_t *q_new()
{
    queue_t *q = malloc(sizeof(queue_t));

    if (q == NULL) {
        return NULL;
    }
    q->head = NULL;
    q->tail = NULL;
    q->size = 0;
    return q;
}

/* Free all storage used by queue */
void q_free(queue_t *q)
{
    list_ele_t *head = NULL;
    list_ele_t *temp = NULL;

    if (q != NULL) {
        head = q->head;
        while (head) {
            temp = head;
            head = head->next;
            if (temp->value != NULL) {
                free(temp->value);
            }
            free(temp);
        }
        free(q);
    }
}

/* Insert a string at head of queue
 * On success, return true
 * On error, return false */
bool q_insert_head(queue_t *q, char *s)
{
    int length = strlen(s);
    list_ele_t *newh = NULL;

    if (q == NULL) {
        return false;
    }
    newh = malloc(sizeof(list_ele_t));
    if (newh == NULL) {
        return false;
    }
    newh->value = malloc(length + 1);
    if (newh->value == NULL) {
        free(newh);
        return false;
    }
    strncpy(newh->value, s, length);
    newh->value[length] = '\0';
    newh->next = NULL;
    if (q->head) {
        newh->next = q->head;
        if (q->tail == NULL) {
            q->tail = q->head;
        }
        q->head = newh;
    } else {
        q->head = newh;
        if (q->tail == NULL) {
            q->tail = q->head;
        }
    }
    q->size++;
    return true;
}

/* Insert a string at tail of queue
 * On success, return true
 * On error, return false */
bool q_insert_tail(queue_t *q, char *s)
{
    int length = strlen(s);
    list_ele_t *newt = NULL;

    if (q == NULL) {
        return false;
    }
    newt = malloc(sizeof(list_ele_t));
    if (newt == NULL) {
        return false;
    }
    newt->value = malloc(length + 1);
    if (newt->value == NULL) {
        free(newt);
        return false;
    }
    strncpy(newt->value, s, length);
    newt->value[length] = '\0';
    newt->next = NULL;
    if (q->tail == NULL) {
        q->tail = newt;
        if (q->head == NULL) {
            q->head = q->tail;
        }
    } else {
        if (q->head == NULL) {
            q->head = q->tail;
        }
        q->tail->next = newt;
        q->tail = newt;
    }
    q->size++;
    return true;
}

/* Remove head of queue
 * On success, return true
 * On error, return false */
bool q_remove_head(queue_t *q, char *sp, size_t bufsize)
{
    list_ele_t *temp = q->head;

    if (q == NULL || q->head == NULL) {
        return false;
    }
    if (sp != NULL) {
        strncpy(sp, q->head->value, bufsize - 2);
	sp[bufsize - 1] = '\0';
    }
    q->head = q->head->next;
    free(temp->value);
    free(temp);
    q->size--;
    return true;
}

/* Return number of elements in queue.
 * Return 0 if q is NULL or empty */
int q_size(queue_t *q)
{
    if (q == NULL || q->head == NULL) {
        return 0;
    }
    return q->size;
}

/* reverse the queue */
void q_reverse(queue_t *q)
{
    list_ele_t *prev = NULL;
    list_ele_t *curr = NULL; 
    list_ele_t *next = NULL;

    if (q != NULL && q->head != NULL) {
        prev = q->head;
	curr = q->head->next;
        q->head = q->tail;
        q->tail = prev;
        prev->next = NULL;
        while (curr) {
            next = curr->next;
            curr->next = prev;
            prev = curr;
            curr = next;
        }
    }
}

/* Sort elements of queue in ascending order */

list_ele_t *merge(list_ele_t *left, list_ele_t *right)
{
    list_ele_t *head = NULL;

    for (list_ele_t **iter = &head; true; iter = &((*iter)->next)) {
        if (!left) {
            *iter = right;
            break;
        }
        if (!right) {
            *iter = left;
            break;
        }
        if (strcmp(left->value, right->value) < 0) {
            *iter = left;
            left = left->next;
        } else {
            *iter = right;
            right = right->next;
        }
    }
    return head;
}

list_ele_t *mergeSort(list_ele_t *start)
{
    list_ele_t *slow = start;
    list_ele_t *mid = NULL;

    if (!start || !start->next){
        return start;
    }
    for (list_ele_t *fast = start->next; fast && fast->next;
         fast = fast->next->next){
        slow = slow->next;
    }
    mid = slow->next;
    slow->next = NULL;

    return merge(mergeSort(start), mergeSort(mid));
}

void q_sort(queue_t *q)
{
    list_ele_t *temp = NULL;

    if (q != NULL && q->head != NULL) {
        q->head = mergeSort(q->head);
        temp = q->head;
        while (temp->next) {
            temp = temp->next;
        }
        q->tail = temp;
    }
}
