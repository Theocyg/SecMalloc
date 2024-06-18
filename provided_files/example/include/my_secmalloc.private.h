// Interface privée utilisée pour cacher a l'utilisateur la structure de notre structure de
// données qui contient des infos sur chaque zone memoire.

// Dans le sujet on aura la partie pubic, partie privé -> on fait ce qu'on veut

#ifndef _SECMALLOC_PRIVATE_H
#define _SECMALLOC_PRIVATE_H
#include <stdbool.h>

#include "my_secmalloc.h"

#include <stdio.h>
#include <alloca.h> // alloue de la mémoire qui se libère automatiquement.
#include <unistd.h>
#include <sys/mman.h>
#include <string.h>
#include <stdint.h>
#include <stdlib.h>
#include <fcntl.h>
#include <stdarg.h>
#include <syslog.h>

// sentez vous libre de modifier ce header comme vous le souhaitez
void *my_malloc(size_t size);
void my_free(void *ptr);
void *my_calloc(size_t nmemb, size_t size);
void *my_realloc(void *ptr, size_t size);

enum chunck_type
{
	FREE = 0,
	BUSY = 1,
	UNUSED = 2
};

// Representation des block de memoires
struct metablock
{
	size_t size;			// Taille du morceau de memoire.
	enum chunck_type flags; // Pour mauquer si le morceau de memoire est occupee ou pas
	void *ptr_to_data;		// Pointeur vers le bloc dans le datapool.
};

struct HeapCheckResult
{
	bool heap_integrity;
	struct chunk *chunk_ptr;
};

struct datablock
{
	long canary;
};

// Fonction interne pour l'initialisation du tas
// Retourne un chunck qui represente toute la memoire disponible
struct metablock *init_metapool_heap();
struct datablock *init_datapool_heap();

struct metablock *get_free_metachunck(size_t s);
struct datablock *get_free_datachunck(size_t s);

// Structure de métadatas avec une taille de 4096.
extern struct metablock *metapool_heap;
extern size_t metapool_heap_size;
// Structure de datas avec une taille de 4096.
extern struct datablock *datapool_heap;
extern size_t datapool_heap_size;

// LOG
int log_message(const char *format, ...);

#endif