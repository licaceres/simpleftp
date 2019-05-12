#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include "sftp.h"

#include <string.h>
#include <stdbool.h>
#include <stdarg.h>
#include <unistd.h>
#include <err.h>

#include <netinet/in.h>

#define BUFSIZE 512
#define CMDSIZE 5 // valor original 4
#define PARSIZE 100
#define BACKLOG 10

#define MSG_220 "220 srvFtp version 1.0\r\n"
#define MSG_331 "331 Password required for %s\r\n"
#define MSG_230 "230 User %s logged in\r\n"
#define MSG_530 "530 Login incorrect\r\n"
#define MSG_221 "221 Goodbye\r\n"
#define MSG_550 "550 %s: no such file or directory\r\n"
#define MSG_299 "299 File %s size %ld bytes\r\n"
#define MSG_226 "226 Transfer complete\r\n"
#define MSG_540 "540 Invalid command\r\n"


/**
 * function: receive the commands from the client
 * sd: socket descriptor
 * operation: \0 if you want to know the operation received
 *            OP if you want to check an especific operation
 *            ex: recv_cmd(sd, "USER", param)
 * param: parameters for the operation involve
 * return: only usefull if you want to check an operation
 *         ex: for login you need the seq USER PASS
 *             you can check if you receive first USER
 *             and then check if you receive PASS
 **/
bool recv_cmd(int sd, char *operation, char *param) {
    char buffer[BUFSIZE], *token;
    int recv_s;

    // receive the command in the buffer and check for errors
    recv_s = recv(sd, buffer, BUFSIZE, 0);
    if (recv_s < 0) warn("error receiving data");
    if (recv_s == 0) errx(1, "connection closed by host");

    // expunge the terminator characters from the buffer
    buffer[strcspn(buffer, "\r\n")] = 0;

    // complex parsing of the buffer
    // extract command receive in operation if not set \0
    // extract parameters of the operation in param if it needed
    token = strtok(buffer, " ");
    if (token == NULL || strlen(token) < 4) {
        warn("not valid ftp command");
        return false;
    } else {
        if (operation[0] == '\0') strcpy(operation, token);
        if (strcmp(operation, token)) {
            warn("abnormal client flow: did not send %s command", operation);
            return false;
        }
        token = strtok(NULL, " ");
        if (token != NULL) strcpy(param, token);
    }
    return true;
}

/**
 * function: send answer to the client
 * sd: file descriptor
 * message: formatting string in printf format
 * ...: variable arguments for economics of formats
 * return: true if not problem arise or else
 * notes: the MSG_x have preformated for these use
 **/
bool send_ans(int sd, char *message, ...){
    char buffer[BUFSIZE];

    va_list args;
    va_start(args, message);

    vsprintf(buffer, message, args);
    va_end(args);
    // send answer preformated and check errors
    if(send(sd, buffer, (sizeof(char)*BUFSIZE), 0) > -1)
        return true;

    return false;

}

/**
 * function: RETR operation
 * sd: socket descriptor
 * file_path: name of the RETR file
 **/

void retr(int sd, char *file_path) {
    FILE *file = fopen (file_path, "r");
    int bread; // byte read
    long fsize;
    char buffer[BUFSIZE];

    // check if file exists if not inform error to client
    if(!file){
        send_ans(sd, MSG_550, file_path);
    }else{
        struct stat st;
        stat(file_path, &st);
        fsize = st.st_size;
        // send a success message with the file length
        send_ans(sd, MSG_299, file_path, fsize);
        // important delay for avoid problems with buffer size
        //sleep(1);


        // send the file
        while(!feof(file)){
            bread = fread (buffer, 1, BUFSIZE, file);
            if (bread > 0) {
                send(sd, buffer, bread, 0);
                sleep(1);
            } if (bread < BUFSIZE) break;
        }
        // close the file
        fclose(file);
        // send a completed transfer message
        send_ans(sd, MSG_226);
    }
}
/**
 * funcion: check valid credentials in ftpusers file
 * user: login user name
 * pass: user password
 * return: true if found or false if not
 **/
