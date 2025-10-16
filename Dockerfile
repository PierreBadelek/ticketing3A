# Build stage
FROM gcc:latest AS builder

RUN apt-get update && \
    apt-get install -y cmake && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN rm -rf build && mkdir build

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build

# Runtime stage
FROM debian:bookworm-slim

# Install runtime dependencies (C++ standard library, etc.)
RUN apt-get update && \
    apt-get install -y libstdc++6 && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app

# Copy only the compiled binary
COPY --from=builder /app/build/server .
COPY --from=builder /app/build/client .

CMD ["./server"]