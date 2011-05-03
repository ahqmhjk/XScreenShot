#ifndef _GG_LIST_H
#define _GG_LIST_H

typedef struct _GList {
	void *data;
	struct _GList *next;
}GList;

typedef void (*FUNC)(void *data, void *usr_data);

GList *g_list_append(GList *list, void *data);

unsigned int g_list_length(GList *list);

void g_list_foreach(GList *list, FUNC func, void *usr_data);

void g_list_free(GList *list);

#endif
