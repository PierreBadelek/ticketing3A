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
void process_client_input();
void create_ticket(const char *titre, const char *description);
void list_tickets();

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

void process_technicien_input()
{
    char command[256];
    while (1)
    {
        printf("Entrez une commande (connectTicket -l | connectTicket -t| exit): ");
        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            printf("Erreur de lecture de la commande.\n");
            continue;
        }

        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "connectTicket") == 0)
        {
        printf("Options disponibles:\n");
        printf("  connectTicket -l    : Lister les tickets disponibles\n");
        printf("  connectTicket -t    : Lister les tickets assignés\n");
        printf("  connectTicket -p    : Assigner les tickets prioritaires jusqu'à la limite du technicien\n");
        }
        else if (strcmp(command, "connectTicket -l") == 0)
        {
            list_tickets();
            printf("Entrez une commande (assignTicket <id> | exit): ");
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
            else if (strcmp(command, "exit") == 0)
            {
                printf("Déconnexion du serveur...\n");
                break;
            }
            else
            {
                printf("Commande inconnue. Veuillez réessayer.\n");
            }
        }

        else if (strcmp(command, "connectTicket -t") == 0)
        {
            list_tickets_technicien();
            printf("Entrez une commande (closeTicket <id> | exit): ");
            if (fgets(command, sizeof(command), stdin) == NULL)
            {
                printf("Erreur de lecture de la commande.\n");
                continue;
            }

            command[strcspn(command, "\n")] = 0;

            if (strncmp(command, "closeTicket ", 12) == 0)
            {
                int ticket_id = atoi(command + 12);
                close_ticket(ticket_id);
            }
            else if (strcmp(command, "exit") == 0)
            {
                printf("Déconnexion du serveur...\n");
                break;
            }
            else
            {
                printf("Commande inconnue. Veuillez réessayer.\n");
            }
        }
        else if (strcmp(command, "connectTicket -p") == 0)
        {
            pthread_mutex_lock(&shared_mem->mutex);
            int assigned_count = 0;
            for (int i = 0; i < shared_mem->ticket_count; i++)
            {
                Ticket *t = &shared_mem->tickets[i];
                if (t->is_priority && t->status == TICKET_OPEN && t->technician_id == -1)
                {
                    t->technician_id = technicien_id;
                    t->status = TICKET_IN_PROGRESS;
                    t->updated_at = time(NULL);
                    assigned_count++;
                    printf("Ticket ID %d assigné avec succès.\n", t->id);
                    if (assigned_count >= 5) // Limite de tickets assignés
                        break;
                }
            }
            if (assigned_count == 0)
            {
                printf("Aucun ticket prioritaire disponible pour l'assignation.\n");
            }
            pthread_mutex_unlock(&shared_mem->mutex);
        }
        else if (strcmp(command, "exit") == 0)
        {
            printf("Déconnexion du serveur...\n");
            break;
        }
        else
        {
            printf("Commande inconnue. Veuillez réessayer.\n");
        }
    }
}


void list_tickets()
{
    pthread_mutex_lock(&shared_mem->mutex);
    int found = 0;

    // Affiche les tickets du client
    printf("Vos tickets:\n");
    for (int i = 0; i < shared_mem->ticket_count; i++)
    {
        if (shared_mem->tickets[i].client_id == client_id)
        {
            Ticket *t = &shared_mem->tickets[i];
            printf("ID: %d | Titre: %s | Statut: %s\n", t->id, t->title,
                   (t->status == TICKET_OPEN)
                       ? "Ouvert"
                   : (t->status == TICKET_IN_PROGRESS) ? "En cours"
                                                       : "Fermé");
            found = 1;
        }
    }

    // Aucun tickets trouvés
    if (!found)
    {
        printf("Aucun ticket trouvé.\n");
    }
    pthread_mutex_unlock(&shared_mem->mutex);
}

void list_tickets_technicien()
{
    pthread_mutex_lock(&shared_mem->mutex);
    int found = 0;

    // Affiche les tickets du technicien
    printf("Tickets assignés:\n");
    for (int i = 0; i < shared_mem->ticket_count; i++)
    {
        if (shared_mem->tickets[i].technician_id == technicien_id)
        {
            Ticket *t = &shared_mem->tickets[i];
            printf("ID: %d | Titre: %s | Statut: %s\n", t->id, t->title,
                   (t->status == TICKET_OPEN)
                       ? "Ouvert"
                   : (t->status == TICKET_IN_PROGRESS) ? "En cours"
                                                       : "Fermé");
            found = 1;
        }
    }

    // Aucun tickets trouvés
    if (!found)
    {
        printf("Aucun ticket trouvé.\n");
    }
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