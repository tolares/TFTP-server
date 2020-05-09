//
// Created by Lagarde Henry on 23/04/2020.
//

#ifndef RSA_PACKET_H
#define RSA_PACKET_H
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INFO               "\x1b[34m INFO : \x1b[0m"
#define SUCCESS            "\x1b[32m SUCCESS : \x1b[0m"
#define ERROR              "\x1b[31m ERROR : \x1b[0m"


void s_to_i(char *f, int n){
    if(n==0){
        f[0] = '0', f[1] = '0', f[2] = '\0';
    } else if(n%10 > 0 && n/10 == 0){
        char c = n+'0';
        f[0] = '0', f[1] = c, f[2] = '\0';
    } else if(n%100 > 0 && n/100 == 0){
        char c2 = (n%10)+'0';
        char c1 = (n/10)+'0';
        f[0] = c1, f[1] = c2, f[2] = '\0';
    } else {
        f[0] = '9', f[1] = '9', f[2] = '\0';
    }
}

// makes ERR packet
char* make_err(char *errcode, char* errmsg){
    char *packet;
    packet = malloc(4+strlen(errmsg));
    memset(packet, 0, sizeof &packet);
    strcat(packet, "05");//opcode
    strcat(packet, errcode);
    strcat(packet, errmsg);
    return packet;
}
/*
 * RRQ packet
 */

char* make_rrq(char *filename){
    char *packet;
    packet = malloc(2+strlen(filename));
    memset(packet, 0, sizeof(&packet));
    strcat(packet, "01");//opcode
    strcat(packet, filename);
    return packet;
}

/*
 * ACK packet
 */

char* make_ack(char* block){
    char *packet;
    packet = malloc(2+strlen(block));
    memset(packet, 0, sizeof(&packet));
    strcat(packet, "04");//opcode
    strcat(packet, block);
    return packet;
}

/*
 * WRQ packet
 */
char* make_wrq(char *filename){
    char *packet;
    packet = malloc(2+strlen(filename));
    memset(packet, 0, sizeof(&packet));
    strcat(packet, "02");//opcode
    strcat(packet, filename);
    return packet;
}

/*
 * DATA packet
 */
char* make_data(int block, char *data){
    char *packet;
    char temp[3];
    s_to_i(temp, block);
    packet = malloc(4+strlen(data));
    memset(packet, 0, sizeof &packet);
    strcat(packet, "03");//opcode
    strcat(packet, temp);
    strcat(packet, data);
    return packet;
}


#endif //RSA_PACKET_H
