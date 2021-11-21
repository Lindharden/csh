
#include "iflist_yaml.h"

#include <csp/csp_interface.h>
#include <csp/csp_iflist.h>
#include <csp/csp_rtable.h>
#include <csp/interfaces/csp_if_zmqhub.h>

#include <assert.h>
#include <stdio.h>
#include <yaml.h>

struct {
    char *name;
    char *driver;
    char *device;
    char *addr;
    char *netmask;
    char *server;
    char *is_dfl;
} data;

static void iflist_yaml_start_if(void) {
    memset(&data, 0, sizeof(data));
}

static void iflist_yaml_end_if(void) {

    /* Sanity checks */
    if ((!data.name) || (!data.driver) || (!data.addr)) {
        printf("interface is missing, name, driver or addr\n");
        return;
    }

    csp_iface_t * iface;

    /* ZMQ */
    if (strcmp(data.driver, "zmq") == 0) {
        printf("Adding ZMQ interface: %s node %s\n", data.name, data.addr);

        /* Check for valid server */
        if (!data.server) {
            printf("No server: configured\n");
            return;
        }

		printf("server: %s\n", data.server);
		
		csp_zmqhub_init(atoi(data.addr), data.server, 0, &iface);
        iface->addr = atoi(data.addr);
        iface->netmask = atoi(data.netmask);

        strncpy(iface->name, data.name, CSP_IFLIST_NAME_MAX);
		
        csp_iflist_add(iface);

        
    }

    if (iface && data.is_dfl) {
        printf("Setting default route to %s\n", iface->name);
        csp_rtable_set(0, 0, iface, CSP_NO_VIA_ADDRESS);
    }


}

static void iflist_yaml_key_value(char * key, char * value) {
    //printf("%s : %s\n", key, value);

    if (strcmp(key, "name") == 0) {
        data.name = value;
    } else if (strcmp(key, "driver") == 0) {
        data.driver = value;
    } else if (strcmp(key, "device") == 0) {
        data.device = value;
    } else if (strcmp(key, "addr") == 0) {
        data.addr = value;
    } else if (strcmp(key, "netmask") == 0) {
        data.netmask = value;
    } else if (strcmp(key, "server") == 0) {
        data.server = value;
    } else if (strcmp(key, "default") == 0) {
        data.is_dfl = value;
    } else {
        printf("Unkown key %s\n", key);
    }    

}


void iflist_yaml_init(void) {

    FILE * file = fopen("iflist.yaml", "rb");
    if (file == NULL)
        return;

    yaml_parser_t parser;
    yaml_parser_initialize(&parser);
    yaml_parser_set_input_file(&parser, file);
    yaml_event_t event;

    assert(yaml_parser_parse(&parser, &event));
    if (event.type != YAML_STREAM_START_EVENT)
        return;

    assert(yaml_parser_parse(&parser, &event));
    if (event.type != YAML_DOCUMENT_START_EVENT)
        return;

    assert(yaml_parser_parse(&parser, &event));
    if (event.type != YAML_SEQUENCE_START_EVENT)
        return;

    while(1) {

        assert(yaml_parser_parse(&parser, &event));

        //printf("Event type %d\n", event.type);

        if (event.type == YAML_SEQUENCE_END_EVENT)
            break;

        if (event.type == YAML_MAPPING_START_EVENT) {
            iflist_yaml_start_if();
            continue;
        }

        if (event.type == YAML_MAPPING_END_EVENT) {
            iflist_yaml_end_if();
            continue;
        }
        
        if (event.type == YAML_SCALAR_EVENT) {
            yaml_char_t * key = event.data.scalar.value;

            yaml_char_t * value;
            yaml_parser_parse(&parser, &event);
            if (event.type != YAML_SCALAR_EVENT) {
                printf("Extected value\n");
                break;
            } else {
                value = event.data.scalar.value;
            }

            iflist_yaml_key_value((char *) key, (char *) value);
            continue;
        }
        
    }
    
}