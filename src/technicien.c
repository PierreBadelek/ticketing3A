#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ticketing.h"

#define SHM_NAME "/ticketing_shm"

SharedMemory *shared_mem;

int technicien_id;

int connect_to_server()
{
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1)
    {
        perror("Erreur: Impossible de se connecter au serveur");
        return -1;
    }

    shared_mem = mmap(NULL, sizeof(SharedMemory),
                      PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);

    if (shared_mem == MAP_FAILED)
    {
        perror("Erreur: Échec du mapping mémoire");
        close(shm_fd);
        return -1;
    }

    technicien_id = getpid();

    printf("Connecté au serveur avec Technicien ID: %d\n", technicien_id);
    return 0;
}

