// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

#define FLAG 0x7E

#define A_SENDER 0x03
#define A_RECEIVER 0x01

#define C_SET 0x03
#define C_UA 0x07
#define C_IFN0 0x00
#define C_IFN1 0x80
#define C_RR0 0xAA
#define C_RR1 0xAB
#define C_REJ0 0x54
#define C_REJ1 0x55

typedef enum {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP, ESCP_RCV} State;

int alarmEnabled = FALSE;
int alarmCount = 0;
static int Ns = 0;
static int expected_inframe = 0;
static int nRetransmissions, timeout;

// Alarm function handler.
// This function will run whenever the signal SIGALRM is received.
void alarmHandler(int signal)
{
    alarmEnabled = FALSE;
    alarmCount++;

    printf("Alarm #%d received\n", alarmCount);
}

////////////////////////////////////////////////
// LLOPEN
////////////////////////////////////////////////
int llOpenTx(LinkLayer llParameters)
{
    // ----------------------------------------------------
    // This example code shows how to open the serial port and send a string.
    // TODO: Adapt and extend this code according to the specifications of the project.
    // ----------------------------------------------------

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    struct sigaction act = {0};
    act.sa_handler = &alarmHandler;
    if (sigaction(SIGALRM, &act, NULL) == -1)
    {
        perror("sigaction");
        exit(1);
    }

    printf("Alarm configured\n");

    // preparar o set tal como pedido no enunciado
    unsigned char set[5] = {FLAG, A_SENDER, C_SET, A_SENDER ^ C_SET, FLAG};

    State state = START;
    unsigned char b;
    
    while(alarmCount < llParameters.nRetransmissions && state != STOP){
        state = START;

        int bytes = writeBytesSerialPort(set, 5);
        printf("%d bytes written to serial port\n", bytes);

        // dar enable ao alarm
        alarm(llParameters.timeout);
        alarmEnabled = TRUE;

        while(alarmEnabled && state != STOP){
            int r = readByteSerialPort(&b);
            
            if (r < 0) {
                if (errno == EINTR){
                } else{
                    return -1;
                }
            } else if (r != 0){
                switch (state){
                    case START:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        }
                        break;
                    case FLAG_RCV:
                        if (b != FLAG){
                            if (b == A_SENDER){
                                state = A_RCV;
                            } else {
                                state = START;
                            }
                        }
                        break;
                    case A_RCV:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        } else if (b == C_UA){
                            state = C_RCV;
                        } else{
                            state = START;
                        }
                        break;
                    case C_RCV:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        } else if (b == (A_SENDER ^ C_UA)){
                            state = BCC_OK;
                        } else{
                            state = START;
                        }
                        break;
                    case BCC_OK:
                        if (b == FLAG){
                            state = STOP;
                        } else {
                            state = START;
                        }
                        break;
                    default:
                        break;
                }
            }
        }
    }
        alarm(0);


    if (state != STOP){
        printf("no UA received. tried %d times\n", llParameters.nRetransmissions);
        closeSerialPort();
        return -1;
    }

    printf("UA received successfully\n");

    Ns = 0;
    nRetransmissions = llParameters.nRetransmissions;
    timeout = llParameters.timeout;

    return 0;
}

