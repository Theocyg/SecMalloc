#define _GNU_SOURCE
#include "my_secmalloc.private.h"

// Définition de la page à 4096 octets.
#define PAGE_SIZE 4096
// Structure de métadatas avec une taille de 4096.
struct metablock *metapool_heap = NULL;
size_t metapool_heap_size = PAGE_SIZE;
// Structure de datas avec une taille de 4096.
struct datablock *datapool_heap = NULL;
size_t datapool_heap_size = PAGE_SIZE;

// LOG

FILE *log_file = NULL;

int log_message(const char *format, ...)
{
    va_list args, args_copy;
    va_start(args, format);

    // Déterminer la taille du buffer nécessaire
    va_copy(args_copy, args);
    size_t size = vsnprintf(NULL, 0, format, args_copy) + 1; // +1 pour le null-terminator
    va_end(args_copy);

    // Allouer le buffer sur la pile
    char *buffer = (char *)alloca(size);

    // Écrire les données formatées dans le buffer
    vsnprintf(buffer, size, format, args);
    va_end(args);

    // Initialisation du chemin du fichier de journalisation
    char *log_file = getenv("MSM_OUTPUT");
    if (log_file == NULL)
    {
        fprintf(stderr, "Erreur : Chemin du fichier de journalisation non défini.\n");
        return -1; // Retourner un code d'erreur
    }

    // Ouvrir le fichier de journalisation en mode ajout (append) et écriture seulement (write-only)
    int fd = open(log_file, O_APPEND | O_WRONLY | O_CREAT, 0644);
    if (fd == -1)
    {
        perror("Échec de l'ouverture du fichier de journalisation");
        return -1;
    }

    // Écrire le message de journalisation dans le fichier
    ssize_t ret = write(fd, buffer, strlen(buffer));
    if (ret == -1)
    {
        perror("Échec de l'écriture dans le fichier de journalisation");
    }

    close(fd);

    return ret == -1 ? -1 : 0; // Retourner 0 en cas de succès, -1 en cas d'erreur
}
// Initialisation d'une page de métadonnées avec l'adresse mémoire <metapool_heap> qui sera retournée.
// Dans la fonction init_metapool_heap

struct metablock *init_metapool_heap()
{
    log_message("Début de l'initialisation de metapool_heap.\n"); // Log au début

    if (metapool_heap == NULL)
    {
        metapool_heap = mmap((void *)(PAGE_SIZE), metapool_heap_size, PROT_WRITE | PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (metapool_heap == MAP_FAILED)
        {
            perror("Mmap doesn't work.");
            log_message("Échec de l'initialisation de metapool_heap avec mmap.\n"); // Log en cas d'échec
            return NULL;
        }
        metapool_heap->size = datapool_heap_size - sizeof(struct datablock);
        metapool_heap->flags = FREE;
        metapool_heap->ptr_to_data = datapool_heap;

        int nb_of_metadata_elements = (metapool_heap_size) / sizeof(struct metablock);
        for (int i = 1; i < nb_of_metadata_elements; i++)
        {
            metapool_heap[i].flags = UNUSED;
            metapool_heap[i].size = 0;
            metapool_heap[i].ptr_to_data = NULL;
        }
        log_message("Succès de l'initialisation de metapool_heap.\n"); // Log en cas de succès
    }
    else
    {
        log_message("metapool_heap déjà initialisé.\n"); // Log si déjà initialisé
    }
    return metapool_heap;
}
// Initialisation d'une page de données avec l'adresse mémoire <datapool_heap> qui sera retournée.
struct datablock *init_datapool_heap()
{
    log_message("Début de l'initialisation de datapool_heap.\n"); // Log au début

    if (datapool_heap == NULL)
    {
        datapool_heap = mmap((void *)(PAGE_SIZE), metapool_heap_size, PROT_WRITE | PROT_READ, MAP_ANON | MAP_PRIVATE, -1, 0);
        if (datapool_heap == MAP_FAILED)
        {
            perror("Mmap doesn't work.");
            log_message("Échec de l'initialisation de datapool_heap avec mmap.\n"); // Log en cas d'échec
            return NULL;
        }
        struct datablock *ptr_end = (struct datablock *)((size_t)datapool_heap + (PAGE_SIZE - sizeof(struct datablock)));
        ptr_end->canary = (long)0;

        log_message("Succès de l'initialisation de datapool_heap.\n"); // Log en cas de succès
    }
    else
    {
        log_message("datapool_heap déjà initialisé.\n"); // Log si déjà initialisé
    }

    return datapool_heap;
}

// Une fonction qui renvoie un pointeur vers le premier morceau libre de la taille appropriée. "s" correspond à la taille du chunck demandée.
struct metablock *get_free_metablock_raw(size_t s)
{
    log_message("Début de la recherche d'un metablock libre.\n"); // Log au début de la fonction

