#include "sim_udp.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

bool sim_udp_open(sim_udp_t *u, const char *host, uint16_t port)
{
    struct addrinfo hints = { .ai_family = AF_INET, .ai_socktype = SOCK_DGRAM };
    struct addrinfo *res = NULL;
    char ps[8];
    snprintf(ps, sizeof(ps), "%u", port);
    if (getaddrinfo(host, ps, &hints, &res) != 0 || !res) {
        fprintf(stderr, "Gateway '%s' nicht aufloesbar\n", host);
        return false;
    }
    u->fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (u->fd < 0 || connect(u->fd, res->ai_addr, res->ai_addrlen) != 0) {
        perror("udp");
        freeaddrinfo(res);
        return false;
    }
    freeaddrinfo(res);
    fcntl(u->fd, F_SETFL, fcntl(u->fd, F_GETFL, 0) | O_NONBLOCK);
    return true;
}

static bool udp_send(void *ctx, const uint8_t *b, size_t l)
{
    sim_udp_t *u = ctx;
    return send(u->fd, b, l, 0) == (ssize_t)l;
}

static int udp_recv(void *ctx, uint8_t *b, size_t cap)
{
    sim_udp_t *u = ctx;
    ssize_t n = recv(u->fd, b, cap, 0);
    return n > 0 ? (int)n : 0; /* EAGAIN / ECONNREFUSED -> nichts */
}

knx_transport_t sim_udp_transport(sim_udp_t *u)
{
    knx_transport_t t = { u, udp_send, udp_recv };
    return t;
}
