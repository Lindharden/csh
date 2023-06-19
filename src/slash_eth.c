/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Space Inventor ApS
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of
 * this software and associated documentation files (the "Software"), to deal in
 * the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
 * the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
 * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
 * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
 * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "csp_if_eth.h"
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netdb.h>
#include <ifaddrs.h>
#include <sys/ioctl.h>
#include <csp/csp.h>
#include <csp/csp_cmp.h>
#include <csp/csp_debug.h>
#include <csp_autoconfig.h>
#include <csp/csp_hooks.h>
#include <sys/types.h>
#include <unistd.h>
#include <linux/netdevice.h>
#include <slash/slash.h>
#include <slash/optparse.h>
#include <slash/dflopt.h>
#include "base16.h"
#include "known_hosts.h"

#define DEB printf("%s:%d\n", __FILE__, __LINE__);

slash_command_group(eth, "Ethernet");

extern bool eth_debug;

static int eth_debug_toggle(struct slash *slash)
{
    eth_debug = !eth_debug;
    printf("Ethernet debugginbg %s\n", eth_debug ? "ON" : "OFF");
    if (eth_debug) {
        csp_dbg_packet_print = 2;
    } else {
        csp_dbg_packet_print = 0;
    }
    return SLASH_SUCCESS;
}
slash_command_sub(eth, debug, eth_debug_toggle, "", "Toggle ethernet debugging");


int eth_init_check(char * device) {
    static int sockfd;
    struct ifreq if_idx;
    struct ifreq if_mac;

    /**
     * TX SOCKET
     */

    /* Open RAW socket to send on */
    /* See: */
	if ((sockfd = socket(AF_PACKET, SOCK_RAW, htons(ETH_P_ALL))) == -1) {
	    printf("socket error (not root?)\n");
        return -1;
	}

	/* Get the index of the interface to send on */
	memset(&if_idx, 0, sizeof(struct ifreq));
	strncpy(if_idx.ifr_name, device, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFINDEX, &if_idx) < 0) {
	    printf("SIOCGIFINDEX error (device '%s')\n", device);
        return -2;
    }

	/* Get the MAC address of the interface to send on */
	memset(&if_mac, 0, sizeof(struct ifreq));
	strncpy(if_mac.ifr_name, device, IFNAMSIZ-1);
	if (ioctl(sockfd, SIOCGIFHWADDR, &if_mac) < 0) {
	    printf("SIOCGIFHWADDR (device '%s')\n", device);
        return -3;
    }

	printf("ETH %s idx %d mac %02hhx:%02hhx:%02hhx:%02hhx:%02hhx:%02hhx\n", device, if_idx.ifr_ifindex, 
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[0],
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[1],
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[2],
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[3],
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[4],
        ((uint8_t *)&if_mac.ifr_hwaddr.sa_data)[5]);

    /* Allow the socket to be reused - incase connection is closed prematurely */
    int sockopt;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &sockopt, sizeof sockopt) == -1) {
		printf("setsockopt error (device '%s')\n", device);
		close(sockfd);
		return -4;
	}

	/* Bind to device */
	if (setsockopt(sockfd, SOL_SOCKET, SO_BINDTODEVICE, device, IFNAMSIZ-1) == -1)	{
		printf("SO_BINDTODEVICE error (device '%s')\n", device);
		close(sockfd);
		return -5;
	}

	/* fill sockaddr_ll struct to prepare binding */
	struct sockaddr_ll my_addr;
	my_addr.sll_family = AF_PACKET;
	my_addr.sll_protocol = htons(ETH_TYPE_CSP);
	my_addr.sll_ifindex = if_idx.ifr_ifindex;

	/* bind socket  */
	bind(sockfd, (struct sockaddr *)&my_addr, sizeof(struct sockaddr_ll));

    printf("sll fam:0x%x prot:0x%x idx:%d hatyp:0x%x pktyp:0x%x halen:%u addr:",
            my_addr.sll_family,
            my_addr.sll_protocol,
            my_addr.sll_ifindex,
            my_addr.sll_hatype,
            my_addr.sll_pkttype,
            my_addr.sll_halen);
    for (int i = 0; i < 8; i++) {
        printf(" %02x", my_addr.sll_addr[i]);
    }
    printf("\n");

    // Close socket
	close(sockfd);

    return 0;
}