    // Parcourir toute la zone mémoire
    for (struct metablock *item = metapool_heap;                               // Commencer au début du tas à l'adresse mémoire de item. item est définit au début du metapool_heap.
         (size_t)item < (size_t)metapool_heap + metapool_heap_size;            // Pour que le pointeur item reste uniquement dans l'espace du tas équivalent à 4096.
         item = (struct metablock *)((size_t)item + sizeof(struct metablock))) // Passer au bloc suivant dans le métapool.
    {
        // Log pour chaque metablock vérifié
        char log_buffer[128];
        snprintf(log_buffer, sizeof(log_buffer), "Vérification du metablock à l'adresse %p, taille %lu, statut %u.\n", item, item->size, item->flags);
        log_message(log_buffer);

        // Si le morceau est libre et est égal ou supérieur au nombre d'octets requis
        if (item->size >= s && item->flags == FREE)
        {
            snprintf(log_buffer, sizeof(log_buffer), "Metablock libre trouvé à l'adresse %p, taille %lu.\n", item, item->size);
            log_message(log_buffer);
            return item;
        }
    }
    // Si aucun morceau libre de la taille appropriée n'est trouvé
    log_message("Aucun metablock libre de taille suffisante trouvé.\n");
    return NULL;
}

// Fonction qui permettra de ressortir un pointeur vers le début du chunck de données disponible dans le datapool
struct metablock *get_free_metablock(size_t s)
{
    log_message("Début de get_free_metablock.\n"); // Log au début de la fonction

    // Si le tas n'a pas encore été initialisé
    if (datapool_heap == NULL)
    {
        log_message("datapool_heap n'est pas initialisé, initialisation en cours...\n");
        datapool_heap = init_datapool_heap();
        if (datapool_heap == NULL)
        {
            log_message("Échec de l'initialisation de datapool_heap.\n");
            return NULL;
        }
    }

    // Une tentative d'obtenir un morceau de mémoire libre de la taille appropriée. On vérifie dans métapool.
    struct metablock *item = get_free_metablock_raw(s);

    // Si aucun morceau de mémoire libre de la taille appropriée n'est trouvé, on créera de nouvelles pages :
    if (item == NULL)
    {
        log_message("Aucun metablock libre trouvé, extension du datapool en cours...\n");

        size_t data_tok_chunck = s + sizeof(struct datablock);
        // Conserver la taille actuelle du tas de données, avant de l'augmenter.
        size_t data_old_size = datapool_heap_size;

        size_t number_of_pages = ((data_tok_chunck / PAGE_SIZE) + ((data_tok_chunck % PAGE_SIZE != 0) ? 1 : 0));
        size_t delta_size = number_of_pages * PAGE_SIZE;

        // Obtenir un pointeur vers le dernier morceau last_item.
        struct metablock *last_item = get_free_metablock_raw(s);

        // Concaténation de la taille supplémentaire avec la taille d'origine.
        datapool_heap_size += delta_size;
        log_message("Extension du datapool.\n");
        struct datablock *new_heap = mremap(datapool_heap, data_old_size, datapool_heap_size, MREMAP_MAYMOVE);
        if (new_heap == MAP_FAILED)
        {
            perror("Mremap doesn't work.");
            log_message("Échec de mremap, impossible d'étendre le datapool.\n");
            return NULL;
        }
        datapool_heap = new_heap;
        log_message("Datapool étendu avec succès.\n");

        last_item->size += delta_size;
        item = get_free_metablock_raw(s);
    }
    else
    {
        log_message("Metablock libre trouvé, pas besoin d'étendre le datapool.\n");
    }

    return item;
}

struct metablock *get_last_metablock_raw()
{
    log_message("Début de la recherche du dernier metablock.\n"); // Log au début de la fonction

