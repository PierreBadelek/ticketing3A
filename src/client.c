#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "ticketing.h"

SharedMemory *shared_mem;
int client_id;

// Déclarations des fonctions
int connect_to_server();
void process_client_input();
void create_ticket(const char *titre, const char *description, const char *date_str);
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

    client_id = getpid();

    printf("Connecté au serveur avec Client ID: %d\n", client_id);
    return 0;
}

void process_client_input()
{
    // lit les commandes du client et les traite
    char command[256];
    while (1)
    {
        printf("Entrez une commande (sendTicket -l | sendTicket -new | exit): ");
        if (fgets(command, sizeof(command), stdin) == NULL)
        {
            printf("Erreur de lecture de la commande.\n");
            continue;
        }

        command[strcspn(command, "\n")] = 0;

        if (strcmp(command, "sendTicket") == 0)
        {
            printf("Options disponibles:\n");
            printf("  sendTicket -l    : Lister vos tickets\n");
            printf("  sendTicket -new  : Créer un nouveau ticket\n");
        }
        else if (strcmp(command, "sendTicket -l") == 0)
        {
            list_tickets();
        }
        else if (strcmp(command, "sendTicket -new") == 0)
        {
            char title[100];
            char description[500];
            time_t date = time(NULL);

            printf("Entrez le titre du ticket: ");
            if (fgets(title, sizeof(title), stdin) == NULL)
            {
                printf("Erreur de lecture du titre.\n");
                continue;
            }
            title[strcspn(title, "\n")] = 0;

            printf("Entrez la description du ticket: ");
            if (fgets(description, sizeof(description), stdin) == NULL)
            {
                printf("Erreur de lecture de la description.\n");
                continue;
            }
        
            description[strcspn(description, "\n")] = 0;

            char date_str[20];
            printf("Entrez la date de l'événement (YYYY-MM-DD): ");
            if (fgets(date_str, sizeof(date_str), stdin) == NULL)
            {
                printf("Erreur de lecture de la date.\n");
                continue;
            }
            date_str[strcspn(date_str, "\n")] = 0;

            create_ticket(title, description, date_str);
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

// Créé un nouveau ticket
void create_ticket(const char *titre, const char *description, const char *date_str)
{
    // Vérifie si le nombre maximum de tickets est atteint
    pthread_mutex_lock(&shared_mem->mutex);
    if (shared_mem->ticket_count >= MAX_TICKETS)
    {
        printf("Erreur: Nombre maximum de tickets atteint\n");
        pthread_mutex_unlock(&shared_mem->mutex);
        return;
    }

    // Création du ticket
    Ticket *new_ticket = &shared_mem->tickets[shared_mem->ticket_count];
    new_ticket->id = shared_mem->next_id++;
    new_ticket->status = TICKET_OPEN;
    new_ticket->client_id = client_id;

    strncpy(new_ticket->title, titre, sizeof(new_ticket->title) - 1);
    new_ticket->title[sizeof(new_ticket->title) - 1] = '\0';
    strncpy(new_ticket->description, description, sizeof(new_ticket->description) - 1);
    new_ticket->description[sizeof(new_ticket->description) - 1] = '\0';

    new_ticket->technician_id = -1;
    new_ticket->is_priority = 0;
    new_ticket->created_at = time(NULL);

    strncpy(new_ticket->date_event, date_str, sizeof(new_ticket->date_event) - 1);
    new_ticket->date_event[sizeof(new_ticket->date_event) - 1] = '\0';

    new_ticket->updated_at = new_ticket->created_at;
    shared_mem->ticket_count++;

    printf("Ticket n°%d créé avec succès\n", new_ticket->id);
    printf("Un technicien prendra en charge votre demande sous peu.\n");
    pthread_mutex_unlock(&shared_mem->mutex);
}

// Liste tous les tickets du client
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
            printf("ID: %d | Titre: %s | Statut: %s | Date de l'événement: %s\n", t->id, t->title,
                   (t->status == TICKET_OPEN)
                       ? "Ouvert"
                   : (t->status == TICKET_IN_PROGRESS) ? "En cours"
                                                       : "Fermé", t->date_event);
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

int main()
{
    if (connect_to_server() == -1)
    {
        printf("Erreur: La connexion au serveur a échouée\n");
        return 1;
    }

    process_client_input();

    // Nettoyage
    munmap(shared_mem, sizeof(SharedMemory));
    return 0;
}