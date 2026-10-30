// RCOM 2026/2027
//
// Application layer protocol implementation

#include "application_layer.h"
#include "link_layer.h"
#include <time.h>

#include <stdio.h>
#include <string.h>

#define C_START 1
#define C_DATA 2
#define C_END 3

#define T_SIZE 0
#define T_NAME 1

#define MAX_CHUNK 997


void applicationLayer(const char *serialPort, const char *role, int baudRate,
                      int nTries, int timeout, const char *filename)
{
    // ----------------------------------------------------
    // TODO: Adapt and extend this code according to the specifications of the project.
    // ----------------------------------------------------

    LinkLayer llParameters = {
        .baudRate = baudRate,
        .nRetransmissions = nTries,
        .timeout = timeout,
    };
    strcpy(llParameters.serialPort, serialPort);

    if (strcmp(role, "tx") == 0)
    {
        struct timespec t0, t1;
        clock_gettime(CLOCK_MONOTONIC, &t0);
        if (llOpenTx(llParameters) < 0) return;

        FILE *f = fopen(filename, "rb");
        if (f == NULL) { 
            perror("fopen"); 
            llCloseTx();
            return; 
        }

        fseek(f, 0, SEEK_END);
        long fileSize = ftell(f);
        if (fileSize < 0) {
            perror("ftell");
            fclose(f);
            llCloseTx();
            return;
        }
        rewind(f);

        unsigned char start[MAX_PAYLOAD_SIZE];
        int index = 0;

        start[index++] = C_START;

        start[index++] = T_SIZE;
        start[index++] = 4;
        start[index++] = (fileSize >> 24) & 0xFF;
        start[index++] = (fileSize >> 16) & 0xFF;
        start[index++] = (fileSize >> 8) & 0xFF;
        start[index++] = fileSize & 0xFF;

        start[index++] = T_NAME;
        start[index++] = strlen(filename);    
        for (int i = 0; i < strlen(filename); i++) start[index++] = filename[i];

        printf("Sending START packet: fileSize = %ld and filename = %s\n", fileSize, filename);

        if (llSend(start, index) < 0) {
            printf("llSend failed\n");
            fclose(f);
            llCloseTx();
            return;
        }

        unsigned char data[MAX_PAYLOAD_SIZE];
        int bytesRead;

        printf("Sending DATA packets...\n");

        while ((bytesRead = fread(&data[3], 1, MAX_CHUNK, f)) > 0) {
            data[0] = C_DATA;
            data[1] = bytesRead / 256;
            data[2] = bytesRead % 256;

            if (llSend(data, bytesRead + 3) < 0) {
                printf("llSend failed\n");
                fclose(f);
                llCloseTx();
                return;
            }
        }

        printf("Sending END packet\n");

        start[0] = C_END;
        llSend(start, index);

        fclose(f);
        llCloseTx();
        printf("Transfer completed\n");
        clock_gettime(CLOCK_MONOTONIC, &t1);
        double secs = (t1.tv_sec - t0.tv_sec) + (t1.tv_nsec - t0.tv_nsec) / 1e9;
        if (secs > 0) printf("Time: %.2fs, R = %.0f bit/s, S = %.3f\n", secs, fileSize * 8.0 / secs, (fileSize * 8.0 / secs) / llParameters.baudRate);

    }
    else if (strcmp(role, "rx") == 0)
    {
        if (llOpenRx(llParameters) < 0) return;

        unsigned char packet[MAX_PAYLOAD_SIZE];
        int size;

        FILE *out = NULL;
        long expectedSize = 0;
        long written = 0;
        
        while (1) {
            if ((size = llReceive(packet)) < 0) {
                printf("llReceive failed\n");
                break;
            }        

            if (packet[0] == C_START) {
                expectedSize = 0;
                int pos = 1;
                while (pos < size) {
                    int t = packet[pos++];
                    int l = packet[pos++];

                    if (t == T_SIZE) {
                        for (int k = 0; k < l; k++) expectedSize = (expectedSize << 8) | packet[pos + k];
                    }

                    pos += l;
                }
                printf("Received START: expectedSize = %ld bytes\n", expectedSize);

                out = fopen(filename, "wb");
                if (out == NULL) { 
                    perror("fopen"); 
                    llCloseRx();
                    return; 
                }
            }

            else if (packet[0] == C_DATA) {
                if (out == NULL) { 
                    printf("Received DATA packet before START packet\n"); 
                    llCloseRx();
                    return; 
                }
                int k = 256 * packet[1] + packet[2];
                written += fwrite(&packet[3], 1, k, out);
                printf("Written %d bytes\n", k);
            }

            else if (packet[0] == C_END) break;
        }

        printf("Received END packet\n");

        if (out) {
            fclose(out);
            if (written != expectedSize) printf("Warning: wrote %ld bytes, expected %ld\n", written, expectedSize);
        }

        llCloseRx();
    }
    else
    {
        printf("Invalid role: %s. Must be 'tx' or 'rx'.\n", role);
        return;
    }
}
