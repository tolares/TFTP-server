#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>
#include "packet.h"


void usage(char* argv[]) {
    printf(ERROR "Usage : %s <Port Number>\n", argv[0]);
}

int main(int argc, char* argv[]){
    /*
     * Variables Declaration
     */
    struct addrinfo hints, *servinfo, *p;
    struct sockaddr_storage serv_addr;
    socklen_t len;
    struct timeval timeout;
    fd_set fds;
    char buf[550];
    char last_recv_message[550];
    char last_sent_ack[10];
    char filename[100];
    char temp[512+5];
    char block[3];
    char *message, *port;
    int n;
    int rv;
    int sockfd;
    int temp_bytes, len_msg;
    int block_number, restant;
    int total;
    int c_written;
    int  rnd_number;

    srand(time(NULL));
    if(argc != 2){
        usage(argv);
        exit(1);
    }

    port = argv[1];

    /*
     * SERVER Configuration
     */
    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo(NULL, port, &hints, &servinfo)) != 0) {
        fprintf(stderr, ERROR "getaddrinfo: %s\n", gai_strerror(rv));
        return 1;
    }

    /*
     * Connect to the first accessible
     */
    for(p = servinfo; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1) {
            perror(ERROR "socket");
            continue;
        }
        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror(ERROR "bind");
            continue;
        }
        break;
    }
    if (p == NULL) {
        fprintf(stderr, ERROR "Failed to bind socket\n");
        return 2;
    }
    if (p->ai_family == AF_INET) {
        printf(INFO "IPV4 server TFTP version\n" );
    } else {
        printf(INFO "IPV6 server TFTP version\n" );
    }

    printf(INFO "Waiting first request...\n");
    len = sizeof(serv_addr);
    if ((len_msg = recvfrom(sockfd, buf, 550-1 , 0, (struct sockaddr *)&serv_addr, &len)) == -1) {
        perror("recvfrom");
        exit(1);
    }
    buf[len_msg] = '\0';
    printf(INFO "Packet contains \"%s\"\n", buf);

    /*
     * RRQ
     */
    if(strncmp(buf, "01", 2) == 0){
        strcpy(filename, buf+2);
        /*
         * Check File ACCESS
         */
        FILE *fp = fopen(filename, "rb");
        if(fp == NULL || access(filename, F_OK) == -1){
            fprintf(stderr,ERROR "File '%s' does not exist\n", filename);
            message = make_err("02", ERROR "File does not exist");
            sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&serv_addr, len);
            printf(INFO "Error packet sent\n");
            exit(1);
        }

        /*
         * Sending file
         */
        block_number = 1;
        fseek(fp, 0, SEEK_END);
        total = (int) ftell(fp);
        fseek(fp, 0, SEEK_SET);
        restant = total;
        if(restant == 0) ++restant;
        else if(restant%512 == 0) --restant;

        while(restant>0){
            if(restant>512){
                fread(temp, 512, sizeof(char), fp);
                temp[512] = '\0';
                restant -= (512);
            } else {
                fread(temp, restant, sizeof(char), fp);
                temp[restant] = '\0';
                restant = 0;
            }

            /*
             * Data Packet
             */
            message = make_data(block_number, temp);
            if((len_msg = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&serv_addr, len)) == -1){
                perror(ERROR" ACK sendto");
                exit(1);
            }

            /*
             * ACK Waiting
             */
            int count;
            for(count=0;count<=5;++count){
                if(count == 5){
                    printf(ERROR " 5 ACK count. Transfert stopped\n");
                    exit(1);
                }
                FD_ZERO(&fds);
                FD_SET(sockfd, &fds);

                timeout.tv_sec = 5;
                timeout.tv_usec = 0;

                n = select(sockfd+1, &fds, NULL, NULL, &timeout);
                if (n == 0){
                    printf(ERROR "Timeout\n");
                    len_msg = -2;
                } else if (n == -1){
                    printf(ERROR "Error\n");
                    len_msg = -1;
                }else{
                    len_msg = recvfrom(sockfd, buf, 550-1 , 0, (struct sockaddr *)&serv_addr, &len);
                }

                if(len_msg == -1){
                    perror(ERROR "recvfrom");
                    exit(1);
                } else if(len_msg == -2){
                    printf(INFO "Try no. %d\n", count+1);

                    if((temp_bytes = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&serv_addr, len)) == -1){
                        perror(ERROR"ACK: sendto");
                        exit(1);
                    }
                    printf(INFO "Sent %d bytes AGAIN\n", temp_bytes);
                    continue;
                } else {
                    break;
                }
            }
            buf[len_msg] = '\0';

            ++block_number;
            if(block_number>99)
                block_number = 1;
        }
        fclose(fp);
    }
        /*
         * WRQ
         */
    else if(strncmp(buf, "02",2) == 0){
        /*
         * Sending ACK
         */
        message = make_ack("00");
        strcpy(last_recv_message, buf);
        strcpy(last_sent_ack, message);
        if((len_msg = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&serv_addr, len)) == -1){
            perror(ERROR"ACK: sendto");
            exit(1);
        }
        printf(INFO "Sent %d bytes\n", len_msg);

        strcpy(filename, buf+2);
        strcat(filename, ".server");
        /*
         * Check File ACCESS + Exist
         */
        if(access(filename, F_OK) != -1){
            fprintf(stderr,ERROR "File %s already exists\n", filename);
            message = make_err("06", "File Already Exist");
            sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&serv_addr, len);
            printf(INFO "Error packet sent\n");
            exit(1);
        }

        FILE *fp = fopen(filename, "wb");
        if(fp == NULL || access(filename, W_OK) == -1){
            fprintf(stderr,ERROR "File %s access denied\n", filename);
            message = make_err("05", "Access Denied");
            sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&serv_addr, len);
            printf(INFO "Error packet sent\n");
            exit(1);
        }


        do{

            if ((len_msg = recvfrom(sockfd, buf, 550-1 , 0, (struct sockaddr *)&serv_addr, &len)) == -1) {
                perror(ERROR "recvfrom");
                exit(1);
            }
            buf[len_msg] = '\0';

            /*
             * Last ACK Resend
             */
            if(strcmp(buf, last_recv_message) == 0){
                sendto(sockfd, last_sent_ack, strlen(last_sent_ack), 0, (struct sockaddr *)&serv_addr, len);
                continue;
            }

            /*
             * Write File
             */
            c_written = (int) strlen(buf+4);
            fwrite(buf+4, sizeof(char), c_written, fp);
            strcpy(last_recv_message, buf);


            strncpy(block, buf+2, 2);
            block[2] = '\0';
            message = make_ack(block);
            /*
             * Lost ACK simulation
             */
            rnd_number = rand();
            if (rnd_number % 2 != 0) {
                if((len_msg = sendto(sockfd, message, strlen(message), 0, (struct sockaddr *)&serv_addr, len)) == -1){
                    perror(ERROR "ACK: sendto");
                    exit(1);
                }
                printf(INFO "Last ACK resent\n");
            } else {
                printf(ERROR "ACK Lost \n");
            }

            printf(INFO "Sent %d bytes\n", len_msg);
            strcpy(last_sent_ack, message);
        } while(c_written == 512);
        printf(SUCCESS "File : %s created\n", filename);
        fclose(fp);
    } else {
        fprintf(stderr,ERROR "Error in the request\n");
        exit(1);
    }

    close(sockfd);

    return 0;
}
