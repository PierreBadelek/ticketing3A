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

// Déclarations des fonctions
int connect_to_server();
void auto_assign_priority_tickets();
void process_technicien_input();
void show_main_menu();
void handle_list_and_assign();
void handle_list_and_close();
void list_tickets();
void list_tickets_technicien();
void assign_ticket(int ticket_id);
void close_ticket(int ticket_id);
void priority_ticket(int ticket_id);

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

    pthread_mutex_lock(&shared_mem->mutex);
    technicien_id = shared_mem->next_technicien_id++;
    pthread_mutex_unlock(&shared_mem->mutex);

    printf("Connecté au serveur avec Technicien ID: %d\n", technicien_id);
    
    // assigne automatiquement les tickets prioritaires (âgés de 1 jour) lors de la connexion
    auto_assign_priority_tickets();
    
    return 0;
}

void auto_assign_priority_tickets()
{
    pthread_mutex_lock(&shared_mem->mutex);
    int assigned_count = 0;
    time_t now = time(NULL);
    
    printf("\n=== Auto-assignation des tickets prioritaires ===\n");
    
    for (int i = 0; i < shared_mem->ticket_count; i++)
    {
        Ticket *t = &shared_mem->tickets[i];
        if (t->is_priority && 
            t->status == TICKET_OPEN && 
            t->technician_id == -1 &&
            (now - t->created_at) >= 86400) // 1 jour en secondes
        {
            if (assigned_count < 5) // Limite de tickets assignés
            {
                t->technician_id = technicien_id;
                t->status = TICKET_IN_PROGRESS;
                t->updated_at = now;
                printf("  Ticket ID %d (créé il y a %ld heures) assigné automatiquement.\n", 
                       t->id, (now - t->created_at) / 3600);
                assigned_count++;
            }
        }
    }
    
    if (assigned_count == 0)
    {
        printf("  Aucun ticket prioritaire de plus d'un jour à assigner.\n");
    }
    else
    {
        printf("  Total: %d ticket(s) prioritaire(s) assigné(s).\n", assigned_count);
    }
    printf("=================================================\n\n");
    
    pthread_mutex_unlock(&shared_mem->mutex);
}

void show_main_menu()
{
    printf("\n=== Menu Principal ===\n");
    printf("  connectTicket -l : Lister et assigner des tickets disponibles\n");
    printf("  connectTicket -t : Lister et fermer vos tickets assignés\n");
    printf("  connectTicket -p : Assigner tous les tickets prioritaires (max 5)\n");
    printf("  quit             : Quitter le programme\n");
    printf("======================\n");
}

void process_technicien_input()
{
    char command[256];
    
    while (1)
    {
        show_main_menu();
        printf("Commande> ");
        
        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            printf("Erreur de lecture de la commande.\n");
            continue;
        }

        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "connectTicket -l") == 0)
        {
            handle_list_and_assign();
        }
        else if (strcmp(command, "connectTicket -t") == 0)
        {
            handle_list_and_close();
        }
        else if (strcmp(command, "connectTicket -p") == 0)
        {
            pthread_mutex_lock(&shared_mem->mutex);
            int assigned_count = 0;
            
            printf("\n=== Assignation des tickets prioritaires ===\n");
            for (int i = 0; i < shared_mem->ticket_count; i++)
            {
                Ticket *t = &shared_mem->tickets[i];
                if (t->is_priority && t->status == TICKET_OPEN && t->technician_id == -1)
                {
                    t->technician_id = technicien_id;
                    t->status = TICKET_IN_PROGRESS;
                    t->updated_at = time(NULL);
                    assigned_count++;
                    printf("  Ticket ID %d assigné avec succès.\n", t->id);
                    if (assigned_count >= 5) // Limite de tickets assignés
                        break;
                }
            }
            
            if (assigned_count == 0)
            {
                printf("  Aucun ticket prioritaire disponible pour l'assignation.\n");
            }
            else
            {
                printf("  Total: %d ticket(s) prioritaire(s) assigné(s).\n", assigned_count);
            }
            printf("============================================\n");
            
            pthread_mutex_unlock(&shared_mem->mutex);
        }
        else if (strcmp(command, "quit") == 0)
        {
            printf("Déconnexion du serveur...\n");
            break;
        }
        else if (strcmp(command, "connectTicket") == 0)
        {
            continue;
        }
        else
        {
            printf("Commande inconnue. Veuillez réessayer.\n");
        }
    }
}

void handle_list_and_assign()
{
    char command[256];
    
    while (1)
    {
        list_tickets();
        
        printf("\n--- Mode Assignation ---\n");
        printf("  assignTicket <id> : Assigner un ticket\n");
        printf("  back              : Retour au menu principal\n");
        printf("Commande> ");
        
        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            printf("Erreur de lecture de la commande.\n");
            continue;
        }

        command[strcspn(command, "\n")] = 0;

        if (strncmp(command, "assignTicket ", 13) == 0)
        {
            int ticket_id = atoi(command + 13);
            assign_ticket(ticket_id);
        }
        else if (strcmp(command, "back") == 0)
        {
            printf("Retour au menu principal...\n");
            break;
        }
        else
        {
            printf("Commande inconnue. Utilisez 'assignTicket <id>' ou 'back'.\n");
        }
    }
}