int llOpenRx(LinkLayer llParameters)
{
    // ----------------------------------------------------
    // This example code shows how to open the serial port and receive a string.
    // TODO: Adapt and extend this code according to the specifications of the project.
    // ----------------------------------------------------

    if (openSerialPort(llParameters.serialPort, llParameters.baudRate) < 0)
    {
        perror("openSerialPort");
        return -1;
    }

    printf("Serial port %s opened\n", llParameters.serialPort);

    State state = START;
    unsigned char b;
    
    while(state != STOP){

            int r = readByteSerialPort(&b);
            
            if (r < 0) {
                return -1;
            } else if (r != 0){
                switch (state){
                    case START:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        }
                        break;
                    case FLAG_RCV:
                        if (b != FLAG){
                            if (b == A_SENDER){
                                state = A_RCV;
                            } else {
                                state = START;
                            }
                        }
                        break;
                    case A_RCV:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        } else if (b == C_SET){
                            state = C_RCV;
                        } else{
                            state = START;
                        }
                        break;
                    case C_RCV:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        } else if (b == (A_SENDER ^ C_SET)){
                            state = BCC_OK;
                        } else{
                            state = START;
                        }
                        break;
                    case BCC_OK:
                        if (b == FLAG){
                            state = STOP;
                        } else {
                            state = START;
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        if (state == STOP){
            unsigned char ua[5] = {FLAG, A_SENDER, C_UA, A_SENDER ^ C_UA, FLAG};
            writeBytesSerialPort(ua, 5);
            printf("valid SET!\n");
        } else {
            printf("invalid SET!\n");
            closeSerialPort();
            return -1;
        }

    expected_inframe = 0;

    return 0;
}

////////////////////////////////////////////////
// LLSEND
////////////////////////////////////////////////
int llSend(const unsigned char *buf, int bufSize)
{
    unsigned char inframe[2048];
    alarmCount = 0;

    // usamos como índice. meto 3 porque começa em 0 e depois o 5º frame será o n=4
    int index = 3;

    // FLAG A C BCC D BCC2 FLAG
    inframe[0] = FLAG;
    inframe[1] = A_SENDER;
    if (Ns == 0){
        inframe[2] = C_IFN0;
        inframe[3] = A_SENDER ^ C_IFN0;
    } else{
        inframe[2] = C_IFN1;
        inframe[3] = A_SENDER ^ C_IFN1;
    }

    // percorremos todos os bytes do buffer de dados e metemos no information frame
    for (int i = 0; i < bufSize; i++){
        switch (buf[i]){
            case FLAG:
                inframe[++index] = 0x7D;
                inframe[++index] = 0x5E;
                break;
            case 0x7D:
                inframe[++index] = 0x7D;
                inframe[++index] = 0x5D;
                break;
            default:
                inframe[++index] = buf[i];
                break;
        }
    }

    unsigned char bcc_2 = 0;
    for (int i = 0; i < bufSize; i++){
        bcc_2 ^= buf[i];
    }

    switch (bcc_2){
            case 0x7E:
                inframe[++index] = 0x7D;
                inframe[++index] = 0x5E;
                break;
            case 0x7D:
                inframe[++index] = 0x7D;
                inframe[++index] = 0x5D;
                break;
            default:
                inframe[++index] = bcc_2;
                break;
        }

    inframe[++index] = FLAG;

    int bytes, stop = 0;
    State state = START;
    unsigned char b;
    while (alarmCount < nRetransmissions && stop == 0){
        state = START;
        bytes = writeBytesSerialPort(inframe, index + 1);

        alarm(timeout);
        alarmEnabled = TRUE;

        unsigned char c = 0;
        while(alarmEnabled && state != STOP){
            int r = readByteSerialPort(&b);
            
            if (r < 0) {
                if (errno == EINTR){
                } else{
                    return -1;
                }
            } else if (r != 0){
                switch (state){
                    case START:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        }
                        break;
                    case FLAG_RCV:
                        if (b != FLAG){
                            if (b == A_SENDER){
                                state = A_RCV;
                            } else {
                                state = START;
                            }
                        }
                        break;
                    case A_RCV:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        } else if (b == C_RR0 || b == C_RR1 || b == C_REJ0 || b == C_REJ1){
                            state = C_RCV;
                            c = b;
                        } else{
                            state = START;
                        }
                        break;
                    case C_RCV:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        } else if (b == (A_SENDER ^ c)){
                            state = BCC_OK;
                        } else{
                            state = START;
                        }
                        break;
                    case BCC_OK:
                        if (b == FLAG){
                            state = STOP;
                        } else {
                            state = START;
                        }
                        break;
                    default:
                        break;
                }
            }
        }

        alarm(0);

        if (state == STOP){
            if (c == C_RR0 || c == C_RR1){
                int expected_ns;
                switch (c){
                    case C_RR0 : expected_ns = 0; break;
                    case C_RR1 : expected_ns = 1; break;
                }

                if (expected_ns != Ns){
                    stop = 1;
                 
                    switch (Ns){
                        case 0 : Ns = 1; break;
                        case 1 : Ns = 0; break;
                    }
                } else{
                    alarmCount++;
                }
            }

            if (c == C_REJ0 || c == C_REJ1){
                alarmCount++;
            }

        }
    }

    if (stop == 0){
        return -1;
    } else{
        return bytes;
    }
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    while (1){

        State state = START;
        unsigned char b;
        unsigned char data[2048];
        int index = 0;
    
        unsigned char c = 0;
        while(state != STOP){

            int r = readByteSerialPort(&b);
            
            if (r < 0) {
                return -1;
            } else if (r != 0){
                switch (state){
                    case START:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        }
                        break;
                    case FLAG_RCV:
                        if (b != FLAG){
                            if (b == A_SENDER){
                                state = A_RCV;
                            } else {
                                state = START;
                            }
                        }
                        break;
                    case A_RCV:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        } else if (b == C_IFN0 || b == C_IFN1){
                            state = C_RCV;
                            c = b;
                        } else{
                            state = START;
                        }
                        break;
                    case C_RCV:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        } else if (b == (A_SENDER ^ c)){
                            state = BCC_OK;
                        } else{
                            state = START;
                        }
                        break;
                    case BCC_OK:
                        if (b == FLAG){
                            state = STOP;
                        } else if (b == 0x7D){
                            state = ESCP_RCV;
                        } else{
                            data[index++] = b;
                        }
                        break;
                    case ESCP_RCV:
                        data[index++] = b ^ 0x20;
                        state = BCC_OK;
                        break;
                    default:
                        break;
                }
            }
        }

        if (index == 0) {continue;}


        unsigned char bcc2 = data[index - 1];
        unsigned char bcc2_real = 0;
        for (int i = 0; i < index - 1; i++){
            bcc2_real ^= data[i];
        }

        int n_frame = 0;
        switch (c){
            case C_IFN0: n_frame = 0; break;
            case C_IFN1: n_frame = 1; break;
        }

        if (n_frame != expected_inframe){
            // frame duplicado 

            if (expected_inframe == 0){
                unsigned char response[5] = {FLAG, A_SENDER, C_RR0, A_SENDER ^ C_RR0, FLAG};
                writeBytesSerialPort(response, 5);
            } else if (expected_inframe == 1){
                unsigned char response[5] = {FLAG, A_SENDER, C_RR1, A_SENDER ^ C_RR1, FLAG};
                writeBytesSerialPort(response, 5);
            }
        } else{
            // frame novo

            if (bcc2 != bcc2_real){
                if (expected_inframe == 0){
                    unsigned char response[5] = {FLAG, A_SENDER, C_REJ0, A_SENDER ^ C_REJ0, FLAG};
                    writeBytesSerialPort(response, 5);
                } else if (expected_inframe == 1){
                    unsigned char response[5] = {FLAG, A_SENDER, C_REJ1, A_SENDER ^ C_REJ1, FLAG};
                    writeBytesSerialPort(response, 5);
                }
            } else{
                for (int i = 0; i < index - 1; i++){
                    packet[i] = data[i];
                }

                if (expected_inframe == 0){
                    unsigned char response[5] = {FLAG, A_SENDER, C_RR1, A_SENDER ^ C_RR1, FLAG};
                    writeBytesSerialPort(response, 5);
                    expected_inframe = 1;
                } else if (expected_inframe == 1){
                    unsigned char response[5] = {FLAG, A_SENDER, C_RR0, A_SENDER ^ C_RR0, FLAG};
                    writeBytesSerialPort(response, 5);
                    expected_inframe = 0;
                }

                return index - 1;
            }
        }
    }
}

////////////////////////////////////////////////
// LLCLOSE
////////////////////////////////////////////////
int llCloseTx()
{
    // TODO: Implement this function

    return 0;
}

int llCloseRx()
{
    // TODO: Implement this function

    return 0;
}