bool check_credentials(char *user, char *pass) {
    FILE *file;
    char *path = "./ftpusers";
    size_t len = 0;
    bool found = false;
    file = fopen (path, "r");

    // make the credential string
    char* cred = (char*)malloc(sizeof(char) * (sizeof(user) + sizeof(pass) + 1));
    strcpy(cred, user);
    strcat(cred, ":");
    strcat(cred, pass);
    //strcat(cred, "\n");

    // check if ftpusers file it's present
    if (file<0){
        fclose(file);
        return false;
    }

    // search for credential string
    found = busqEnArchivo(cred, file);

    // close file and release any pointes if necessary
    fclose(file);

    // return search status
    return found;
}

/**
 * function: login process management
 * sd: socket descriptor
 * return: true if login is succesfully, false if not
 **/
bool authenticate(int sd) {
    char user[PARSIZE], pass[PARSIZE];

    // wait to receive USER action
    recv_cmd(sd, "USER", user);

    // ask for password
    send_ans(sd, MSG_331, user);

    // wait to receive PASS action
    recv_cmd(sd, "PASS", pass);

    // if credentials don't check denied login
    if(check_credentials(user, pass)){
        send_ans(sd, MSG_530);
        return false;
    }
    // confirm login
    send_ans(sd, MSG_230, user);
    return true;
}

/**
 *  function: execute all commands (RETR|QUIT)
 *  sd: socket descriptor
 **/

void operate(int sd) {
    char op[CMDSIZE], param[PARSIZE];

    while (true) {
        op[0] = param[0] = '\0';
        // check for commands send by the client if not inform and exit

        if(!recv_cmd(sd, op, param)) exit(1);

        if (strcmp(op, "RETR") == 0) {
            retr(sd, param);
        } else if (strcmp(op, "QUIT") == 0) {

            // send goodbye and close connection
            send_ans(sd, MSG_221);
            close(sd);

            break;
        } else {
            // invalid command
            send_ans(sd, MSG_540);
            // furute use
        }
    }
}

/**
 * Run with
 *         ./mysrv <SERVER_PORT>
 **/
int main (int argc, char *argv[]) {

    // arguments checking
    if (argc != 2) {
        printf("usage:%s <SERVER_PORT>\n", argv[0]);
        exit(1);
    }

    // reserve sockets and variables space
    int sockfd, new_fd;
    struct sockaddr_in sock;
    struct sockaddr_in t_sock;
    int sin_size;

    sock.sin_family = AF_INET;
    sock.sin_port = htons(*argv[1]);
    sock.sin_addr.s_addr = INADDR_ANY;
    memset(&(sock.sin_zero), '\0', 8);

    // create server socket and check errors
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("socket");
        exit(1);
    }
    // bind master socket and check errors
    if (bind(sockfd, (struct sockaddr *)&sock, sizeof(struct sockaddr)) == -1) {
        perror("bind");
        exit(1);
    }

    // make it listen
    if (listen(sockfd, BACKLOG) == -1) {
        perror("listen");
        exit(1);
    }
    // main loop
    while (true) {

        // accept connectiones sequentially and check errors
        sin_size = sizeof(struct sockaddr_in);
        if ((new_fd = accept(sockfd, (struct sockaddr *)&t_sock,(socklen_t *)&sin_size)) == -1) {
            perror("accept");
            continue;
        }
        printf("server: got connection from %s\n", inet_ntoa(t_sock.sin_addr));

        // send hello
        send_ans(new_fd, MSG_220);

        // operate only if authenticate is true
        if(!authenticate(new_fd)) close(new_fd);
            else operate(new_fd);

    }

    // close server socket
    close(sockfd);
    return 0;
}
