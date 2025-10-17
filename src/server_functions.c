#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "ticketing.h"
#include "server_functions.h"

extern SharedMemory *shared_mem;
extern int shm_fd;

// ============================================================================
// FONCTIONS UTILITAIRES
// ============================================================================

const char* status_to_string(TicketStatus status) {
    // Convertit le statut d'un ticket en chaîne lisible
    switch(status) {
        case TICKET_OPEN: return "OUVERT";
        case TICKET_IN_PROGRESS: return "EN COURS";
        case TICKET_CLOSED: return "FERME";
        default: return "INCONNU";
    }
}

const char* priority_to_string(int is_priority) { 
    // Convertit le statut de priorité en chaîne lisible
    if (is_priority) {
        return "PRIORITAIRE";
    } else {
        return "NORMAL";
    }
}

void log_event(const char *message) {
    // affiche l'heure et la date de l'événement suivi du message
    time_t now = time(NULL);
    char time_str[26];
    ctime_r(&now, time_str);
    time_str[24] = '\0'; // Enlever le \n
    printf("[%s] %s\n", time_str, message);
}

// ============================================================================
// INITIALISATION
// ============================================================================

int init_shared_memory(void) {
    // Nettoyer une éventuelle mémoire partagée existante
    shm_unlink(SHM_NAME); // On supprime l'ancienne mémoire partagée si elle existe
    // Shm est un descripteur de fichier pour la mémoire partagée
    
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666); // Crée ou ouvre la mémoire partagée avec droits de lecture/écriture
    if (shm_fd == -1) {
        // On vérifie si l'ouverture a réussi
        perror("shm_open");
        return -1;
    }
    
    if (ftruncate(shm_fd, sizeof(SharedMemory)) == -1) {
        // On définit la taille de la mémoire partagée, si échec on ferme et quitte
        perror("ftruncate");
        close(shm_fd);
        return -1;
    }
    
    shared_mem = mmap(NULL, sizeof(SharedMemory), PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0); // On mappe la mémoire partagée
    if (shared_mem == MAP_FAILED) {
        // On vérifie si le mapping a réussi
        perror("mmap");
        close(shm_fd);
        return -1;
    }
    
    // Initialisation du mutex partagé entre processus
    pthread_mutexattr_t attr; // Création des attributs du mutex
    pthread_mutexattr_init(&attr); // Initialisation des attributs du mutex
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED); // On indique que le mutex sera partagé entre processus
    pthread_mutex_init(&shared_mem->mutex, &attr); // Initialisation du mutex avec les attributs dans la mémoire partagée
    pthread_mutexattr_destroy(&attr); // Destruction des attributs du mutex 
    
    // Initialisation des données dans la mémoire partagée
    memset(shared_mem->tickets, 0, sizeof(shared_mem->tickets)); // Dans la mémoire partagée, on initialise tous les tickets de notre tableau ticket à 0
    shared_mem->ticket_count = 0;
    shared_mem->next_id = 1;
    shared_mem->next_client_id = 1;  // Initialiser le compteur de clients
    
    log_event("Serveur de ticketing initialise");
    printf("   Nom de la memoire partagee: %s\n", SHM_NAME);
    printf("   Capacite: %d tickets maximum\n", MAX_TICKETS);
    printf("   Delai priorite: %d heures\n\n", PRIORITY_DELAY / 3600); // On divise par 3600 pour que notre Priority_delay soit exrimée en heures (24h)
    
    return 0;
}

void cleanup_server(int sig) {
    // Nettoyage de la mémoire partagée et des ressources avant de quitter
    printf("\n");
    log_event("Arret du serveur...");
    
    if (shared_mem != MAP_FAILED && shared_mem != NULL) {
        pthread_mutex_destroy(&shared_mem->mutex); // Destruction du mutex
        munmap(shared_mem, sizeof(SharedMemory)); // On détache la mémoire partagée de notre espace d'adressage
    }
    
    if (shm_fd != -1) {
        close(shm_fd); // On ferme le descripteur de fichier de la mémoire partagée
    }
    
    shm_unlink(SHM_NAME); // On supprime la mémoire partagée du système
    log_event("Nettoyage termine");
    exit(0);
}

// ============================================================================
// GESTION DES TICKETS
// ============================================================================

int count_active_tickets(void) {
    // Compte le nombre de tickets actifs
    int count = 0;
    for (int i = 0; i < shared_mem->ticket_count; i++) {
        if (shared_mem->tickets[i].status != TICKET_CLOSED) {
            count++;
        }
    }
    return count;
}

int count_technician_tickets(int technician_id) {
    // Compte le nombre de tickets en cours pour un technicien passé en parametre
    int count = 0;
    for (int i = 0; i < shared_mem->ticket_count; i++) {
        if (shared_mem->tickets[i].technician_id == technician_id &&
            shared_mem->tickets[i].status == TICKET_IN_PROGRESS) {
            count++;
        }
    }
    return count;
}

