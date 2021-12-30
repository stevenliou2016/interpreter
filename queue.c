#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#include "harness.h"
#include "queue.h"

#ifndef strlcpy
#define strlcpy(dst, src, sz) snprintf((dst), (sz), "%s", (src))
#endif
/*
 * Create empty queue.
 * Return NULL if could not allocate space.
 */
queue_t *q_new()
{
    queue_t *q = malloc(sizeof(queue_t));
    /* TODO: What if malloc returned NULL? */
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
    /* TODO: How about freeing the list elements and the strings? */
    if (q != NULL) {
        list_ele_t *head = q->head;
        while (head) {
            list_ele_t *temp = head;
            head = head->next;
            if (temp->value != NULL) {
                free(temp->value);
            }
            free(temp);
        }
        /* Free queue structure */
        free(q);
    }
}

/*
 * Attempt to insert element at head of queue.
 * Return true if successful.
 * Return false if q is NULL or could not allocate space.
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_head(queue_t *q, char *s)
{
    list_ele_t *newh;
    /* TODO: What should you do if the q is NULL? */
    if (q == NULL) {
        return false;
    }
    newh = malloc(sizeof(list_ele_t));
    if (newh == NULL) {
        return false;
    }
    /* Don't forget to allocate space for the string and copy it */
    int length = strlen(s);
    newh->value = malloc(length + 1);
    if (newh->value == NULL) {
        free(newh);
        return false;
    }
    /* copy string  */
    strlcpy(newh->value, s, length + 1);
    newh->next = NULL;
    /* What if either call to malloc returns NULL? */
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

/*
 * Attempt to insert element at tail of queue.
 * Return true if successful.
 * Return false if q is NULL or could not allocate space.
 * Argument s points to the string to be stored.
 * The function must explicitly allocate space and copy the string into it.
 */
bool q_insert_tail(queue_t *q, char *s)
{
    /* TODO: You need to write the complete code for this function */
    if (q == NULL) {
        return false;
    }
    list_ele_t *newt = malloc(sizeof(list_ele_t));
    if (newt == NULL) {
        return false;
    }
    int length = strlen(s);
    newt->value = malloc(length + 1);
    if (newt->value == NULL) {
        free(newt);
        return false;
    }
    /* copy string */
    strlcpy(newt->value, s, length + 1);
    newt->next = NULL;
    /* Remember: It should operate in O(1) time */
    /* TODO: Remove the above comment when you are about to implement. */
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

/*
 * Attempt to remove element from head of queue.
 * Return true if successful.
 * Return false if queue is NULL or empty.
 * If sp is non-NULL and an element is removed, copy the removed string to *sp
 * (up to a maximum of bufsize-1 characters, plus a null terminator.)
 * The space used by the list element and the string should be freed.
 */
bool q_remove_head(queue_t *q, char *sp, size_t bufsize)
{
    /* TODO: You need to fix up this code. */
    /* TODO: Remove the above comment when you are about to implement. */
    if (q == NULL || q->head == NULL) {
        return false;
    }
    if (sp != NULL) {
        strlcpy(sp, q->head->value, bufsize);
    }
    list_ele_t *temp = q->head;
    q->head = q->head->next;
    free(temp->value);
    free(temp);
    q->size--;
    return true;
}

/*
 * Return number of elements in queue.
 * Return 0 if q is NULL or empty
 */
int q_size(queue_t *q)
{
    if (q == NULL || q->head == NULL) {
        return 0;
    }
    return q->size;
}

/*
 * Reverse elements in queue
 * No effect if q is NULL or empty
 * This function should not allocate or free any list elements
 * (e.g., by calling q_insert_head, q_insert_tail, or q_remove_head).
 * It should rearrange the existing ones.
 */
void q_reverse(queue_t *q)
{
    /* TODO: You need to write the code for this function */
    /* TODO: Remove the above comment when you are about to implement. */
    if (q != NULL && q->head != NULL) {
        list_ele_t *prev = q->head;
        list_ele_t *curr = q->head->next;
        list_ele_t *next;

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

/*
 * Sort elements of queue in ascending order
 * No effect if q is NULL or empty. In addition, if q has only one
 * element, do nothing.
 */

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
    if (!start || !start->next)
        return start;

    list_ele_t *slow = start;

    for (list_ele_t *fast = start->next; fast && fast->next;
         fast = fast->next->next)
        slow = slow->next;

    list_ele_t *mid = slow->next;
    slow->next = NULL;

    return merge(mergeSort(start), mergeSort(mid));
}

void q_sort(queue_t *q)
{
    /* TODO: You need to write the code for this function */
    /* TODO: Remove the above comment when you are about to implement. */
    if (q != NULL && q->head != NULL) {
        q->head = mergeSort(q->head);
        list_ele_t *temp = q->head;
        while (temp->next) {
            temp = temp->next;
        }
        q->tail = temp;
    }
}
