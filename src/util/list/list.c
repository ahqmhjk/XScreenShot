#include "list.h"
#include <stdlib.h>
static inline GList *_g_list_alloc()
{
    GList *list;
   
//     LOCK (current_allocator);//multi-thread
	list = (GList*)malloc(sizeof(GList));
    list->data = NULL;
//     UNLOCK (current_allocator);
     
    list->next = NULL;
   
   	return list;
}

static inline GList *g_list_last (GList *list) {

	if (list) {
    	while (list->next)
        	list = list->next;
    }
   	
	return list;
}


GList *g_list_append(GList *list, void *data) {
	GList *new_list;
	GList *last;
	new_list = _g_list_alloc ();
	new_list->data = data;
	if (list) {
		last = g_list_last (list);
		/* g_assert (last != NULL); */
		last->next = new_list;
		return list;
	}
	else
		return new_list;
}

unsigned int g_list_length(GList *list)
{
	unsigned int length;
   
    length = 0;
    while (list)
    {
    	length++;
        list = list->next;
    }
   
    return length;
}

void g_list_free(GList *list)
{
	if (list)
    {
    	GList *last_node = list->next;
        while (last_node)
        {
        	if (list->data) free(list->data);
			free(list);
            list = last_node;
			last_node = last_node->next;
        }
      	if (list->data) free(list->data);
		free(list);
    }
}

void g_list_foreach (GList *list, FUNC func,void *user_data)
{
	while (list)
    {
    	GList *next = list->next;
        (*func) (list->data, user_data);
        list = next;
    }
}
