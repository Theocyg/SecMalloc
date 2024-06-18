// Cette partie sera "vue par l'extérieure". C'est pour utiliser mon sec_malloc au lieu du malloc de base.

#ifndef _SECMALLOC_H
#define _SECMALLOC_H

#include <stddef.h>

void    *malloc(size_t size);
void    free(void *ptr);
void    *calloc(size_t nmemb, size_t size);
void    *realloc(void *ptr, size_t size);

#endif
