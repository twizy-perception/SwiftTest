#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <rs232.h>

#include <libsbp/sbp.h>
#include <libsbp/system.h>
#include <libsbp/piksi.h>

char *serial_port_name = NULL;
int piksi_port = -1;

static sbp_msg_callbacks_node_t heartbeat_callback_node;
static sbp_msg_callbacks_node_t monitor_callback_node;

void usage(char *prog_name) {
    fprintf(stderr, "usage: %s [-p serial port] [-l]\n", prog_name);
}

void printByteMessage(char *name, u8 len, u8 msg[])
{
    printf("%s: ", name);
    for(int i = 0; i < len; i++){
        printf("0x%02X ", msg[i]);
    }
    printf("\n");
}

// SBP uses little-endian byte order

s32 bytesToSignedInt(u8 start, u8 msg[]){
    return msg[start] | msg[start + 1] << 8 | msg[start + 2] << 16 | msg[start + 3] << 24;
}

u32 bytesToUnsignedInt(u8 start, u8 msg[]){
    return (u32) bytesToSignedInt(start, msg);
}

s16 bytesToSignedShort(u8 start, u8 msg[]){
    return msg[start] | msg[start + 1] << 8;
}

u16 bytesToUnsignedShort(u8 start, u8 msg[]){
    return (u16) bytesToSignedShort(start, msg);
}

const u32 HEARTBEAT_SYSTEMERROR = 0x00000001;
const u32 HEARTBEAT_IOERROR = 0x00000002;
const u32 HEARTBEAT_SWIFTNAPERROR = 0x00000004;
const u32 HEARTBEAT_ANTENNASHORTERROR = 0x40000000;
const u32 HEARTBEAT_ANTENNAPRESENT = 0x80000000;
void heartbeat_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
    (void)sender_id, (void)len, (void)msg, (void)context;
    printByteMessage("heartbeat", len, msg);

    u32 flags = bytesToUnsignedInt(0, msg);

    printf("System Error: %s\n", (flags & HEARTBEAT_SYSTEMERROR) > 0 ? "yes" : "no");
    printf("IO Error: %s\n", (flags & HEARTBEAT_IOERROR) > 0 ? "yes" : "no");
    printf("SwiftNAP Error: %s\n", (flags & HEARTBEAT_SWIFTNAPERROR) > 0 ? "yes" : "no");
    printf("Antenna Short Error: %s\n", (flags & HEARTBEAT_ANTENNASHORTERROR) > 0 ? "yes" : "no");
    printf("Antenna Present: %s\n", (flags & HEARTBEAT_ANTENNAPRESENT) > 0 ? "yes" : "no");

}

void monitor_callback(u16 sender_id, u8 len, u8 msg[], void *context)
{
    (void)sender_id, (void)len, (void)msg, (void)context;
    printByteMessage("monitor", len, msg);

    printf("Vin: %.2fV\n", bytesToSignedShort(0, msg) / 1000.0);
    printf("CPU Vin: %.2fV\n", bytesToSignedShort(2, msg) / 1000.0);
    printf("CPU Vaux: %.2fV\n", bytesToSignedShort(4, msg) / 1000.0);
    printf("CPU Temp: %.2fC\n", bytesToSignedShort(6, msg) / 100.0);
    printf("Front Temp: %.2fC\n", bytesToSignedShort(8, msg) / 100.0);
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

    char *optarg;

    sbp_state_t s;

    if (argc <= 1) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    // Discover COM devices
    comEnumerate();
/*
    while ((opt = getopt(argc, argv, "p:")) != -1) {
        printf("%c: %s\n", opt, optarg);
        switch (opt) {
            case 'p':
                serial_port_name = (char *)calloc(strlen(optarg) + 1, sizeof(char));
                printf(serial_port_name);
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
*/
    serial_port_name = "COM3";

    if (!serial_port_name) {
        fprintf(stderr, "Please supply the serial port path where the Piksi is " \
                    "connected!\n");
        exit(EXIT_FAILURE);
    }

    printf("Port: %s\n", serial_port_name);

    piksi_port = comFindPort(serial_port_name);
    if(piksi_port == -1){
        fprintf(stderr, "Cannot find provided serial port!\n");
        exit(EXIT_FAILURE);
    }

    printf("Attempting to open the serial port...\n");

    result = comOpen(piksi_port, 115200);
    if(result != 1){
        fprintf(stderr, "Cannot open %s for reading! Result: %i\n", serial_port_name, result);
        exit(EXIT_FAILURE);
    }

    printf("Serial port opened!\n");

    sbp_state_init(&s);

    sbp_register_callback(&s, SBP_MSG_HEARTBEAT, &heartbeat_callback, NULL,
                          &heartbeat_callback_node);

    sbp_register_callback(&s, SBP_MSG_DEVICE_MONITOR, &monitor_callback, NULL, &monitor_callback_node);

        //for(int i = 0; i < 10; i++){
    while(1) {
        sbp_process(&s, &piksi_port_read);
    }

    comClose(piksi_port);

    free(serial_port_name);

    return 0;
}
