FROM gcc:15.2
RUN apt-get update && apt-get install -y cmake iproute2

WORKDIR /app

COPY CMakeLists.txt .
COPY src ./src
COPY scripts/ ./scripts

RUN mkdir build && cd build && cmake .. && make
RUN chmod +x /app/scripts/entrypoint.sh

ENTRYPOINT ["/app/scripts/entrypoint.sh", "/app/build/src/core"]
