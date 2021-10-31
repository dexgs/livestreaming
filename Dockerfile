# syntax=docker/dockerfile:1
FROM alpine:3.14
RUN apk add g++ make cmake openssl openssl-dev musl-dev linux-headers
WORKDIR /ShaRT
COPY . .
ARG CXX=g++
RUN make all
USER nobody
CMD ["bin/ShaRT"]
