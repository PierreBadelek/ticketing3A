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

// Fonction d'initialisation de la mémoire partagée
int init_shared_memory() {
    int shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }
    
    if (ftruncate(shm_fd, sizeof(SharedMemory)) == -1) {
        perror("ftruncate");
        return -1;
    }
    
    shared_mem = mmap(NULL, sizeof(SharedMemory), 
                      PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (shared_mem == MAP_FAILED) {
        perror("mmap");
        return -1;
    }
    
    // Initialisation du mutex
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_mem->mutex, &attr);
    
    shared_mem->ticket_count = 0;
    
    printf("Serveur de ticketing initialisé\n");
    return 0;
}

int main() {
    if (init_shared_memory() == -1) {
        return 1;
    }
    
    printf("Serveur en attente de connexions...\n");
    
    // Le serveur reste actif
    while (1) {
        sleep(1);
    }
    
    return 0;
}