#include <stdbool.h>
#include <stdio.h>
#include "messages.h"

bool mem_alloc_succ(void *p)
{
    if (!p) {
	show_message("memory allocation failed\n");
        return false;
    }
    return true;
}