#include "com_cyz14_client4over6_MainActivity.h"
#include <Android/log.h>
#include <jni.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <resolv.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>
#include <time.h>

#define LOGI(...) __android_log_print(ANDROID_LOG_DEBUG,"CYZ_CC",__VA_ARGS__)
//#define LOGW(...) ((void)__android_log_print(ANDROID_LOG_WARN, __FILE__, __VA_ARGS__))
//#define LOGE(...) ((void)__android_log_print(ANDROID_LOG_ERROR, __FILE, __VA_ARGS__))

#define MAXBUF 10240
#define MAX_FIFO_BUF 1024
#define DATA_SIZE 4096

//Global variables
int sockfd;
int program_start_time_sec;
int prev_heartbeat_time_sec = -1;
int recv_pakage_number = 0;
int recv_package_total_size_byte = 0;
int recv_data_byte_sec = 0;
int send_pakage_number = 0;
int send_package_total_size_byte = 0;
int send_data_byte_sec = 0;

int socket_alive = 1;
char *fifo_name = "/data/data/com.cyz14.client4over6/cmd_pipe";
char *tun_name = "/data/data/com.cyz14.client4over6/tun_pipe";

int tun_file_descriptor;
int fifo_handle = 0;

struct Message
{
    int length;
    char type;
    char data[DATA_SIZE];
};

//Fill msg with given data
void FillMessage(struct Message* msg, int type, char* data, int length)
{
    msg->type = type;
    if (data == NULL) {
        length = 0;
    } else // (data != NULL)
    {
        memset(&(msg->data), 0, sizeof(msg->data));
        memcpy(&(msg->data), data, length);
    }
    msg->length = 5+length;
}

//Read from TUN file and send to server
void* TunThread(void* arg)
{
    struct Message send_msg;
    char tun_buffer[DATA_SIZE];
    memset(&send_msg, 0, sizeof(send_msg));
    memset(tun_buffer, 0, sizeof(tun_buffer));

    char show[100];
    while(1)
    {
        if (socket_alive == 0) {
            //LOGI("Socket closed"); break;
            }
        memset(tun_buffer, 0, sizeof(tun_buffer));
        memset(&send_msg, 0, sizeof(send_msg));
        memset(show, 0, sizeof(show));
        int len = read(tun_file_descriptor, tun_buffer, sizeof(tun_buffer));//        fscanf(tun, "%s", tun_buffer);
//        char tempstring[] = "1234567";
//        int len = strlen(tempstring);
//        memcpy(tun_buffer, tempstring, sizeof(tempstring));
        if (len > 0) {
            //LOGI("Read from tun, length = %d", len);
            memcpy(show, tun_buffer, 99);
            show[99] = '\0';
            //LOGI("Tun: %s", show);
            memcpy(send_msg.data, tun_buffer, len);
            send_msg.length = sizeof(send_msg); // 5 + strlen(send_msg.data);
            send_msg.type = 102;
//            memset(tun_buffer, 0, sizeof(tun_buffer));
//            memcpy(tun_buffer, &send_msg, sizeof(send_msg));
            if (send(sockfd, &send_msg, sizeof(send_msg), 0) < 0)
            {
                //LOGI("Error while sending data(102) to server.");
                continue;
            } else
            {
                //LOGI("Sent 102, length = %d", send_msg.length);
                //LOGI("102 data: %s",send_msg.data);
            }
            send_data_byte_sec += send_msg.length;
            send_pakage_number ++;
            send_package_total_size_byte += send_msg.length;
        }
//        else {
//            //LOGI("TUN_FD: %d, Len: %d\n", tun_file_descriptor, len);
//        }
    }
    //LOGI("Exit tun loop.");
}

