#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>
#include "ticketing.h"
#include "server_functions.h"

SharedMemory *shared_mem = NULL;
int shm_fd = -1;

int main() {
    pthread_t monitor_thread;

    printf("\n");
    printf("===============================================================\n");
    printf("           SERVEUR DE TICKETING - DEMARRAGE\n");
    printf("===============================================================\n\n");

    // Installer les gestionnaires de signaux
    signal(SIGINT, cleanup_server);
    signal(SIGTERM, cleanup_server);

    // Initialiser la mémoire partagée
    if (init_shared_memory() == -1) {
        fprintf(stderr, "Erreur d'initialisation du serveur\n");
        return 1;
    }

    log_event("Serveur de ticketing demarre");
    log_event("En attente de connexions clients...");

    printf("\n");
    printf("Commandes:\n");
    printf("   - Appuyez sur ENTREE pour voir le statut\n");
    printf("   - Ctrl+C pour arreter le serveur\n");
    printf("\n");
    printf("===============================================================\n\n");

    // Lancer le thread de surveillance
    if (pthread_create(&monitor_thread, NULL, monitoring_thread, NULL) != 0) {
        perror("pthread_create");
        cleanup_server(0);
        return 1;
    }

    // Détacher le thread (il tournera indépendamment)
    pthread_detach(monitor_thread);

    // Boucle principale : affichage du statut sur demande
    char input[10];
    while (1) {
        if (fgets(input, sizeof(input), stdin) != NULL) {
            display_server_status();
        }
    }
    
    return 0;
}