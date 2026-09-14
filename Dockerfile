# Build stage
FROM alpine:3.20 AS builder

RUN apk add --no-cache g++ make

WORKDIR /src
COPY include/ include/
COPY src/ src/
COPY makefile .

# Build statically linked and stripped binary
RUN g++ -std=c++17 -Wall -Wextra -O3 -flto -static -s \
    -I include -I include/kvllay \
    src/main.cpp -o /kvllay -pthread

# Ultra-minimal final image (< 2 MB)
FROM scratch

LABEL org.opencontainers.image.title="kvllay" \
      org.opencontainers.image.description="Lightweight in-memory key-value database written in C++17 compatible with Redis RESP2" \
      org.opencontainers.image.authors="kenyka" \
      org.opencontainers.image.url="https://thekeny.github.io/kvllay" \
      org.opencontainers.image.source="https://github.com/thekeny/kvllay" \
      org.opencontainers.image.documentation="https://github.com/thekeny/kvllay#readme" \
      org.opencontainers.image.licenses="MIT"

COPY --from=builder /kvllay /kvllay

EXPOSE 6379

ENTRYPOINT ["/kvllay"]
CMD ["-p", "6379", "-h", "0.0.0.0"]
