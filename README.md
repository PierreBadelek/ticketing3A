# Projet Ticketing SAV

Ce projet en C a pour but de gérer un système de ticketing pour un service après-vente (SAV).

# Docker

Pour lancer le projet en utilisant Docker, suivez les étapes ci-dessous :

Récupérer l'image Docker depuis Docker Hub:
```bash
docker pull anthorld/projet-systeme:latest
```

Lancer le serveur sur un autre terminal:
```bash
docker run --rm -it --name serveur --ipc=shareable anthorld/projet-systeme:latest
```

Lancer le client sur un terminal:
```bash
docker run --rm -it --name client --ipc=container:serveur anthorld/projet-systeme:latest ./client
```


