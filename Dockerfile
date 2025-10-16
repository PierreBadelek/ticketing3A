FROM gcc:latest

RUN apt-get update && \
    apt-get install -y cmake && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /app
COPY . .

RUN rm -rf build && \
    mkdir build

RUN cmake -B build -DCMAKE_BUILD_TYPE=Release && \
    cmake --build build

CMD ["./build/server"]