void handle_list_and_close()
{
    char command[256];
    
    while (1)
    {
        list_tickets_technicien();
        
        printf("\n--- Mode Fermeture ---\n");
        printf("  closeTicket <id> : Fermer un ticket\n");
        printf("  back             : Retour au menu principal\n");
        printf("Commande> ");
        
        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            printf("Erreur de lecture de la commande.\n");
            continue;
        }

        command[strcspn(command, "\n")] = 0; // Remove newline

        if (strncmp(command, "closeTicket ", 12) == 0)
        {
            int ticket_id = atoi(command + 12);
            close_ticket(ticket_id);
        }
        else if (strcmp(command, "back") == 0)
        {
            printf("Retour au menu principal...\n");
            break; // Return to main menu
        }
        else
        {
            printf("Commande inconnue. Utilisez 'closeTicket <id>' ou 'back'.\n");
        }
    }
}


void list_tickets()
{
    pthread_mutex_lock(&shared_mem->mutex);
    int found = 0;

    // Affiche les tickets disponibles (non assignés)
    printf("\n=== Tickets Disponibles ===\n");
    for (int i = 0; i < shared_mem->ticket_count; i++)
    {
        if (shared_mem->tickets[i].status == TICKET_OPEN && shared_mem->tickets[i].technician_id == -1)
        {
            Ticket *t = &shared_mem->tickets[i];
            printf("  ID: %d | Titre: %s | Statut: %s | Prioritaire: %s\n", 
                   t->id, t->title,
                   (t->status == TICKET_OPEN) ? "Ouvert"
                   : (t->status == TICKET_IN_PROGRESS) ? "En cours" : "Fermé",
                   t->is_priority ? "Oui" : "Non");
            found = 1;
        }
    }

    // Aucun tickets trouvés
    if (!found)
    {
        printf("  Aucun ticket disponible.\n");
    }
    printf("===========================\n");
    pthread_mutex_unlock(&shared_mem->mutex);
}

void list_tickets_technicien()
{
    pthread_mutex_lock(&shared_mem->mutex);
    int found = 0;

    // Affiche les tickets du technicien
    printf("\n=== Vos Tickets Assignés ===\n");
    for (int i = 0; i < shared_mem->ticket_count; i++)
    {
        if (shared_mem->tickets[i].technician_id == technicien_id)
        {
            Ticket *t = &shared_mem->tickets[i];
            printf("  ID: %d | Titre: %s | Statut: %s\n", 
                   t->id, t->title,
                   (t->status == TICKET_OPEN) ? "Ouvert"
                   : (t->status == TICKET_IN_PROGRESS) ? "En cours" : "Fermé");
            found = 1;
        }
    }

    // Aucun tickets trouvés
    if (!found)
    {
        printf("  Aucun ticket assigné.\n");
    }
    printf("============================\n");
    pthread_mutex_unlock(&shared_mem->mutex);
}

void assign_ticket(int ticket_id)
{
    pthread_mutex_lock(&shared_mem->mutex);
    for (int i = 0; i < shared_mem->ticket_count; i++)
    {
        Ticket *t = &shared_mem->tickets[i];
        if (t->id == ticket_id && t->status == TICKET_OPEN && t->technician_id == -1)
        {
            t->technician_id = technicien_id;
            t->status = TICKET_IN_PROGRESS;
            t->updated_at = time(NULL);
            printf("Ticket ID %d assigné avec succès.\n", ticket_id);
            pthread_mutex_unlock(&shared_mem->mutex);
            return;
        }
    }
    printf("Erreur: Ticket ID %d introuvable ou déjà assigné.\n", ticket_id);
    pthread_mutex_unlock(&shared_mem->mutex);
}

void close_ticket(int ticket_id)
{
    pthread_mutex_lock(&shared_mem->mutex);
    for (int i = 0; i < shared_mem->ticket_count; i++)
    {
        Ticket *t = &shared_mem->tickets[i];
        if (t->id == ticket_id && t->status == TICKET_IN_PROGRESS && t->technician_id == technicien_id)
        {
            t->status = TICKET_CLOSED;
            t->updated_at = time(NULL);
            printf("Ticket ID %d fermé avec succès.\n", ticket_id);
            pthread_mutex_unlock(&shared_mem->mutex);
            return;
        }
    }
    printf("Erreur: Ticket ID %d introuvable ou non assigné à vous.\n", ticket_id);
    pthread_mutex_unlock(&shared_mem->mutex);
}

void priority_ticket(int ticket_id)
{
    pthread_mutex_lock(&shared_mem->mutex);
    for (int i = 0; i < shared_mem->ticket_count; i++)
    {
        Ticket *t = &shared_mem->tickets[i];
        if (t->id == ticket_id && t->status == TICKET_OPEN)
        {
            t->is_priority = 1;
            t->updated_at = time(NULL);
            printf("Ticket ID %d marqué comme prioritaire.\n", ticket_id);
            pthread_mutex_unlock(&shared_mem->mutex);
            return;
        }
    }
    printf("Erreur: Ticket ID %d introuvable ou non ouvert.\n", ticket_id);
    pthread_mutex_unlock(&shared_mem->mutex);
}

int main()
{
    if (connect_to_server() == -1)
    {
        printf("Erreur: La connexion au serveur a échouée\n");
        return 1;
    }

    process_technicien_input();

    // Nettoyage
    munmap(shared_mem, sizeof(SharedMemory));
    return 0;
}