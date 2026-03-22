FROM alpine:latest

RUN apk add --no-cache \
    build-base \
    cmake

WORKDIR /first_and_follow

COPY --chown=1000:1000 . .

RUN test -f /first_and_follow/validator

RUN chmod +x /first_and_follow/validator

ENTRYPOINT ["/first_and_follow/validator"]