void check_priority_tickets(void) {
    // Vérifie les tickets ouverts et les marque comme prioritaires si le délai est dépassé
    time_t now = time(NULL);
    int priority_count = 0;
    
    pthread_mutex_lock(&shared_mem->mutex);
    
    for (int i = 0; i < shared_mem->ticket_count; i++) {
        if (shared_mem->tickets[i].status == TICKET_OPEN) {
            double elapsed = difftime(now, shared_mem->tickets[i].created_at); // Temps écoulé depuis la création du ticket
            
            if (elapsed >= PRIORITY_DELAY && !shared_mem->tickets[i].is_priority) {
                shared_mem->tickets[i].is_priority = 1;
                priority_count++;
                
                char msg[256];
                snprintf(msg, sizeof(msg), "Ticket #%d devenu PRIORITAIRE (cree il y a %.0f heures)",shared_mem->tickets[i].id, elapsed / 3600); // Création d'un message pour indiquer que 
                log_event(msg);
            }
        }
    }
    
    pthread_mutex_unlock(&shared_mem->mutex);
    
    if (priority_count > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d ticket(s) prioritaire(s) en attente", priority_count);
        log_event(msg);
    }
}

void cleanup_closed_tickets(void) {
    pthread_mutex_lock(&shared_mem->mutex);
    
    // Compter les tickets actifs
    int active_count = count_active_tickets();
    
    // Si on a moins de MAX_TICKETS tickets actifs, pas besoin de nettoyer
    if (active_count < MAX_TICKETS) {
        pthread_mutex_unlock(&shared_mem->mutex);
        return;
    }
    
    // Trouver le ticket fermé le plus ancien
    int oldest_closed_index = -1;
    time_t oldest_time = 0;
    
    for (int i = 0; i < shared_mem->ticket_count; i++) {
        // Cherche l'indice du ticket fermé le plus ancien
        if (shared_mem->tickets[i].status == TICKET_CLOSED) {
            if (oldest_closed_index == -1 || 
                shared_mem->tickets[i].updated_at < oldest_time) {
                oldest_closed_index = i;
                oldest_time = shared_mem->tickets[i].updated_at;
            }
        }
    }
    
    // Supprimer le plus ancien ticket fermé
    if (oldest_closed_index != -1) {
        int removed_id = shared_mem->tickets[oldest_closed_index].id;
        
        // Décaler tous les tickets après celui-ci
        for (int i = oldest_closed_index; i < shared_mem->ticket_count - 1; i++) {
            shared_mem->tickets[i] = shared_mem->tickets[i + 1];
        }
        
        shared_mem->ticket_count--; // On retire un ticket du compte total
        
        char msg[128];
        snprintf(msg, sizeof(msg), "Ticket #%d supprime (ferme le plus ancien)", removed_id);
        log_event(msg);
    }
    
    pthread_mutex_unlock(&shared_mem->mutex);
}

// ============================================================================
// AFFICHAGE
// ============================================================================

void display_ticket(Ticket *ticket) {
    // Affiche les détails d'un ticket

    time_t now = time(NULL);

    double elapsed = difftime(now, ticket->created_at);

    printf("  [#%d] [%s] [%s]\n", ticket->id, status_to_string(ticket->status),priority_to_string(ticket->is_priority));
    printf("      Titre : %s\n", ticket->title);
    printf("      Description : %s\n", ticket->description);
    printf("      Client ID : %d\n", ticket->client_id);
    
    if (ticket->technician_id > 0) {
        printf("      Technicien ID : %d\n", ticket->technician_id);
    }
    
    printf("      Cree il y a : %.0f heures\n", elapsed / 3600);
    printf("\n");
}

void display_server_status(void) {
    // Affiche le statut actuel du serveur et des tickets
    
    pthread_mutex_lock(&shared_mem->mutex);
    
    printf("\n");
    printf("===============================================================\n");
    printf("              STATUT DU SERVEUR DE TICKETING\n");
    printf("===============================================================\n");
    
    printf("Tickets total: %d/%d\n", shared_mem->ticket_count, MAX_TICKETS);
    printf("Prochain ID: %d\n\n", shared_mem->next_id);
    
    // Compter par statut
    int open = 0, in_progress = 0, closed = 0, priority = 0;
    
    for (int i = 0; i < shared_mem->ticket_count; i++) {
        // Ajuste les compteurs selon le statut du ticket
        switch(shared_mem->tickets[i].status) {
            case TICKET_OPEN: open++; break;
            case TICKET_IN_PROGRESS: in_progress++; break;
            case TICKET_CLOSED: closed++; break;
        }
        if (shared_mem->tickets[i].is_priority) priority++;
    }
    
    printf("Ouverts: %d\n", open);
    printf("En cours: %d\n", in_progress);
    printf("Fermes: %d\n", closed);
    printf("Prioritaires: %d\n\n", priority);
    
    if (shared_mem->ticket_count > 0) {
        printf("---------------------------------------------------------------\n");
        printf("                    LISTE DES TICKETS\n");
        printf("---------------------------------------------------------------\n\n");
        
        for (int i = 0; i < shared_mem->ticket_count; i++) {
            display_ticket(&shared_mem->tickets[i]);
        }
    } else {
        printf("Aucun ticket dans le systeme.\n\n");
    }
    
    printf("===============================================================\n\n");
    
    pthread_mutex_unlock(&shared_mem->mutex);
}

// ============================================================================
// THREAD DE SURVEILLANCE
// ============================================================================

void* monitoring_thread(void *arg) {
    log_event("Thread de surveillance demarre");
    
    int cycle = 0;
    
    while (1) {
        sleep(60); // Vérification toutes les minutes
        cycle++;
        
        // Vérifier les tickets prioritaires
        check_priority_tickets();
        
        // Nettoyer les vieux tickets fermés si nécessaire
        cleanup_closed_tickets();
        
        // Afficher le statut toutes les 5 minutes
        if (cycle % 5 == 0) {
            log_event("Affichage automatique du statut");
            display_server_status();
        }
    }
    
    return NULL;
}