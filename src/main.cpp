#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rs232.h>

#include <libsbp/sbp.h>
#include <libsbp/system.h>

char *serial_port_name = NULL;
//struct sp_port *piksi_port = NULL;
int piksi_port = -1;
static sbp_msg_callbacks_node_t heartbeat_callback_node;

void usage(char *prog_name) {
    fprintf(stderr, "usage: %s [-p serial port] [-l]\n", prog_name);
}

void setup_port()
{
    int result;

    printf("Attempting to configure the serial port...\n");

    /*
    result = sp_set_baudrate(piksi_port, 115200);
    if (result != SP_OK) {
        fprintf(stderr, "Cannot set port baud rate!\n");
        exit(EXIT_FAILURE);
    }
    printf("Configured the baud rate...\n");

    result = sp_set_flowcontrol(piksi_port, SP_FLOWCONTROL_NONE);
    if (result != SP_OK) {
        fprintf(stderr, "Cannot set flow control!\n");
        exit(EXIT_FAILURE);
    }
    printf("Configured the flow control...\n");

    result = sp_set_bits(piksi_port, 8);
    if (result != SP_OK) {
        fprintf(stderr, "Cannot set data bits!\n");
        exit(EXIT_FAILURE);
    }
    printf("Configured the number of data bits...\n");

    result = sp_set_parity(piksi_port, SP_PARITY_NONE);
    if (result != SP_OK) {
        fprintf(stderr, "Cannot set parity!\n");
        exit(EXIT_FAILURE);
    }

    printf("Configured the parity...\n");

    result = sp_set_stopbits(piksi_port, 1);
    if (result != SP_OK) {
        fprintf(stderr, "Cannot set stop bits!\n");
        exit(EXIT_FAILURE);
    }
    */

    printf("Configured the number of stop bits... done.\n");
}

void heartbeat_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
    (void)sender_id, (void)len, (void)msg, (void)context;
    fprintf(stdout, "%s\n", __FUNCTION__);
}

u32 piksi_port_read(u8 *buff, u32 n, void *context)
{
    (void)context;
    u32 result;

    result = comRead(piksi_port, (char *) buff, n);

    return result;
}

int main(int argc, char **argv)
{
    int opt;
    int result = 0;
    int numPorts;

    sbp_state_t s;

    if (argc <= 1) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Discover COM devices
    comEnumerate();

    while ((opt = getopt(argc, argv, "plh:")) != -1) {
        switch (opt) {
            case 'p':
                serial_port_name = (char *)calloc(strlen(optarg) + 1, sizeof(char));
                if (!serial_port_name) {
                    fprintf(stderr, "Cannot allocate memory!\n");
                    exit(EXIT_FAILURE);
                }
                strcpy(serial_port_name, optarg);
                break;
            case 'l':
                numPorts = comGetNoPorts();
                fprintf(stdout, "Printing available COM ports (%i):\n", numPorts);
                for(int i = 0; i < numPorts; i++){
                    fprintf(stdout, "Port %i: %s (%s)\n", i, comGetInternalName(i), comGetPortName(i));
                }
                exit(EXIT_SUCCESS);
            case 'h':
            default:
                usage(argv[0]);
                exit(EXIT_FAILURE);
        }
    }

    if (!serial_port_name) {
        fprintf(stderr, "Please supply the serial port path where the Piksi is " \
                    "connected!\n");
        exit(EXIT_FAILURE);
    }

    piksi_port = comFindPort(serial_port_name);
    if(piksi_port == -1){
        fprintf(stderr, "Cannot find provided serial port!\n");
        exit(EXIT_FAILURE);
    }
    /*
    result = sp_get_port_by_name(serial_port_name, &piksi_port);
    if (result != SP_OK) {
        fprintf(stderr, "Cannot find provided serial port!\n");
        exit(EXIT_FAILURE);
    }
     */

    printf("Attempting to open the serial port...\n");

    result = comOpen(piksi_port, 115200);
    if(result != 1){
        fprintf(stderr, "Cannot open %s for reading!\n", serial_port_name);
        exit(EXIT_FAILURE);
    }
    /*
    result = sp_open(piksi_port, SP_MODE_READ);
    if (result != SP_OK) {
        fprintf(stderr, "Cannot open %s for reading!\n", serial_port_name);
        exit(EXIT_FAILURE);
    }
     */

    //setup_port();

    sbp_state_init(&s);

    sbp_register_callback(&s, SBP_MSG_HEARTBEAT, &heartbeat_callback, NULL,
                          &heartbeat_callback_node);

    while(1) {
        sbp_process(&s, &piksi_port_read);
    }

    comClose(piksi_port);

    free(serial_port_name);

    return 0;
}
