
#ifdef _WIN32
    /* See http://stackoverflow.com/questions/12765743/getaddrinfo-on-win32 */
    #ifndef _WIN32_WINNT
        #define _WIN32_WINNT 0x0501  /* Windows XP. */
    #endif
    #include <winsock2.h>
    #include <Ws2tcpip.h>
    #include <conio.h>
#else
    /* Assume that any non-Windows platform uses POSIX-style sockets instead. */
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netdb.h>  /* Needed for getaddrinfo() and freeaddrinfo() */
    #include <unistd.h> /* Needed for close() */
    #include <errno.h>
    //#include <curses.h>
    #include <stdlib.h>
    #include <string.h>
    #define h_addr h_addr_list[0]
#endif

#include <stdio.h>

#include <libsbp/sbp.h>
#include <libsbp/system.h>
#include <libsbp/piksi.h>

#ifndef _WIN32
    typedef int SOCKET;
    extern int errno;

    int _kbhit(void)
    {
    /*
        int ch = getch();

        if (ch != ERR) {
            ungetch(ch);
            return 1;
        } else {
            return 0;
        }
       */
    }
#endif

#define bcopy(b1,b2,len) (memmove((b2), (b1), (len)), (void) 0)
#define bzero(b,len) (memset((b), '\0', (len)), (void) 0)

#define PORT 55555  //The port on which to listen for incoming data
#define SERVER "192.168.0.222"

static sbp_msg_callbacks_node_t heartbeat_callback_node;
static sbp_msg_callbacks_node_t monitor_callback_node;
static SOCKET sock;

int sockInit(void)
{
#ifdef _WIN32
    WSADATA wsa_data;
    return WSAStartup(MAKEWORD(1,1), &wsa_data);
#else
    return 0;
#endif
}

int sockClose(SOCKET sock)
{
    int status = 0;
#ifdef _WIN32
    status = shutdown(sock, SD_BOTH);
    if (status == 0) { status = closesocket(sock); }
#else
    status = shutdown(sock, SHUT_RDWR);
    if (status == 0) { status = close(sock); }
#endif
    return status;
}

int sockQuit(void)
{
#ifdef _WIN32
    return WSACleanup();
#else
    return 0;
#endif
}

void exitError(char *name){
#ifdef _WIN32
    fprintf(stderr, "%s: error %d", name, WSAGetLastError());
#else
    fprintf(stderr, "%s: error %d", name, errno);
#endif
    exit(EXIT_FAILURE);
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

    result = (u32) recv(sock, (char *) buff, n, 0);

    return result;
}

int main(int argc, char **argv)
{
    sbp_state_t s; // SBP State object
    struct sockaddr_in serveraddr; // Destination information (ip and port)
    struct hostent *server;
    int sbp_result;

    /* Initialize socket */
    printf("Initializing socket...\n");
    sockInit();
    printf("Initialized\n");

    /* Initialize TCP socket */
    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        exitError("Socket create");
    }
    printf("Socket created\n");

    /* Get the server's DNS entry */
    server = gethostbyname(SERVER);
    if (server == NULL) {
        exitError("Get Hostname");
    }

    /* build the server's Internet address */
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy(server->h_addr, (char *)&serveraddr.sin_addr.s_addr, (size_t) server->h_length);
    serveraddr.sin_port = htons(PORT);

    /* Connect to server via TCP */
    if(connect(sock, (struct sockaddr *) &serveraddr, sizeof(serveraddr)) < 0){
        exitError("Failed to connect");
    }

    /* Initialize SBP */
    sbp_state_init(&s);
    sbp_register_callback(&s, SBP_MSG_HEARTBEAT, &heartbeat_callback, NULL, &heartbeat_callback_node);
    sbp_register_callback(&s, SBP_MSG_DEVICE_MONITOR, &monitor_callback, NULL, &monitor_callback_node);

    /* Read data stream */
    do{
        sbp_result = sbp_process(&s, &piksi_port_read);
    }
    while(sbp_result >= 0 && _kbhit() == 0); // _kbhit returns > 0 if a key has been entered, this lets us exit the loop gracefully.

    /* Finish off the program */
    sockClose(sock);
    sockQuit();

    return 0;
}