void* DataPackThread(void* arg) {
    //Local variables
    int length, received_length, pack_type;
    char ip[20], router[20], dns1[20], dns2[20], dns3[20];
    char tun_buffer[MAXBUF + 1];
    char recv_buffer[MAXBUF + 1];
    struct Message send_msg, recv_msg;
    unsigned long int tid_tun = -1;

    FillMessage(&send_msg, 100, NULL, 0);
    if (send(sockfd, &send_msg, sizeof(send_msg), 0) < 0) {
        //LOGI("Send 100 failed.");
        socket_alive = 0;
    } else {
        //LOGI("Sent 100");
        send_data_byte_sec += sizeof(send_msg);
        send_pakage_number++;
        send_package_total_size_byte += sizeof(send_msg);
    }

    while (1) {
        if (socket_alive == 0) { //LOGI("Socket close."); break;
        }
        memset(recv_buffer, 0, sizeof(recv_buffer));
        length = recv(sockfd, recv_buffer, MAXBUF, 0); // no &recv_buffer
        //LOGI("Receive Length: %d", length);
        memcpy(&recv_msg, recv_buffer, sizeof(recv_msg));
        if (length > 0) {
            //LOGI("Received package of type %d", recv_msg.type);
        }
//        while (received_length < recv_msg.length)
//        {
//            length = recv(sockfd, &(recv_buffer[received_length]), MAXBUF - received_length, 0);
//            received_length += length;
//            if (length < 0)
//            {
////                char *toWrite = "Error while receiving data from server.\n";
////                WriteTunnel(toWrite, strlen(toWrite));
//                break;
//            }
//        }
//        memcpy(&recv_msg, recv_buffer, received_length);
        recv_pakage_number++;
        recv_package_total_size_byte += recv_msg.length;
        recv_data_byte_sec += recv_msg.length;

        switch (recv_msg.type) {
            case 101:
                sscanf(recv_msg.data, "%s%s%s%s%s", ip, router, dns1, dns2, dns3);
                char toWrite[DATA_SIZE];
                sprintf(toWrite, "%s %s %s %s %s %d", ip, router, dns1, dns2, dns3, sockfd);
                WriteTunnel(toWrite, strlen(toWrite));
                sleep(2);
                ReadTunnel(tun_buffer, MAX_FIFO_BUF);
                sscanf(tun_buffer, "%d", &tun_file_descriptor);
                //LOGI("FD: %d", tun_file_descriptor);

                pthread_create(&tid_tun, NULL, TunThread, NULL);
                //LOGI("Create Tun thread");
                //pthread_join(tid_tun, NULL);
                break;

            case 103: {
                //LOGI("103 data: %s", recv_msg.data); //fprintf(tun, "%s", recv_msg.data);
                int len = write(tun_file_descriptor, recv_msg.data, sizeof(recv_msg.data));
                if (len < 0) {
                    //LOGI("Write tun data error");
                }

                else {
                    recv_pakage_number++;
                    recv_package_total_size_byte += strlen(recv_msg.data);
                }

            }
                break;

            case 104:
                LOGI("Received pack 104(heartbeat)");
                prev_heartbeat_time_sec = time(NULL);
                break;

            default: {
                //LOGI("Other packet");
                break;
            }
        }
    }
}

void* HeartbeatPackThread(void* arg)
{
    int timer_start_sec = time(NULL);
    int prev_time_sec = time(NULL);
    int current_time_sec = time(NULL);
    char traffic_data[1000];
    struct Message send_msg;

    FillMessage(&send_msg, 104, NULL, 0);

    while (1)
    {
        if (socket_alive == 0)
        {
            LOGI("Exit heartbeat thread due to closed socket.");
            break;
        }
        sleep(1);
        LOGI("Heartbeat loop");
        LOGI("current time:%d prev_heartneat:%d timer_start:%d", current_time_sec, prev_heartbeat_time_sec, timer_start_sec);
        current_time_sec = time(NULL);
        if (current_time_sec != prev_time_sec)
        {
            prev_time_sec = current_time_sec;
            memset(traffic_data, 0, sizeof(traffic_data));
            recv_data_byte_sec = recv_data_byte_sec/1024;
            send_data_byte_sec = send_data_byte_sec/1024;
            sprintf(traffic_data, "%d %d %d %d %d %d",
                    recv_pakage_number,
                    recv_package_total_size_byte,
                    recv_data_byte_sec,
                    send_pakage_number,
                    send_package_total_size_byte,
                    send_data_byte_sec);

            recv_data_byte_sec = 0;
            send_data_byte_sec = 0;

            //Check latest heartbeat pack receive time
            if (current_time_sec - prev_heartbeat_time_sec > 60 && prev_heartbeat_time_sec >= 0)
            {
//                close(sockfd);
                LOGI("Heartbeat timeout, close socket.");
                socket_alive = 0;
                break;
            }
            else
            {
                if (current_time_sec - timer_start_sec >= 20)
                {
                    if (send(sockfd, &send_msg, sizeof(send_msg) ,0) < 0)
                    {
                        LOGI("Send 104 failed.");
                    }
                    else
                    {
                        LOGI("Send 104 pack");
                    }
                    send_data_byte_sec += sizeof(send_msg);
                    send_pakage_number ++;
                    send_package_total_size_byte += 5;
                    timer_start_sec = time(NULL);
                }
            }
        }
    }
    LOGI("out of heart thread loop");
}

