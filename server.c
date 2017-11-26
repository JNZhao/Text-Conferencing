#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include<pthread.h>
#include <errno.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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
} lab3message;

//hard coded into your program or kept as a database file
int client_num = 3;
char client_ids[][100] = {"Tom", "John", "Sum"};
char passwords[][100] = {"123", "123", "123"};
char session_ids[][100] = {"", "", ""};
int in_session[] = {0, 0, 0};
int client_socks[] = {-1, -1, -1};

int client_index(int sock);

int find_session(char *session_id);

int sendMessage(int sock, lab3message *msg);

int readMessage(int sock, lab3message *msg);

//with each client on the server side, to disconnect clients that have been inactive for a long time.
void timeout(int sock) {
    //disconnect client
    lab3message reply;
    reply.type = TIME_OUT;
    strcpy(reply.data, "Disconnect by server for that have been inactive for a long time");
    if (sendMessage(sock, &reply) < 0) {
        printf("send failed\n");
        exit(-1);
    }
    int index = client_index(sock);
    if (index >= 0) {
        client_socks[index] = -1;
        strcpy(session_ids[index], "");
    }
    close(sock);
}

//read message from sock
int readMessage(int sock, lab3message *msg) {
    char buff[MAX_SIZE];
    memset(buff, 0, MAX_SIZE);
    int left = sizeof(lab3message);
    int recvBytes = 0;
    //printf("sizeof(lab3message)=%d\n", left);
    while (1) {
        int bytes = recv(sock, buff + recvBytes, left, 0);
        //printf("buff: %s， bytes= %d\n", buff, bytes);
        if (bytes == -1) {
            //check for clients that have been inactive for a long time
            if (errno == EAGAIN) {
                timeout(sock);
            } else {
                perror("recv error.\n");
            }
            return -1;
        }
        left -= bytes;
        recvBytes += bytes;
        if (left <= 0) {
            break;
        }
    }
    memcpy(msg, buff, sizeof(lab3message));
    //printf("type: %d, data: %s\n", msg->type, msg->data);
    return 0;
}

//send the message
int sendMessage(int sock, lab3message *msg) {
    char buff[MAX_SIZE];
    memset(buff, 0, MAX_SIZE);
    memcpy(buff, msg, sizeof(lab3message));

    int left = sizeof(lab3message);
    int sendBytes = 0;
    while (1) {
        int bytes = send(sock, buff + sendBytes, left, 0);
        //printf("send bytes = %d\n", bytes);
        if (bytes == -1) {
            perror("send error.\n");
            return -1;
        }
        left -= bytes;
        sendBytes += bytes;
        if (left <= 0) {
            break;
        }
    }

    return 0;
}


//split string by ,
void split(char *str, char *args[], int *num) {
    char *s = strtok(str, ",");
    *num = 0;
    while (s) {
        args[*num] = s;
        //printf("args[%d]=%s\n", *num, args[*num]);
        *num = *num + 1;
        s = strtok(NULL, ",");
    }
}

//check client and password
int check_login_info(int sock, char *client_id, char *passwd) {
    int i;
    for (i = 0; i < client_num; ++i) {
        if (strcmp(client_id, client_ids[i]) == 0 && strcmp(passwd, passwords[i]) == 0) {
            //login successfully
            if (client_socks[i] == -1) {
                client_socks[i] = sock;
                return 0;
            } else {
                //the client has login
                return 1;
            }
        }
    }
    //error client id or password
    return 2;
}

//find client index
int client_index(int sock) {
    int i;
    for (i = 0; i < client_num; ++i) {
        if (client_socks[i] == sock) {
            return i;
        }
    }
    return -1;
}

//find session
int find_session(char *session_id) {
    int i;
    for (i = 0; i < client_num; ++i) {
        if (strcmp(session_ids[i], session_id) == 0) {
            return i;
        }
    }
    return -1;
}

//broadcast message
void broadcast_message(int client_sock, char *message) {
    lab3message msg;
    msg.type = MESSAGE;
    strcpy(msg.data, message);
    char *session_id = session_ids[client_index(client_sock)];
    int i;
    for (i = 0; i < client_num; ++i) {
        if (strcmp(session_id, session_ids[i]) == 0) {
            //send the message
            if (sendMessage(client_socks[i], &msg) < 0) {
                printf("send failed\n");
                exit(-1);
            }
        }
    }
}

