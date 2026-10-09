#ifndef SIM_UDP_H
#define SIM_UDP_H

#include "knx_ip.h"

typedef struct {
    int fd;
} sim_udp_t;

/* UDP-Socket (connect()-ed) zum KNXnet/IP-Gateway oeffnen. host als IPv4-Adresse oder Name. */
bool sim_udp_open(sim_udp_t *u, const char *host, uint16_t port);
knx_transport_t sim_udp_transport(sim_udp_t *u);

#endif