//Read tun descriptor from tunnel
int ReadTunnel(char* buffer, int length)
{
    fifo_handle = open(tun_name, O_RDONLY);
    int bytes = 0, res;

    if (fifo_handle != -1)
    {
        do // bug
        {
            res = read(fifo_handle, buffer, length);
//            if (res < 0) break;
            bytes += res;
        } while(res > 0);
    }
    else
    {
//        close(fifo_handle);
        //LOGI("FIFO_HANDLE == -1");
    }
    if (fifo_handle != -1)
        close(fifo_handle);
    return bytes;
}

int WriteTunnel(char* buffer, int length) // write to fifo_name pipe
{
    int bytes = 0, res = 0;
    int fifo_handle = open(fifo_name, O_WRONLY);
    if (fifo_handle >= 0)
    {
        while (bytes < length) {
            res = write(fifo_handle, buffer, length);
            if (res <= 0) {
                break;
            }
            bytes += res;
        }
        close(fifo_handle);
        return 0;
    }
    else {
        return -1;
    }
}


JNIEXPORT jstring JNICALL Java_com_cyz14_client4over6_MainActivity_RequestInfo
        (JNIEnv *env, jobject thiz) {
    int length;
    unsigned long int tid_data, tid_heart;
    struct sockaddr_in6 dest;
    char buffer[MAXBUF + 1];
    struct Message send_msg, recv_msg;
    memset(&recv_msg, 0, sizeof(recv_msg));

    program_start_time_sec = time(NULL);

    if ((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
        //LOGI("Socket init error");

    memset(&dest, 0, sizeof(dest));
    dest.sin6_family = AF_INET6;
    dest.sin6_port = htons(5678);
    if (inet_pton(AF_INET6, "2402:f000:1:4417::900", &dest.sin6_addr) < 0)
    {
        //LOGI("Net address error");
    }

    int temp = connect(sockfd, (struct sockaddr *) &dest, sizeof(dest));
    if (temp < 0){
        //LOGI("Connection failed.");
    }

//    FillMessage(&send_msg, 100, NULL, 0);
//
//    if (send(sockfd, &send_msg, sizeof(send_msg), 0) < 0)
//    {
//        close(sockfd);
//        socket_alive = 0;
//    }
//    length = recv(sockfd, &recv_msg, sizeof(recv_msg), 0);

    if (access(fifo_name, F_OK) == -1)
    {
        mknod(fifo_name, S_IFIFO | 0666, 0);
    }
    if (access(tun_name, F_OK) == -1)
    {
        mknod(tun_name, S_IFIFO | 0666, 0);
    }
    //Activate Threads, and else
    pthread_create(&tid_data, NULL, DataPackThread, NULL);
    pthread_create(&tid_heart, NULL, HeartbeatPackThread, NULL);

//    WriteTunnel(recv_msg.data, strlen(recv_msg.data));

//    ReadTunnel(read_buf, MAX_FIFO_BUF);
//    char extra[10];
//    sscanf(read_buf, "%d%s", &tun_file_descriptor, extra);
    char ret[20];
    sprintf(ret, "Sockfd: %d\n", sockfd);
    pthread_join(tid_data, NULL);
    pthread_join(tid_heart, NULL);
    return (*env)->NewStringUTF(env, ret);
}
