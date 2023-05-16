#include "list_tools.h"

#include <stddef.h>
#include <stdlib.h>
#include <string.h>

struct list *list_append(struct list *list, const void *value, size_t data_size)
{
    struct list *node = calloc(1, sizeof(struct list));
    if (node == NULL)
        return NULL;

    if (list == NULL)
        list = node;
    else
    {
        struct list *cpy = list;
        while (cpy->next != NULL)
            cpy = cpy->next;
        cpy->next = node;
    }

    node->next = NULL;
    node->data = malloc(data_size);

    if (node->data == NULL)
        return NULL;

    memcpy(node->data, value, data_size);

    return list;
}

struct list *list_prepend(struct list *list, const void *value,
                          size_t data_size)
{
    if (value == NULL)
        return NULL;

    struct list *node = calloc(1, sizeof(struct list));
    if (node == NULL)
        return NULL;

    node->next = list;

    node->data = malloc(data_size);
    if (node->data == NULL)
        return NULL;

    memcpy(node->data, value, data_size);

    return node;
}

size_t list_length(struct list *list)
{
    size_t len = 0;
    struct list *cpy = list;

    while (cpy != NULL)
    {
        len++;
        cpy = cpy->next;
    }
    return len;
}

void list_destroy(struct list *list)
{
    struct list *prev;
    struct list *curr = list;

    while (curr != NULL)
    {
        prev = curr;
        curr = curr->next;
        free(prev->data);
        free(prev);
    }
}
