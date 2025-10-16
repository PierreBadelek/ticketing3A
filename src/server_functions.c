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
    switch(status) {
        case TICKET_OPEN: return "OUVERT";
        case TICKET_IN_PROGRESS: return "EN COURS";
        case TICKET_CLOSED: return "FERME";
        default: return "INCONNU";
    }
}

const char* priority_to_string(int is_priority) {
    return is_priority ? "PRIORITAIRE" : "NORMAL";
}

void log_event(const char *message) {
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
    shm_unlink(SHM_NAME);
    
    shm_fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0666);
    if (shm_fd == -1) {
        perror("shm_open");
        return -1;
    }
    
    if (ftruncate(shm_fd, sizeof(SharedMemory)) == -1) {
        perror("ftruncate");
        close(shm_fd);
        return -1;
    }
    
    shared_mem = mmap(NULL, sizeof(SharedMemory), 
                      PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd, 0);
    
    if (shared_mem == MAP_FAILED) {
        perror("mmap");
        close(shm_fd);
        return -1;
    }
    
    // Initialisation du mutex partagé entre processus
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
    pthread_mutex_init(&shared_mem->mutex, &attr);
    pthread_mutexattr_destroy(&attr);
    
    // Initialisation des données
    memset(shared_mem->tickets, 0, sizeof(shared_mem->tickets));
    shared_mem->ticket_count = 0;
    shared_mem->next_id = 1;
    
    log_event("Serveur de ticketing initialise");
    printf("   Nom de la memoire partagee: %s\n", SHM_NAME);
    printf("   Capacite: %d tickets maximum\n", MAX_TICKETS);
    printf("   Delai priorite: %d heures\n\n", PRIORITY_DELAY / 3600);
    
    return 0;
}

void cleanup_server(int sig) {
    printf("\n");
    log_event("Arret du serveur...");
    
    if (shared_mem != MAP_FAILED && shared_mem != NULL) {
        pthread_mutex_destroy(&shared_mem->mutex);
        munmap(shared_mem, sizeof(SharedMemory));
    }
    
    if (shm_fd != -1) {
        close(shm_fd);
    }
    
    shm_unlink(SHM_NAME);
    log_event("Nettoyage termine");
    exit(0);
}

// ============================================================================
// GESTION DES TICKETS
// ============================================================================

int count_active_tickets(void) {
    int count = 0;
    for (int i = 0; i < shared_mem->ticket_count; i++) {
        if (shared_mem->tickets[i].status != TICKET_CLOSED) {
            count++;
        }
    }
    return count;
}

int count_technician_tickets(int technician_id) {
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
    time_t now = time(NULL);
    int priority_count = 0;
    
    pthread_mutex_lock(&shared_mem->mutex);
    
    for (int i = 0; i < shared_mem->ticket_count; i++) {
        if (shared_mem->tickets[i].status == TICKET_OPEN) {
            double elapsed = difftime(now, shared_mem->tickets[i].created_at);
            
            if (elapsed >= PRIORITY_DELAY && !shared_mem->tickets[i].is_priority) {
                shared_mem->tickets[i].is_priority = 1;
                priority_count++;
                
                char msg[256];
                snprintf(msg, sizeof(msg), 
                        "Ticket #%d devenu PRIORITAIRE (cree il y a %.0f heures)",
                        shared_mem->tickets[i].id, elapsed / 3600);
                log_event(msg);
            }
        }
    }
    
    pthread_mutex_unlock(&shared_mem->mutex);
    
    if (priority_count > 0) {
        char msg[128];
        snprintf(msg, sizeof(msg), "%d ticket(s) prioritaire(s) en attente", 
                priority_count);
        log_event(msg);
    }
}

void cleanup_closed_tickets(void) {
    pthread_mutex_lock(&shared_mem->mutex);
    
    // Compter les tickets actifs
    int active_count = count_active_tickets();
    
    // Si on a moins de 5 tickets actifs, pas besoin de nettoyer
    if (active_count < MAX_TICKETS) {
        pthread_mutex_unlock(&shared_mem->mutex);
        return;
    }
    
    // Trouver le ticket fermé le plus ancien
    int oldest_closed_index = -1;
    time_t oldest_time = 0;
    
    for (int i = 0; i < shared_mem->ticket_count; i++) {
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
        
        shared_mem->ticket_count--;
        
        char msg[128];
        snprintf(msg, sizeof(msg), 
                "Ticket #%d supprime (ferme le plus ancien)", removed_id);
        log_event(msg);
    }
    
    pthread_mutex_unlock(&shared_mem->mutex);
}

// ============================================================================
// AFFICHAGE
// ============================================================================

void display_ticket(Ticket *ticket) {
    time_t now = time(NULL);
    double elapsed = difftime(now, ticket->created_at);
    
    printf("  [#%d] [%s] [%s]\n", 
           ticket->id, 
           status_to_string(ticket->status),
           priority_to_string(ticket->is_priority));
    printf("      Titre: %s\n", ticket->title);
    printf("      Description: %s\n", ticket->description);
    printf("      Client ID: %d\n", ticket->client_id);
    
    if (ticket->technician_id > 0) {
        printf("      Technicien ID: %d\n", ticket->technician_id);
    }
    
    printf("      Cree il y a: %.0f heures\n", elapsed / 3600);
    printf("\n");
}

void display_server_status(void) {
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