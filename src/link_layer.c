// RCOM 2026/2027
//
// Link layer protocol implementation

#include "link_layer.h"
#include "serial_port.h"

#include <stdio.h>
#include <unistd.h>
#include <signal.h>

// MISC
#define _POSIX_SOURCE 1 // POSIX compliant source
#define BUF_SIZE 256

#define FLAG 0x7E

#define A_SENDER 0x03
#define A_RECEIVER 0x01

#define C_SET 0x03
#define C_UA 0x07

typedef enum {START, FLAG_RCV, A_RCV, C_RCV, BCC_OK, STOP} State;

int alarmEnabled = FALSE;
int alarmCount = 0;

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
                        } else if (b == C_UA){
                            state = C_RCV;
                        } else{
                            state = START;
                        }
                        break;
                    case C_RCV:
                        if (b == FLAG){
                            state = FLAG_RCV;
                        } else if (b == A_SENDER ^ C_UA){
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

    // // receber o UA
    // int n = 0;
    // while (n < 5) {
    //     unsigned char b;
    //     int r = readByteSerialPort(&b);

    //     if (r == 1) {
    //         frame[n] = b;
    //         n++;
    //     }
    // }

    // // checka se o frame recebido no UA está de acordo com o enunciado
    // if (frame[0] == FLAG && frame[1] == A_SENDER && frame[2] == C_UA && frame[3] == (A_SENDER ^ C_UA) && frame[4] == FLAG) {
    //     printf("this UA is valid\n");
    // }
    // else {
    //     printf("this UA is invalid\n");
    //     return -1;
    // }

    if (state != STOP){
        printf("no UA received. tried %d times", llParameters.nRetransmissions);
        closeSerialPort();
        return -1;
    }

    printf("UA received successfully");

    // Close serial port
    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port %s closed\n", llParameters.serialPort);

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

    

    

    unsigned char frame[5];

    // ler o frame
    int n = 0;
    while (n < 5) {
        unsigned char b;
        int r = readByteSerialPort(&b);

        if (r == 1) {
            frame[n] = b;
            n++;
        }
    }

    // checka se o frame recebido no SET está de acordo com o enunciado
    if (frame[0] == FLAG && frame[1] == A_SENDER && frame[2] == C_SET && frame[3] == (A_SENDER ^ C_SET) && frame[4] == FLAG) {
        printf("this SET is valid\n");

        // constrói o UA para mandar de volta
        unsigned char ua[5] = {FLAG, A_SENDER, C_UA, A_SENDER ^ C_UA, FLAG};
        writeBytesSerialPort(ua, 5);
        sleep(1);
    }
    else {
        printf("this SET is invalid\n");
        return -1;
    }

    for (int i = 0; i < 5; i++) { printf("var = 0x%02X\n", frame[i]); }

    // Close serial port
    if (closeSerialPort() < 0)
    {
        perror("closeSerialPort");
        return -1;
    }

    printf("Serial port %s closed\n", llParameters.serialPort);

    return 0;
}

////////////////////////////////////////////////
// LLSEND
////////////////////////////////////////////////
int llSend(const unsigned char *buf, int bufSize)
{
    // TODO: Implement this function

    return 0;
}

////////////////////////////////////////////////
// LLRECEIVE
////////////////////////////////////////////////
int llReceive(unsigned char *packet)
{
    // TODO: Implement this function

    return 0;
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
