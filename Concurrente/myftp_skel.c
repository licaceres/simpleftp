#include <stdio.h>
#include <stdlib.h>

#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <err.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "sftp.h"

#define BUFSIZE 512
#define BACKLOG 10

#define MSG_550 "550 %s: no such file or directory\r\n"

/**
 * function: receive and analize the answer from the server
 * sd: socket descriptor
 * code: three leter numerical code to check if received
 * text: normally NULL but if a pointer if received as parameter
 *       then a copy of the optional message from the response
 *       is copied
 * return: result of code checking
 **/
bool recv_msg(int sd, int code, char *text) {
    char buffer[BUFSIZE], message[BUFSIZE];
    int recv_s, recv_code;

    // receive the answer

    recv_s = recv(sd, buffer, BUFSIZE, 0);

    // error checking
    if (recv_s < 0) warn("error receiving data");
    if (recv_s == 0) errx(1, "connection closed by host");

    // parsing the code and message receive from the answer
    sscanf(buffer, "%d %[^\r\n]\r\n", &recv_code, message);
    printf("%d %s\n", recv_code, message);
    // optional copy of parameters
    if(text) strcpy(text, message);
    // boolean test for the code
    return (code == recv_code) ? true : false;
}

/**
 * function: send command formated to the server
 * sd: socket descriptor
 * operation: four letters command
 * param: command parameters
 **/
void send_msg(int sd, char *operation, char *param) {
    char buffer[BUFSIZE] = "";

    // command formating
    if (param != NULL)
        sprintf(buffer, "%s %s\r\n", operation, param);
    else
        sprintf(buffer, "%s\r\n", operation);

    // send command and check for errors

    if(send(sd, buffer, (sizeof(char)*BUFSIZE), 0) == -1)
        printf("Error when sending the command.");

}

/**
 * function: simple input from keyboard
 * return: input without ENTER key
 **/
char * read_input() {
    char *input = malloc(BUFSIZE);
    if (fgets(input, BUFSIZE, stdin)) {
        return strtok(input, "\n");
    }
    return NULL;
}

/**
 * function: login process from the client side
 * sd: socket descriptor
 **/
