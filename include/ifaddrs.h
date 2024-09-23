
#define udp_sendto(sock, data, size,dst) juice_udp_sendto(sock, data, size,dst)

// TODO:
#define NI_NUMERICHOST AI_NUMERICHOST
#define IFF_LOOPBACK 1
#define IFF_UP 2


struct ifaddrs {
    struct ifaddrs  *ifa_next;    /* Next item in list */
    char            *ifa_name;    /* Name of interface */
    struct sockaddr *ifa_addr;    /* Address of interface */
    int ifa_flags;
};

int getifaddrs(struct ifaddrs **ifap);
void freeifaddrs(struct ifaddrs *ifa);