#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include "include/ticketing.h"

int main() {
    printf("=== TEST CREATION DE TICKET ===\n\n");
    
    // 1. Se connecter à la mémoire partagée
    printf("1. Connexion au serveur...\n");
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    
    if (shm_fd == -1) {
        perror("shm_open");
        printf("❌ Le serveur doit être lancé d'abord!\n");
        printf("💡 Lancez './server' dans un autre terminal\n");
        return 1;
    }
    
    SharedMemory *mem = mmap(NULL, sizeof(SharedMemory),
                             PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (mem == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return 1;
    }
    
    printf("✓ Connecté au serveur!\n\n");
    
    // 2. Créer un ticket de test
    printf("2. Création d'un ticket...\n");
    pthread_mutex_lock(&mem->mutex);
    
    if (mem->ticket_count < MAX_TICKETS) {
        Ticket *t = &mem->tickets[mem->ticket_count];
        t->id = mem->next_id++;
        strcpy(t->title, "Imprimante en panne");
        strcpy(t->description, "L'imprimante du 2ème étage ne s'allume plus");
        t->created_at = time(NULL);
        t->updated_at = time(NULL);
        t->status = TICKET_OPEN;
        t->client_id = 1001;
        t->technician_id = 0;
        t->is_priority = 0;
        
        mem->ticket_count++;
        
        printf("✅ Ticket #%d créé avec succès!\n", t->id);
        printf("   Titre: %s\n", t->title);
        printf("   Description: %s\n", t->description);
        printf("   Client ID: %d\n", t->client_id);
    } else {
        printf("❌ Mémoire pleine (5 tickets max)!\n");
    }
    
    pthread_mutex_unlock(&mem->mutex);
    
    // 3. Déconnexion
    printf("\n3. Déconnexion...\n");
    munmap(mem, sizeof(SharedMemory));
    close(shm_fd);
    
    printf("✓ Test terminé!\n");
    printf("\n💡 Appuyez sur ENTRÉE dans le terminal du serveur pour voir le ticket\n");
    
    return 0;
}