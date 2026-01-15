FROM gcc:15.2
RUN apt-get update && apt-get install -y cmake

WORKDIR /app

COPY CMakeLists.txt .
COPY src ./src

RUN mkdir build && cd build && cmake .. && make
ENTRYPOINT ["/app/build/src/main"]
