#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<pthread.h>

#define MAX_SIZE 1024
#define MAX_NAME 50
#define MAX_DATA 200

#define LOGIN 1
#define LO_ACK 2
#define LO_NAK 3
#define EXIT 4
#define JOIN 5
#define JOIN_ACK 6
#define JOIN_NAK 7
#define LEAVE_SESS 8
#define NEW_SESS 9
#define NS_ACK 10
#define MESSAGE 11
#define QUERY 12
#define QU_ACK 13
#define NS_NAK 14
#define MSG_NAK 15
#define TIME_OUT 16

typedef struct lab3message {
    unsigned int type;
    unsigned int size;
    unsigned char source[MAX_NAME];
    unsigned char data[MAX_DATA];
}lab3message;

void solve();
int getDynSocket();
int sock = -1;

//read message from sock
int readMessage(lab3message* msg){
    char buff[MAX_SIZE];
    memset(buff, 0, MAX_SIZE);
    int left = sizeof(lab3message);
    int recvBytes  = 0;
    while(1){
        int bytes = recv(sock, buff + recvBytes, left, 0);
        if (sock == -1){
            return 1;
        }
        //printf("buff: %s， bytes= %d\n", buff, bytes);
        if (bytes == -1){
            perror("recv error.\n");
            return 1;
        }
        left -= bytes;
        recvBytes += bytes;
        if (left <= 0){
            break;
        }
    }
    memcpy(msg, buff, sizeof(lab3message));
    //printf("type: %d, data: %s", msg->type, msg->data);
    return 0;
}

//split string by space
void split(char* str, char* args[], int* num){
    char * s = strtok(str, " ");
    *num = 0;
    while(s)
    {
        args[*num] = s;
        *num = *num + 1;
        s = strtok(NULL, " ");
    }
}

//recv message thread function
void *recvMsg(void *arg) {
    lab3message msg;
    while(1){
        if (sock == -1){
            continue;
        }
        if(readMessage(&msg)){
            break;
        }
        printf("%s\n", msg.data);
        //timeout
        if (msg.type == TIME_OUT){
            close(sock);
            exit(-1);
        }
    }
}

//main function
int main(int argc, char *argv[]) {
    pthread_t t;
    int err = pthread_create(&t, NULL, recvMsg, NULL);
    if(err != 0){
        printf("create thread error: %s/n",strerror(err));
        return 1;
    }

    solve();
    return 0;
}

//get a socket dynamic
int getDynSocket()
{
    int resultSock = socket(AF_INET, SOCK_STREAM, 0);

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = 0;

    if (bind(resultSock, (struct sockaddr*)&addr, sizeof(struct sockaddr))) {
        printf("socket failed.\n");
        exit(1);
    }

    struct sockaddr_in connAddr;
    unsigned int len = sizeof connAddr;
    if (getsockname(resultSock, (struct sockaddr*)&connAddr, &len)){
        printf("getsockname failed.\n");
        exit(1);
    }

    return resultSock;
}

//send the message
int sendMessage(int sock, lab3message* msg){
    char buff[MAX_SIZE];
    memset(buff, 0, MAX_SIZE);
    memcpy(buff, msg, sizeof(lab3message));

    int left = sizeof(lab3message);
    int sendBytes  = 0;
    while(1){
        int bytes = send(sock, buff + sendBytes, left, 0);
        //printf("send bytes = %d\n", bytes);
        if (bytes == -1){
            perror("send error.\n");
            return -1;
        }
        left -= bytes;
        sendBytes += bytes;
        if (left <= 0){
            break;
        }
    }

    return 0;
}


//run
void solve(){
    char line[MAX_SIZE];
    char tmp[MAX_NAME];
    char** args = (char**)malloc(sizeof(char*) * MAX_SIZE);
    int num;
    char client_id[MAX_NAME];
    while(NULL != gets(line)){
        strcpy(tmp, line);
        split(tmp, args, &num);
        if (num < 1){
            continue;
        }
        lab3message msg;
        msg.type = -1;
        memset(msg.source, 0, sizeof(msg.source));
        memset(msg.data, 0, sizeof(msg.data));

        //printf("args[0]: %s\n", args[0]);
        //login
        if (strcmp(args[0], "/login") == 0){
            if(num != 5){
                printf("Invalid command !\n");
            }else{
                int tmp_sock = getDynSocket();
                struct sockaddr_in addr;
                memset(&addr, 0, sizeof(addr));
                addr.sin_family = AF_INET;
                addr.sin_addr.s_addr = inet_addr(args[3]);
                addr.sin_port = htons(atol(args[4]));
                if (connect(tmp_sock, (struct sockaddr *) &addr, sizeof(struct sockaddr)) < 0) {
                    perror("connect failed");
                    exit(-1);
                }
                sock = tmp_sock;
                msg.type = LOGIN;
                strcpy(msg.data, args[1]);
                strcat(msg.data, ",");
                strcat(msg.data, args[2]);
                msg.size = strlen(msg.data);
                strcpy(msg.source, args[1]);
                //printf("msg.data=%s\n", msg.data);
            }
        }else if (strcmp(args[0], "/logout") == 0){ //logout
            if(num != 1){
                printf("Invalid command !\n");
            }else{
                msg.type = EXIT;
                strcpy(msg.source, client_id);
            }
        }else if (strcmp(args[0], "/joinsession") == 0){ //login session
            if(num != 2){
                printf("Invalid command !\n");
            }else{
                msg.type = JOIN;
                strcpy(msg.source, client_id);
                strcpy(msg.data, args[1]);
                msg.size = strlen(msg.data);
            }
        }else if (strcmp(args[0], "/leavesession") == 0){ //leave session
            if(num != 1){
                printf("Invalid command !\n");
            }else{
                msg.type = LEAVE_SESS;
                strcpy(msg.source, client_id);
            }
        }else if (strcmp(args[0], "/createsession") == 0){ //create session
            if(num != 2){
                printf("Invalid command !\n");
            }else{
                msg.type = NEW_SESS;
                strcpy(msg.source, client_id);
                strcpy(msg.data, args[1]);
                msg.size = strlen(msg.data);
            }
        }else if (strcmp(args[0], "/list") == 0){ //list
            if(num != 1){
                printf("Invalid command !\n");
            }else{
                msg.type = QUERY;
                strcpy(msg.source, client_id);
            }
        }else if (strcmp(args[0], "/quit") == 0){ //quit
            if(num != 1){
                printf("Invalid command !\n");
            }else{
                msg.type = EXIT;
                strcpy(msg.source, client_id);
            }
        }else{     //text
            msg.type = MESSAGE;
            strcpy(msg.source, client_id);
            strcpy(msg.data, line);
            msg.size = strlen(msg.data);
        }

        if (sock == -1 && msg.type != EXIT){
            printf("Please login first.\n");
            continue;
        }

        if (sock != -1){
            if(sendMessage(sock, &msg)  < 0){
                printf("send failed\n");
                exit(-1);
            }
        }

        //printf("send: %s\n", msg.data);
        //exit
        if (msg.type == EXIT){
            int tmp = sock;
            sock = -1;
            close(tmp);
            if (strcmp(args[0], "/logout") == 0){
                printf("logout successfully\n");
            }else{ //quit
                exit(0);
            }
        }
    }
}
