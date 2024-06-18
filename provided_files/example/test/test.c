#include <criterion/criterion.h>
#include <stdio.h>
#include <sys/mman.h>
#include <string.h>
#include "my_secmalloc.private.h"
#include "my_secmalloc.h"

// Définir les constantes manquantes
#define PAGE_SIZE 4096

Test(my_malloc, memory_allocation)
{
    size_t size = 200;
    void *ptr = my_malloc(size);
    cr_assert(ptr != NULL);
    printf("pointeur : %p\n", ptr);
    printf("Taille restante dans le datablock : %lu\n", 4096 - size);
}

Test(my_free, memory_deallocation)
{
    void *ptr1 = my_malloc(500);
    my_free(ptr1);
    void *ptr2 = my_malloc(500);

    cr_expect(ptr1 == ptr2, "my_free should free the memory block for reuse");
}

Test(my_calloc, memory_allocation)
{
    size_t nmemb = 10;
    size_t size = sizeof(int);
    int *ptr = (int *)my_calloc(nmemb, size);
    cr_assert(ptr != NULL, "Failed to calloc.");
    for (size_t i = 0; i < nmemb; i++)
        cr_assert(ptr[i] == 0, "Failed to calloc at position %lu\n", i);
}