void authenticate(int sd) {
    char *input, desc[100];
    int code;

    // ask for user
    printf("username: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "USER", input);

    // relese memory
    free(input);

    // wait to receive password requirement and check for errors
    code = 331;
    if(!recv_msg(sd, code, desc)) exit(1);

    // ask for password
    printf("passwd: ");
    input = read_input();

    // send the command to the server
    send_msg(sd, "PASS", input);

    // release memory
    free(input);

    // wait for answer and process it and check for errors
    code = 230;
    if(!recv_msg(sd, code, desc)) exit(1);

}

/**
 * function: operation get
 * sd: socket descriptor
 * file_name: file name to get from the server
 **/
void get(int sd, int datasd, char *file_name) {
    char desc[BUFSIZE], buffer[BUFSIZE];
    int f_size, recv_s, r_size = BUFSIZE;
    FILE *file;

    // send the RETR command to the server
    send_msg(sd, "RETR", file_name);

    // check for the response
    if (recv_msg(sd, 550, NULL)) return;

    // parsing the file size from the answer received
    // "File %s size %ld bytes"
    sscanf(buffer, "150 Opening BINARY mode data connection for %*s (%d bytes)", &f_size);

    // open the file to write
    file = fopen(file_name, "w");

    //receive the file
    while (1){
        recv_s = read(datasd, desc, r_size);
        if (recv_s > 0) fwrite(desc, 1, recv_s, file);
        if (recv_s < r_size) break;
    }

    // close the file
    fclose(file);

    // receive the OK from the server
    if(!recv_msg(sd, 226, NULL))
        printf("Error.");
}

/**
 * function: operation quit
 * sd: socket descriptor
 **/
void quit(int sd) {
    // send command QUIT to the client
    send_msg(sd, "QUIT", NULL);
    // receive the answer from the server
    recv_msg(sd, 221, NULL);
}

int ftp_act(int sd){

    int sd_act, new_fd, sin_size;
    struct sockaddr_in addr_s2, localAddress, lport, t_sock;
    char *port = (char*) malloc (sizeof(char) * 7);
    char *ip_port = (char*) malloc (sizeof(char) * 23);
    char mess[50];
    socklen_t addressLength;

    // create new socket

    if ((sd_act=socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        exit(1);
    }

    memset((char*)&addr_s2, 0, sizeof(addr_s2));
    addr_s2.sin_family = AF_INET;
    addr_s2.sin_addr.s_addr = htonl(INADDR_ANY);
    addr_s2.sin_port = htons(0);


    if (bind(sd_act, (struct sockaddr*)&addr_s2, sizeof(struct sockaddr)) < 0){
        close(sd_act);
        return -1;
    }

    if (listen(sd_act, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }

    addressLength = sizeof(localAddress);
    getsockname(sd, (struct sockaddr*)&localAddress, &addressLength);
    addressLength = sizeof(lport);
    getsockname(sd_act, (struct sockaddr*)&lport, &addressLength);

    sprintf(ip_port, "%s", inet_ntoa(localAddress.sin_addr));

    for (int i = 0; i < strlen(ip_port); i++){
        if(ip_port[i] == '.')
            ip_port[i] = ',';
    }

    port = calc_port((uint16_t)ntohs(lport.sin_port));

    strcat(ip_port, ",");
    strcat(ip_port, port);

    send_msg(sd, "PORT", ip_port);

    if(!recv_msg(sd, 200, mess)) exit(1);
    sin_size = sizeof(struct sockaddr_in);


    new_fd = accept(sd_act, (struct sockaddr*) NULL, NULL);

    free(port);
    free(ip_port);
    return new_fd;
}

void put(int sd, int datasd, char *fname) {
    char desc[BUFSIZE], buffer[BUFSIZE];
    int f_size, recv_s, r_size = BUFSIZE;
    int bread;
    FILE *file;

    if ((file = fopen(fname, "r")) == NULL) {
        printf(MSG_550, fname);
        return;
    }

    send_msg(sd, "STOR", fname);

    recv_msg(sd, 200, NULL);

    recv_msg(sd, 150, NULL);

    // send the file
    while(1) {
        bread = fread(buffer, 1, BUFSIZE, file);
        if (bread > 0) {
            send(datasd, buffer, bread, 0);
            sleep(1);
        }
        if (bread < BUFSIZE) break;
    }

    fclose(file);

    recv_msg(sd, 226, NULL);
}

/**
 * function: make all operations (get|quit)
 * sd: socket descriptor
 **/
void operate(int data_sd, int sd) {
    char *input, *op, *param;


    while (true) {
        printf("Operation: ");
        input = read_input();
        if (input == NULL)
            continue; // avoid empty input
        op = strtok(input, " ");
        // free(input);
        if (strcmp(op, "get") == 0) {
            param = strtok(NULL, " ");
            get(sd, data_sd, param);
        }
        else if (strcmp(op, "put") == 0) {
            param = strtok(NULL, " ");
            put(sd, data_sd, param);
}
        else if (strcmp(op, "quit") == 0) {
            quit(sd);
            break;
        }
        else {
            // new operations in the future
            printf("TODO: unexpected command\n");
        }
        free(input);
    }
    free(input);
}


/**
 * Run with
 *         ./myftp <SERVER_IP> <SERVER_PORT>
 **/
int main (int argc, char *argv[]) {
    int sd, sd_data;
    struct sockaddr_in addr;
    struct hostent *lh = gethostbyname(argv[1]);

    // arguments checking
    if(argc != 3){
        printf("Usage: ./clFtp <IP or domain name> <PORT>");
        exit(1);
    }

    // create socket and check for errors
    if ((sd=socket(AF_INET, SOCK_STREAM, 0)) == -1){
        perror("socket");
        exit(1);
    }
    // set socket data
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(*argv[2]);
    inet_aton(inet_ntoa(*((struct in_addr *)lh->h_addr)), &(addr.sin_addr));
    memset(&addr.sin_zero, '\0', 8);
    // connect and check for errors
    if(connect(sd, (struct sockaddr *)&addr, sizeof(addr)) == -1) exit (1);
    // if receive hello proceed with authenticate and operate if not warning
    if(!recv_msg(sd, 220, NULL))
        warn("Error reciving response...");
    else{
        authenticate(sd);
        sd_data = ftp_act(sd);
        operate(sd_data, sd);
    }

    // close socket

    close(sd);
    return 0;
}
