
#include <winsock2.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <libsbp/sbp.h>
#include <libsbp/system.h>
#include <libsbp/piksi.h>
#include <w32api/inaddr.h>

#define BUFLEN 512  //Max length of buffer
#define PORT 55557  //The port on which to listen for incoming data
#define SERVER "192.168.0.222"

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

    //result = comRead(piksi_port, (char *) buff, n);

    return result;
}

int main(int argc, char **argv)
{
    sbp_state_t s;

    struct sockaddr_in si_other;
    SOCKET sock;
    int slen;
    char buf[BUFLEN];
    char message[BUFLEN];
    WSADATA wsa;

    printf("Initializing WinSock...\n");
    if(WSAStartup(MAKEWORD(2, 2), &wsa) != NO_ERROR){
        printf("Failed. Error code: %d", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    printf("Initialized\n");

    if((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)) == INVALID_SOCKET){
        printf("Could not create socket: %d", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    printf("Socket created\n");

    /*if(bind(sock, (struct sockaddr *) &si_other, sizeof(si_other)) == SOCKET_ERROR){
        printf("Failed to bind: %d", WSAGetLastError());
        exit(EXIT_FAILURE);
    }*/

    memset((char *) &si_other, 0, slen);

    si_other.sin_family = AF_INET;
    //si_other.sin_addr.s_addr = htonl(INADDR_ANY);
    si_other.sin_addr.S_un.S_addr = inet_addr(SERVER);
    si_other.sin_port = htons(PORT);

    sbp_state_init(&s);

    sbp_register_callback(&s, SBP_MSG_HEARTBEAT, &heartbeat_callback, NULL,
                          &heartbeat_callback_node);

    sbp_register_callback(&s, SBP_MSG_DEVICE_MONITOR, &monitor_callback, NULL, &monitor_callback_node);


    int result;
    result = bind(sock, (SOCKADDR *) &si_other, sizeof(si_other));
    if(result != 0){
        printf("Bind failed with error %d", WSAGetLastError());
        exit(EXIT_FAILURE);
    }


        //for(int i = 0; i < 10; i++){
    while(1) {
        /*printf("Enter message:\n");
        gets(message);

        if(sendto(sock, message, strlen(message), 0, (struct sockaddr *) &si_other, slen) == SOCKET_ERROR){
            printf("sendto() failed with error code: %d", WSAGetLastError());
            exit(EXIT_FAILURE);
        }*/

        memset(buf, '\0', BUFLEN);

        slen = sizeof(si_other);

        result = recvfrom(sock, buf, BUFLEN, 0, (SOCKADDR*) &si_other, &slen);
        if(result == SOCKET_ERROR){
            printf("recvfrom() failed with error code: %d", WSAGetLastError());
            printf("Result = %d", result);
            exit(EXIT_FAILURE);
        }
        printf("Result: %d", result);

        puts(buf);
    }

    closesocket(sock);
    WSACleanup();

    return 0;
}
