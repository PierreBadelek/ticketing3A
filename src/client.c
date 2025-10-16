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

int connect_to_server() {
    int shm_fd = shm_open(SHM_NAME, O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("Error: Cannot connect to server");
        return -1;
    }
    
    shared_mem = mmap(NULL, sizeof(SharedMemory), 
                      PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (shared_mem == MAP_FAILED) {
        perror("Error: Memory mapping failed");
        return -1;
    }
    
    printf("Connected to server successfully\n");
    return 0;
}

void display_menu() {

}

// Créé un nouveau ticket
void create_ticket() {
}

// Liste tous les tickets du client
void list_tickets() {
    
}

// Voir les détails d'un ticket
void view_ticket() {
    
}

// Mets à jour le statut d'un ticket
void update_ticket_status() {
}

int main() {
    if (connect_to_server() == -1) {
        printf("Erreur: La connexion au serveur a échouée\n");
        return 1;
    }
    
}