    // Parcourir toute la zone mémoire.
    for (struct metablock *item = metapool_heap;                               // Commencer au début du tas
         (size_t)item < (size_t)metapool_heap + metapool_heap_size;            // Pour que le pointeur item reste uniquement dans l'espace du tas
         item = (struct metablock *)((size_t)item + sizeof(struct metablock))) // Passer au morceau suivant
    {
        // Log pour chaque metablock vérifié
        char log_buffer[128];
        snprintf(log_buffer, sizeof(log_buffer), "Vérification du metablock à l'adresse %p, taille %lu.\n", item, item->size);
        log_message(log_buffer);

        // Si marcher jusqu'à la fin du morceau actuel nous amène à la fin du tas ou au-delà
        if ((size_t)item + sizeof(struct metablock) + item->size >= (size_t)metapool_heap + metapool_heap_size)
        {
            snprintf(log_buffer, sizeof(log_buffer), "Dernier metablock trouvé à l'adresse %p.\n", item);
            log_message(log_buffer);
            return item;
        }
    }
    log_message("Aucun metablock trouvé, le tas semble être vide.\n");
    return NULL;
}

// Malloc doit parcourir la page, identifier un espace nécessaire en fonction de ce qui est demandé (size). Il retourne un pointeur vers le début de cet
// espace dans datapool.
void *my_malloc(size_t size)
{
    log_message("Début de my_malloc.\n"); // Log au début de la fonction

    if (datapool_heap == NULL)
    {
        log_message("datapool_heap n'est pas initialisé, initialisation en cours...\n");
        datapool_heap = init_datapool_heap();
        if (datapool_heap == NULL)
        {
            log_message("Échec de l'initialisation de datapool_heap.\n");
            return NULL;
        }
    }
    if (metapool_heap == NULL)
    {
        log_message("metapool_heap n'est pas initialisé, initialisation en cours...\n");
        metapool_heap = init_metapool_heap();
        if (metapool_heap == NULL)
        {
            log_message("Échec de l'initialisation de metapool_heap.\n");
            return NULL;
        }
    }

    log_message("Recherche d'un metablock libre.\n");
    struct metablock *ch = get_free_metablock(size);
    if (!ch)
    {
        log_message("Aucun metablock libre trouvé, échec de my_malloc.\n");
        return NULL;
    }

    struct metablock *next = (struct metablock *)((size_t)ch + sizeof(struct metablock));
    struct datablock *next_data = (struct datablock *)((size_t)ch->ptr_to_data + size);

    size_t old_size = ch->size;

    ch->size = size;
    ch->flags = BUSY;

    char log_buffer[256];
    snprintf(log_buffer, sizeof(log_buffer), "Allocation de %zu octets dans metablock à l'adresse %p.\n", size, ch->ptr_to_data);
    log_message(log_buffer);

    if (((size_t)next < (size_t)metapool_heap + metapool_heap_size) && ((size_t)next_data < (size_t)datapool_heap + datapool_heap_size))
    {
        next->flags = FREE;
        next->size = old_size - ch->size;
        next->ptr_to_data = next_data;
        next_data->canary = 0;

        snprintf(log_buffer, sizeof(log_buffer), "Création d'un nouveau metablock libre après allocation, taille restante %zu.\n", next->size);
        log_message(log_buffer);
    }
    return ch->ptr_to_data;
}

// On veut libérer la mémoire. ptr va pointer vers le bloc de mémoire à libérer.
void my_free(void *ptr)
{
    log_message("Début de my_free.\n"); // Log au début de la fonction

    if (ptr == NULL)
    {
        log_message("Le pointeur est NULL, my_free interrompu.\n");
        return;
    }

    bool found = false;
    for (struct metablock *item = metapool_heap;                               // Commencer au début du tas
         (size_t)item < (size_t)metapool_heap + metapool_heap_size;            // Pour que le pointeur item reste uniquement dans l'espace du tas
         item = (struct metablock *)((size_t)item + sizeof(struct metablock))) // Passer au morceau suivant
    {
        if (item->ptr_to_data == ptr && item->flags == BUSY)
        {
            memset(item->ptr_to_data, 0, item->size);
            item->flags = FREE;
            found = true;

            char log_buffer[256];
            snprintf(log_buffer, sizeof(log_buffer), "Bloc à l'adresse %p libéré avec succès, taille %zu.\n", ptr, item->size);
            log_message(log_buffer);
            break;
        }
    }

    if (!found)
    {
        log_message("Bloc à libérer non trouvé.\n");
    }
}

// // Définition de la fonction my_calloc qui prend deux arguments :
void *my_calloc(size_t nmemb, size_t taille)
{
    size_t taille_totale = nmemb * taille;
    log_message("calloc: Taille: %zu", taille_totale);
    void *ptr = my_malloc(taille_totale);
    if (ptr)
    {
        memset(ptr, 0, taille_totale);
    }
    return ptr;
}

// void *my_realloc(void *ptr, size_t nouvelle_taille)
// {
//     if (ptr == NULL)
//     {
//         return my_malloc(nouvelle_taille);
//     }

//     if (nouvelle_taille == 0)
//     {
//         my_free(ptr);
//         return NULL;
//     }

//     metablock *bloc_courant = liste_libre;
//     while (bloc_courant)
//     {
//         if (bloc_courant->pointeur_data == ptr)
//         {
//             if (!bloc_courant->est_alloue)
//             {
//                 return NULL;
//             }

//             if (bloc_courant->taille >= nouvelle_taille)
//             {
//                 // log_message("realloc: Taille: %zu, Adresse: %p", nouvelle_taille, ptr);
//                 return ptr;
//             }

//             void *new_ptr = my_malloc(nouvelle_taille);
//             if (new_ptr)
//             {
//                 memcpy(new_ptr, ptr, bloc_courant->taille);
//                 my_free(ptr);
//             }
//             // log_message("realloc: Taille: %zu, Nouvelle Adresse: %p", nouvelle_taille, new_ptr);
//             return new_ptr;
//         }
//         bloc_courant = bloc_courant->suivant;
//     }
//     return NULL;
// }

#ifdef DYNAMIC
void *malloc(size_t size)
{
    return my_malloc(size);
}
void free(void *ptr)
{
    my_free(ptr);
}
void *calloc(size_t nmemb, size_t size)
{
    return my_calloc(nmemb, size);
}
void *realloc(void *ptr, size_t size)
{
    return my_realloc(ptr, size);
}
#endif