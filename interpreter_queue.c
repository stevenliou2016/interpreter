#include "interpreter_queue.h"
#include <stdlib.h>
#include <string.h>

Queue *QueueNew() {
  Queue *queue = malloc(sizeof(Queue));

  if (queue == NULL) {
    return NULL;
  }
  memset(queue, 0, sizeof(Queue));
  /*queue->head = NULL;
  queue->tail = NULL;
  queue->size = 0;*/
  return queue;
}

void QueueFree(Queue *queue) {
  ListElement *head = NULL;
  ListElement *next = NULL;

  if (queue) {
    head = queue->head;
    next = head;
    while (next) {
      head = next;
      if (next->value) {
        free(next->value);
      }
      next = next->next;
      free(head);
    }
    free(queue);
  }
}

bool QueueInsertHead(Queue *queue, char *str) {
  int len = 0;
  ListElement *new_head = NULL;

  if (queue == NULL || str == NULL) {
    return false;
  }
  len = strlen(str);
  new_head = malloc(sizeof(ListElement));
  if (new_head == NULL) {
    return false;
  }
  new_head->value = malloc((len + 1) * sizeof(char));
  if (new_head->value == NULL) {
    free(new_head);
    return false;
  }
  memset(new_head->value, 0, (len + 1) * sizeof(char));
  strncpy(new_head->value, str, len);
  new_head->value[len] = '\0';
  new_head->next = NULL;
  if (queue->head) {
    new_head->next = queue->head;
    if (queue->tail == NULL) {
      queue->tail = queue->head;
    }
    queue->head = new_head;
  } else {
    queue->head = new_head;
    if (queue->tail == NULL) {
      queue->tail = queue->head;
    }
  }
  queue->size++;
  return true;
}

bool QueueInsertTail(Queue *queue, char *str) {
  int length = 0;
  ListElement *new_tail = NULL;

  if (queue == NULL || str == NULL) {
    return false;
  }
  length = strlen(str);
  new_tail = malloc(sizeof(ListElement));
  if (new_tail == NULL) {
    return false;
  }
  new_tail->value = malloc(length + 1);
  if (new_tail->value == NULL) {
    free(new_tail);
    return false;
  }
  strncpy(new_tail->value, str, length);
  new_tail->value[length] = '\0';
  new_tail->next = NULL;
  if (queue->tail == NULL) {
    queue->tail = new_tail;
    if (queue->head == NULL) {
      queue->head = queue->tail;
    }
  } else {
    if (queue->head == NULL) {
      queue->head = queue->tail;
    }
    queue->tail->next = new_tail;
    queue->tail = new_tail;
  }
  queue->size++;
  return true;
}

bool QueueRemoveHead(Queue *queue) {
  ListElement *remove_element = NULL;

  if (queue == NULL || queue->head == NULL || queue->size == 0) {
    return false;
  }
  remove_element = queue->head;
  queue->head = queue->head->next;
  free(remove_element->value);
  free(remove_element);
  queue->size--;
  if(queue->size == 0){
    queue->tail = NULL;
  }
  return true;
}

int QueueSize(Queue *queue) {
  if (queue == NULL || queue->head == NULL) {
    return 0;
  }
  return queue->size;
}

void QueueReverse(Queue *queue) {
  ListElement *previous_element = NULL;
  ListElement *current_element = NULL;
  ListElement *next_element = NULL;

  if (queue != NULL && queue->head != NULL) {
    previous_element = queue->head;
    current_element = queue->head->next;
    queue->head = queue->tail;
    queue->tail = previous_element;
    previous_element->next = NULL;
    while (current_element) {
      next_element = current_element->next;
      current_element->next = previous_element;
      previous_element = current_element;
      current_element = next_element;
    }
  }
}


ListElement *Merge(ListElement *left, ListElement *right) {
  ListElement *head = NULL;

  for (ListElement **iter = &head; true; iter = &((*iter)->next)) {
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

ListElement *MergeSort(ListElement *start) {
  ListElement *slow = start;
  ListElement *mid = NULL;

  if (!start || !start->next) {
    return start;
  }
  for (ListElement *fast = start->next; fast && fast->next;
       fast = fast->next->next) {
    slow = slow->next;
  }
  mid = slow->next;
  slow->next = NULL;

  return Merge(MergeSort(start), MergeSort(mid));
}

void QueueSort(Queue *queue) {
  ListElement *tail = NULL;

  if (queue != NULL && queue->head != NULL) {
    queue->head = MergeSort(queue->head);
    tail = queue->head;
    while (tail->next) {
      tail = tail->next;
    }
    queue->tail = tail;
  }
}