static void eth_list_interfaces()
{
    // Create link of interface adresses
    struct ifaddrs *addresses;
    if (getifaddrs(&addresses) == -1)
    {
        printf("getifaddrs call failed\n");
	    return;
    }

    int i = 0;
    char ap[100] = {};
    struct ifaddrs *address = addresses;
    for( ; address; address = address->ifa_next)
    {
        if (address->ifa_addr) {

            i++;

            int family = address->ifa_addr->sa_family;

            printf("%d %p", i, address);
            printf(" Name: %s", address->ifa_name);
            printf(" Flags: 0x%x", address->ifa_flags);
            printf(" Family: 0x%x", family);

            size_t sockaddr_size = (family == AF_INET ? sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6));

            ap[0] = 0;
            getnameinfo(address->ifa_addr, sockaddr_size, 
                        ap, sizeof(ap), 0, 0,
                        NI_DGRAM | NI_NUMERICHOST);
            printf(" Addr: %s", ap);

            ap[0] = 0;
            getnameinfo(address->ifa_netmask, sockaddr_size, 
                        ap, sizeof(ap), 0, 0,
                        NI_DGRAM | NI_NUMERICHOST);
            printf(" Netmask: %s", ap);

            ap[0] = 0;
            getnameinfo(address->ifa_dstaddr, sockaddr_size, 
                        ap, sizeof(ap), 0, 0,
                        NI_DGRAM | NI_NUMERICHOST);
            printf(" \n");


            int ret = eth_init_check(address->ifa_name);
            if (ret < 0) {
                printf("ERROR %d\n", ret);
            }
            printf("----------------------------\n");
        }
    }

    freeifaddrs(addresses);
}

static int eth_info(struct slash *slash)
{
    eth_list_interfaces();
    return SLASH_SUCCESS;
}
slash_command_sub(eth, info, eth_info, "", "List devices");


static char selected_device[20];

static const char * eth_select_interface(const char * device)
{
    selected_device[0] = 0;

    // Create link of interface adresses
    struct ifaddrs *addresses;
    if (getifaddrs(&addresses) == -1)  {
        printf("eth_select_interface: getifaddrs call failed\n");
    } else {
        bool found = false;
        struct ifaddrs *address = addresses;
        for( ; address && !found; address = address->ifa_next) {
            if (address->ifa_addr) {
                printf("cmp:%d '%s' '%s'\n", strncmp(device, address->ifa_name, strlen(device)), device, address->ifa_name);
                if (strncmp(device, address->ifa_name, strlen(device)) == 0) {
                    strncpy(selected_device, address->ifa_name, sizeof(selected_device));
                    found = true;
                }
            }
        }
        freeifaddrs(addresses);
    }
    return (const char *)selected_device;
}

static int eth_select(struct slash *slash)
{
    char * device = "e";

    optparse_t * parser = optparse_new("eth test", "[node]\nTests ethernet");
    optparse_add_help(parser);
    optparse_add_string(parser, 'e', "device", "STR", &device, "Ethernet device name or name prefix (defaults to e)");

    int argi = optparse_parse(parser, slash->argc - 1, (const char **) slash->argv + 1);
    if (argi < 0) {
        optparse_del(parser);
	    return SLASH_EINVAL;
    }

    const char * selected = eth_select_interface(device);
    printf("SELECTED  '%s' -> '%s'\n", device, selected);

    optparse_del(parser);
    return SLASH_SUCCESS;
}
slash_command_sub(eth, select, eth_select, "", "Select device");