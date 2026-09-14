# Build stage
FROM alpine:3.20 AS builder

RUN apk add --no-cache g++ make

WORKDIR /src
COPY include/ include/
COPY src/ src/
COPY makefile .

ARG MALLOC=libc

# Build statically linked and stripped binary
RUN make static-linux MALLOC=${MALLOC} && cp build/kvllay-linux-x86_64 /kvllay

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