//run
void *run(void *arg) {
    lab3message msg, reply;
    int client_sock = *(int *) arg;
    char **args = (char **) malloc(sizeof(char *) * MAX_SIZE);
    int num, is_login = 0;
    int index;
    char username[MAX_SIZE];

    //set a timer with each client on the server side
    struct timeval timeout = {60, 0}; //60s
    setsockopt(client_sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    while (1) {
        //int len = recv(client_sock, &msg, sizeof(msg) + 1, 0);
        if (readMessage(client_sock, &msg)) {
            //perror("recv error\n");
            break;
        }
        //printf("recv: %d, data\n", msg.type, msg.data);
        reply.type = -1;
        //for message type
        switch (msg.type) {
            case LOGIN:
                split(msg.data, args, &num);
                int ret = check_login_info(client_sock, args[0], args[1]);
                if (ret == 0) {
                    reply.type = LO_ACK;
                    strcpy(reply.data, "login successfully.");
                    is_login = 1;
                    strcpy(username, args[0]);
                } else if (ret == 1) {
                    reply.type = LO_NAK;
                    strcpy(reply.data, "this client has login.");
                } else {
                    reply.type = LO_NAK;
                    strcpy(reply.data, "error client id or password.");
                }
                break;
            case EXIT:
                index = client_index(client_sock);
                if (index >= 0) {
                    client_socks[index] = -1;
                    strcpy(session_ids[index], "");
                }
                is_login = 0;
                close(client_sock);
                return NULL;
            case JOIN:
                if (is_login == 0) {
                    reply.type = JOIN_NAK;
                    strcpy(reply.data, "join failed: please login first.");
                } else if (find_session(msg.data) < 0) {
                    reply.type = JOIN_NAK;
                    strcpy(reply.data, "join failed: no such session id.");
                } else {
                    reply.type = JOIN_ACK;
                    strcpy(reply.data, "join successfully.");
                    strcpy(session_ids[client_index(client_sock)], msg.data);
                }
                break;
            case LEAVE_SESS:
                index = client_index(client_sock);
                if (index >= 0) {
                    strcpy(session_ids[index], "");
                }
                break;
            case NEW_SESS:
                if (is_login == 0) {
                    reply.type = NS_NAK;
                    strcpy(reply.data, "new session failed: please login first.");
                } else if (find_session(msg.data) >= 0) {
                    reply.type = NS_NAK;
                    strcpy(reply.data, "new session failed: this session exsist.");
                } else {
                    reply.type = NS_ACK;
                    strcpy(reply.data, "new session successfully.");
                    strcpy(session_ids[client_index(client_sock)], msg.data);
                }
                break;
            case MESSAGE:
                if (is_login == 0) {
                    reply.type = MSG_NAK;
                    strcpy(reply.data, "message failed: please login first.");
                } else if (strcmp(session_ids[client_index(client_sock)], "") == 0) {
                    reply.type = MSG_NAK;
                    strcpy(reply.data, "message failed: you are not in a session.");
                } else {
                    char tmp[MAX_DATA];
                    strcpy(tmp, username);
                    strcat(tmp, " says: ");
                    strcat(tmp, msg.data);
                    broadcast_message(client_sock, tmp);
                }
                break;
            case QUERY:
                if (is_login == 0) {
                    reply.type = QU_ACK;
                    strcpy(reply.data, "list failed: please login first.");
                } else {
                    int i;
                    reply.type = QU_ACK;
                    //strcpy(reply.data, "online users list: \n");
                    strcpy(reply.data, "");
                    for (i = 0; i < client_num; ++i) {
                        if (client_socks[i] != -1) {
                            strcat(reply.data, "user ");
                            strcat(reply.data, client_ids[i]);
                            if (strcmp(session_ids[i], "") == 0) {
                                strcat(reply.data, " not in any session ");
                            } else {
                                strcat(reply.data, " in session ");
                                strcat(reply.data, session_ids[i]);
                            }
                            strcat(reply.data, "\n");
                        }
                    }
                }
                break;
            default:
                break;
        }
        //reply message
        if (reply.type != -1) {
            if (sendMessage(client_sock, &reply) < 0) {
                printf("send failed\n");
                exit(-1);
            }
        }
    }
    close(client_sock);
}

//main function
int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: ./server sport\n");
        exit(-1);
    }
    int port = atoi(argv[1]);

    int server_sock;
    int client_sock;
    struct sockaddr_in m_addr;
    struct sockaddr_in r_addr;
    int size;
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin_family = AF_INET;
    m_addr.sin_addr.s_addr = INADDR_ANY;
    m_addr.sin_port = htons(port);

    if ((server_sock = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printf("socket error !\n");
        exit(-1);
    }

    if (bind(server_sock, (struct sockaddr *) &m_addr, sizeof(struct sockaddr)) < 0) {
        printf("bind error\n");
        exit(-1);
    }

    listen(server_sock, 5);

    size = sizeof(struct sockaddr_in);

    while (1) {
        if ((client_sock = accept(server_sock, (struct sockaddr *) &r_addr, &size)) < 0) {
            perror("accept error !");
            exit(-1);
        }
        int *sock = (int *) malloc(sizeof(int));
        *sock = client_sock;

        pthread_t t;
        int err = pthread_create(&t, NULL, run, (void *) sock);
        if (err != 0) {
            printf("create thread error: %s/n", strerror(err));
            return 1;
        }
    }

    return 0;
}

