# Etape de build
FROM gcc:latest AS builder

RUN apt-get update && \
    apt-get install -y cmake && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN mkdir build

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build

# Etape finale
FROM debian:bookworm-slim

RUN apt-get update && \
    apt-get install -y libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copie les fichier binaires depuis l'étape de construction
COPY --from=builder /app/build/server .
COPY --from=builder /app/build/client .
COPY --from=builder /app/build/technicien .

CMD ["./server"]