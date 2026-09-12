# Build stage
FROM alpine:3.20 AS builder

RUN apk add --no-cache g++ make

WORKDIR /src
COPY include/ include/
COPY src/ src/
COPY makefile .

# Build statically linked and stripped binary
RUN g++ -std=c++17 -Wall -Wextra -O2 -static -s \
    -I include -I include/kvllay \
    src/main.cpp -o /kvllay -pthread

# Ultra-minimal final image (< 2 MB)
FROM scratch

COPY --from=builder /kvllay /kvllay

EXPOSE 6379

ENTRYPOINT ["/kvllay"]
CMD ["-p", "6379", "-h", "0.0.0.0"]
