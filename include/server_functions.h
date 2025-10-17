#ifndef SERVER_FUNCTIONS_H
#define SERVER_FUNCTIONS_H

#include "ticketing.h"

// Initialisation et nettoyage
int init_shared_memory(void);
void cleanup_server(int sig);

// Gestion des tickets
void check_priority_tickets(void);
void cleanup_closed_tickets(void);
int count_active_tickets(void);
int count_technician_tickets(int technician_id);

// Affichage et logs
void display_server_status(void);
void display_ticket(Ticket *ticket);
void log_event(const char *message);

// Sauvegarde JSON
void save_tickets_to_json(void);

// Thread de surveillance
void* monitoring_thread(void *arg);

#endif