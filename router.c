#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "ip.h"
#include "config.h"
#include "util.h"
#include "log.h"

#define MAX_INTERFACES 8
#define MAX_FORWARDING_ENTRIES 2000

typedef struct forwarding_entry {
    uint32_t network;
    uint32_t netmask;
    int interface;
    int netlength;
} forwarding_entry;

int interfaces[MAX_INTERFACES];
struct forwarding_entry forwarding_table[MAX_FORWARDING_ENTRIES];

int numInterfaces = 0;
int numForwardingEntries = 0;

char interfaceExists(int interface) {
    for (int i = 0; i < numInterfaces; i++) {
        if (interfaces[i] == interface) {
            return 1;
        }
    }
    return 0;
}

int getDestinationInterface(uint32_t ip) {
    printf("TRYING TO MATCH: %x\n", ip);
    int bestMatch = -1;
    int len = -1;

    for (int i = 0; i < numForwardingEntries; i++) {
        forwarding_entry entry = forwarding_table[i];

        if (entry.netlength == 0) {
            printf("DEFAULT INTERFACE: %d\n", entry.interface);
            if (entry.netlength > len) {
                len = entry.netlength;
                bestMatch = entry.interface;
            }
        }

        if ((entry.network & entry.netmask) == (ip & entry.netmask)) {
            printf("MATCHED INTERFACE: %d %d %d\n", entry.interface, entry.netlength, len);
            if (entry.netlength > len) {
                len = entry.netlength;
                bestMatch = entry.interface;
            }
        }
    }

    return bestMatch;
}

/*
 * Add a forwarding entry to your forwarding table.  The network address is
 * given by the network and netlength arguments.  Datagrams destined for
 * this network are to be sent out on the indicated interface.
 */
void addForwardEntry(uint32_t network, int netlength, int interface) {
    // TODO: Implement this
    uint32_t netmask = 0xFFFFFFFF << (32 - netlength);

    if (netlength > 32) {
        logLog("error", "Invalid netlength %d", netlength);
        return;
    }

    forwarding_entry entry = {network, netmask, interface, netlength};
    forwarding_table[numForwardingEntries] = entry;

    printf("[%d] - Network: %x|LEN: %d|Interface:%d|Netmask:%x \n", numForwardingEntries, network, netlength, interface, netmask);

    numForwardingEntries += 1;

}
/*
 * Add an interface to your router. 
 */
void addInterface(int interface) {
    // TODO: Implement this
    interfaces[numInterfaces] = interface;
    numInterfaces += 1;
}

void run() {
    int port = getDefaultPort();
    int fd = udp_open(port);
    // TODO: Implement this
    while (1) {
        packet pkt;
        int interface;

        // receive packet
        while (1) {
            readpkt(fd, &pkt, &interface);
            if (interface) {
                break;
            }
        }

        ipheader *header = (ipheader *) &pkt;
        unsigned short checksum = ipchecksum(header, sizeof(ipheader));

        if (checksum != 0) {
            printf("SKIPPING PACKET: Checksum is %x %x\n", checksum, header->checksum);
            continue;
        }

        if (header->ttl <= 1) {
            printf("SKIPPING PACKET: TTL is %d \n", header->ttl);
            continue;
        }

        header->ttl = header->ttl - 1;
        header->checksum = 0;
        header->checksum = ipchecksum(header, sizeof(ipheader));

        int fwinterface = getDestinationInterface(header->dstipaddr);

        if (fwinterface == interface){
            printf("DROPPING PACKAGE, SAME INTERFACE\n");
            continue;
        }

        // return packet
        if (interfaceExists(fwinterface) && fwinterface > 0) {
            printf("Sending packet to interface %d\n", interface);
            sendpkt(fd, fwinterface, &pkt);
        } else {
            logLog("failure", "Interface %d does not exist", fwinterface);
        }
    }

}

int main(int argc, char **argv) {
    printf("Router starting\n");
    logConfig("router", "packet,error,failure");

    char *configFileName = "router.config";
    if (argc > 1) {
	configFileName = argv[1];
    }
    configLoad(configFileName, addForwardEntry, addInterface);
    run();
    return 0;
}
