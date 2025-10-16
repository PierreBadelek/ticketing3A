#ifndef TICKETING_H
#define TICKETING_H

#include <pthread.h>
#include <time.h>

#define MAX_TICKETS 5
#define MAX_DESCRIPTION 256
#define MAX_TITLE 100
#define PRIORITY_DELAY 86400  // 24h en secondes
#define SHM_NAME "/ticketing_shm"

typedef enum {
    TICKET_OPEN,
    TICKET_IN_PROGRESS,
    TICKET_CLOSED
} TicketStatus;

typedef struct {
    int id;
    char title[MAX_TITLE];
    char description[MAX_DESCRIPTION];
    time_t created_at;
    time_t updated_at;
    char date_event[20]; // date de l'événement
    TicketStatus status;
    int client_id;
    int technician_id;
    int is_priority;
} Ticket;

typedef struct {
    Ticket tickets[MAX_TICKETS];
    int ticket_count;
    int next_id;
    pthread_mutex_t mutex;
} SharedMemory;

// Fonctions utilitaires pour affichage 
const char* status_to_string(TicketStatus status);
const char* priority_to_string(int is_priority);

#endif