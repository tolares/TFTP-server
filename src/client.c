#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
#include <sys/timeb.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include "packet.h"

void usage(char * argv[]) {
    printf(ERROR "Usage : %s <IP Adress> <Port Number> <File name> <Action : RRQ/WRQ>\n",argv[0]);
}

int main(int argc, char *argv[]) {
    /*
     * Initialisation Variables
     */
    int serverSocket;
    int n;
    struct sockaddr_storage serv_adrr;
    struct addrinfo *temp, *res, addr;
    char rcvBuff[1500];
    struct hostent *hp;
    socklen_t len = sizeof(serv_adrr);
    char *file;
    char last_recv_message[550];
    char last_sent_ack[100];
    char block[3];
    char data[517];
    char check_numblock[3];
    char current_numblock[3];
    char block_number[3];
    char nblock[3];
    char *ack_packet, *adress, *port, *packet;
    char opcode[3];
    fd_set fds;
    int check, write;
    int nredif;
    int total;
    int rnd_number;
    int size_remaining, numblock;
    struct timeval timeout;

    /*
     * Verify parameters
     */
    if (argc != 5) {
        usage(argv);
        exit(1);
    }
    srand(time(NULL));
    adress = argv[1];
    port = argv[2];
    file = argv[3];

    /*
     * Serveur info
     */

    memset((char *) &serv_adrr, 0, len);
    memset(&addr, 0, sizeof(addr));
    addr.ai_family = AF_UNSPEC;
    addr.ai_socktype = SOCK_DGRAM;

    if (getaddrinfo(adress, port, &addr, &res) != 0) {
        perror("getaddrinfo");
        exit(1);
    }

    for (temp = res; temp != NULL; temp = temp->ai_next) {
        if ((serverSocket = socket(temp->ai_family, temp->ai_socktype, temp->ai_protocol)) == -1) {
            perror("socket");
            continue;
        }
        break;
    }

    if (temp->ai_family == AF_INET) {
        printf(INFO "IPV4 server TFTP version\n" );
    } else {
        printf(INFO "IPV6 server TFTP version\n" );
    }

    hp = (struct hostent *) gethostbyname(argv[1]);
    if (hp == NULL) {
        printf(ERROR "%s : %s not found in /etc/host or in DNS \n", argv[0], argv[1]);
        exit(1);
    }

    /*
     * RRQ
     */
    if (strncmp(argv[4], "RRQ", 3) == 0) {
        packet = make_rrq(file);
        strcpy(last_recv_message, "");
        strcpy(last_sent_ack, packet);

        printf(INFO "Sending RRQ packet %s \n" , packet);
        if ((n = sendto(serverSocket, packet, strlen(packet), 0, temp->ai_addr, temp->ai_addrlen)) == -1) {
            perror("Erreur sendto");
            exit(1);
        }
        strcat(file, ".client");
        FILE *fileWrite = fopen(file, "wb");
        if (fileWrite == NULL) {
            fprintf(stderr, "File %s can't be open \n", file);
            exit(1);
        }

        do {
            if ((n = recvfrom(serverSocket, rcvBuff, 550 - 1, 0, (struct sockaddr *) &serv_adrr, &len)) == 1) {
                perror("Reception");
                exit(1);
            }

            strncpy(opcode, rcvBuff, 2);
            opcode[2] = '\0';

            if (strcmp(opcode, "03") == 0) {
                strncpy(block_number, rcvBuff + 2, 2);
                block_number[2]= '\0';
                rcvBuff[n] = '\0';
                printf(INFO "Packet number %s of %d octets\n" , block_number, n);
                if (strcmp(rcvBuff, last_recv_message) == 0) {
                    sendto(serverSocket, last_sent_ack, strlen(last_sent_ack), 0, temp->ai_addr, temp->ai_addrlen);
                    continue;
                }

                write = (int) strlen(rcvBuff + 4);
                fwrite(rcvBuff + 4, sizeof(char), write, fileWrite);
                strcpy(last_recv_message, rcvBuff);

                ack_packet = make_ack(block);

                /*
                 * Lost ACK simulation
                 */
                rnd_number = rand();
                if (rnd_number % 2 != 0) {
                    if ((n = sendto(serverSocket, ack_packet, strlen(ack_packet), 0, temp->ai_addr, temp->ai_addrlen)) ==
                        -1) {
                        perror(ERROR "During ACK sending\n");
                        exit(1);
                    }
                    printf(INFO "Last ACK resent\n");
                } else {
                    printf(ERROR "ACK Lost \n");
                }

            }else if(strcmp(opcode, "05") == 0){
                printf("%s\n",rcvBuff + 4);
                exit(1);
            } else {
                printf(ERROR "OPCODE \n" );
            }


        } while (write == 512);

        int count;
        for (count = 0; count <= 5; ++count) {
            if (count == 5) {
                printf(ERROR "5 ACK tries. Transfert stopped \n" );
                exit(1);
            }

            FD_ZERO(&fds);
            FD_SET(serverSocket, &fds);

            timeout.tv_sec = 10;
            timeout.tv_usec = 0;

            printf(INFO "Waiting last ACK \n" );

            check = select(serverSocket + 1, &fds, NULL, NULL, &timeout);

            if (check == 0) {
                printf(SUCCESS "Last ACK Received \n");
                break;
            } else if (check == -1) {
                perror(ERROR "Final select");
            } else {
                if ((n = recvfrom(serverSocket, rcvBuff, 550 - 1, 0, (struct sockaddr *) &serv_adrr, &len)) ==
                    -1) {
                    perror(ERROR"Reception Error \n" );
                    exit(1);
                }
                printf(ERROR "Last ACK not Received \n" );

                rnd_number = rand();
                if (rnd_number % 2 != 0) {
                    if ((n = sendto(serverSocket, last_sent_ack, strlen(last_sent_ack), 0, temp->ai_addr,
                                    temp->ai_addrlen)) == -1) {
                        perror(ERROR"During ACK sending\n");
                        exit(1);
                    }
                    printf(INFO "Last ACK resent\n");
                } else {
                    printf(ERROR "ACK Lost \n");
                }
            }
        }
        printf(SUCCESS "File %s received  \n\n" , file);
        fclose(fileWrite);

    }

    /*
     * WRQ
     */

    if (strncmp(argv[4], "WRQ", 3) == 0) {

        packet = make_wrq(file);
        strcpy(last_recv_message, "");
        strcpy(last_sent_ack, packet);

        printf(INFO "Writing process\n");
        if ((n = sendto(serverSocket, packet, strlen(packet), 0, temp->ai_addr, temp->ai_addrlen)) == -1) {
            perror(ERROR "sendto\n");

            exit(1);
        }
        
        int count;
        for (count = 0;
             count <= 5; ++count) {
            if (count == 5) {
                printf(ERROR " 5 ACK count. Transfert stopped\n");
                exit(1);
            }

            FD_ZERO(&fds);
            FD_SET(serverSocket, &fds);

            timeout.tv_sec = 5;
            timeout.tv_usec = 0;

            check = select(serverSocket + 1, &fds, NULL, NULL, &timeout);
            if (check == 0) {
                printf(ERROR "Timeout\n");
                n = -2;
            } else if (check == -1) {
                printf(ERROR "Error\n");
                n = -1;
            } else {
                n = recvfrom(serverSocket, rcvBuff, 550 - 1, 0, (struct sockaddr *) &serv_adrr, &len);
            }


            if (n == -1) {
                perror(ERROR "recvfrom");
                exit(1);
            } else if (n == -2) {
                printf(INFO "Try no. %d\n", count+1);

                if ((nredif = sendto(serverSocket, last_sent_ack, strlen(last_sent_ack), 0, temp->ai_addr,
                                     temp->ai_addrlen)) == -1) {
                    perror(ERROR"ACK: sendto");
                    exit(1);
                }
                continue;
            } else {
                strncpy(opcode, rcvBuff, 2);
                opcode[2] = '\0';

                if (strcmp(opcode, "04") == 0) {
                    strncpy(check_numblock, rcvBuff + 2, 2);
                    check_numblock[2] = '\0';
                    s_to_i(current_numblock, 0);


                    if (strncmp(check_numblock, current_numblock, 2) == 0) {
                        printf(INFO"ACK received \n");
                        break;
                    }
                }
            }
        }

        /*
         * Check File exist
         */
        FILE *fileRead = fopen(file, "rb");
        if (fileRead == NULL || access(file, F_OK) == -1) { 
            fprintf(stderr,ERROR "File '%s' does not exist\n", file);
            exit(1);
        }

        /*
         * Size file calcul
         */
        numblock = 1;
        fseek(fileRead, 0, SEEK_END);
        total = (int) ftell(fileRead);
        fseek(fileRead, 0, SEEK_SET);
        size_remaining = total;
        if (size_remaining == 0) ++size_remaining;
        else if (size_remaining % 512 == 0) --size_remaining;

        while (size_remaining > 0) {
            
            if (size_remaining > 512) { 
                fread(data, 512, sizeof(char), fileRead);
                data[512] = '\0';
                size_remaining = size_remaining - 512;
            } else {  
                fread(data, size_remaining, sizeof(char), fileRead);
                data[size_remaining] = '\0';
                size_remaining = 0;
            }
            
            char *data_packet = make_data(numblock, data);

            strncpy(nblock, data_packet + 2, 2);
            nblock[2] = '\0';

            if ((n = sendto(serverSocket, data_packet, strlen(data_packet), 0, temp->ai_addr, temp->ai_addrlen)) ==
                -1) {
                perror(ERROR"ACK : sendto");
                exit(1);
            }

            /*
             * ACK waiting
             */
            for (count = 0;
                 count <= 5; ++count) {
                if (count == 5) {
                    printf(ERROR " 5 ACK count. Transfert stopped\n");
                    exit(1);
                }

                FD_ZERO(&fds);
                FD_SET(serverSocket, &fds);

                timeout.tv_sec = 5;
                timeout.tv_usec = 0;

                check = select(serverSocket + 1, &fds, NULL, NULL, &timeout);
                if (check == 0) {
                    printf(ERROR "Timeout\n");
                    n = -2;
                } else if (check == -1) {
                    printf(ERROR "Error\n");
                    n = -1;
                } else {
                    n = recvfrom(serverSocket, rcvBuff, 550 - 1, 0, (struct sockaddr *) &serv_adrr, &len);
                }

                /*
                 * Lost ACK recovering if necessary
                 */
                if (n == -1) {
                    perror(ERROR "recvfrom");
                    exit(1);
                } else if (n == -2) {
                    printf(INFO "Try no. %d\n", count+1);
                    if ((nredif = sendto(serverSocket, data_packet, strlen(data_packet), 0, temp->ai_addr,
                                         temp->ai_addrlen)) == -1) {
                        perror(ERROR"ACK: sendto");
                        exit(1);
                    }
                    printf(INFO "Sent %d bytes AGAIN\n", nredif);
                    continue;
                } else {
                    strncpy(opcode, rcvBuff, 2);
                    opcode[2] = '\0';

                    if (strcmp(opcode, "04") == 0) {

                        strncpy(check_numblock, rcvBuff + 2, 2);
                        check_numblock[2] = '\0';
                        s_to_i(current_numblock, numblock);

                        if (strncmp(check_numblock, current_numblock, 2) == 0) {
                            printf(INFO "ACK received\n");
                            break;
                        }
                    }
                }
            }

            ++numblock;
            if (numblock > 99) { numblock = 1; }
            rcvBuff[n] = '\0';


        }
        fclose(fileRead);

    }
    close(serverSocket);
}
