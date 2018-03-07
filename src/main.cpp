
#include <winsock2.h>

#include <stdio.h>

#include <libsbp/sbp.h>
#include <libsbp/system.h>
#include <libsbp/piksi.h>

#define PORT 55555  //The port on which to listen for incoming data
#define SERVER "192.168.0.222"

static sbp_msg_callbacks_node_t heartbeat_callback_node;
static sbp_msg_callbacks_node_t monitor_callback_node;

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

int sock;

u32 piksi_port_read(u8 *buff, u32 n, void *context)
{
    (void)context;
    u32 result;

    result = (u32) recv((SOCKET) sock, (char *) buff, n, 0);

    return result;
}

int main(int argc, char **argv)
{
    sbp_state_t s{}; // SBP State object
    struct sockaddr_in serveraddr{}; // Destination information (ip and port)
    struct hostent *server;
    WSADATA wsa{};
    int sbp_result;

    /* Initialize Winsock version 2.2 */
    printf("Initializing WinSock...\n");
    if(WSAStartup(MAKEWORD(2, 2), &wsa) != NO_ERROR){
        printf("Failed. Error code: %d", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    printf("Initialized\n");

    /* Initialize TCP socket */
    if((sock = (int) socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET){
        fprintf(stderr, "Could not create socket: %d", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    printf("Socket created\n");

    /* Get the server's DNS entry */
    server = gethostbyname(SERVER);
    if (server == nullptr) {
        fprintf(stderr, "ERROR, no such host\n");
        exit(EXIT_FAILURE);
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy(server->h_addr, (char *)&serveraddr.sin_addr.s_addr, (size_t) server->h_length);
    serveraddr.sin_port = htons(PORT);

    /* Connect to server via TCP */
    if(connect((SOCKET) sock, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0){
        fprintf(stderr, "Failed to connect: %d", WSAGetLastError());
        exit(EXIT_FAILURE);
    }

    /* Initialize SBP */
    sbp_state_init(&s);
    sbp_register_callback(&s, SBP_MSG_HEARTBEAT, &heartbeat_callback, nullptr, &heartbeat_callback_node);
    sbp_register_callback(&s, SBP_MSG_DEVICE_MONITOR, &monitor_callback, nullptr, &monitor_callback_node);

    /* Read data stream */
    do{
        sbp_result = sbp_process(&s, &piksi_port_read);
    }
    while(sbp_result >= 0);

    /* Finish off the program */
    closesocket((SOCKET) sock);
    WSACleanup();

    return 0;
}
