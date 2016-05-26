#include "com_cyz14_client4over6_MainActivity.h"
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

#define MAXBUF 4112
#define MAX_FIFO_BUF 100

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
int tun_file_descriptor;
int socket_alive = 1;
FILE* tun = NULL;
char *fifo_name = "/data/data/com.cyz14.client4over6/cmd_pipe";
int fifo_handle = 0;
char read_buf[MAX_FIFO_BUF + 1];

struct Message
{
    int length;
    char type;
    char data[4096];
};

void SetFifoName(char* name)
{
    memcpy(fifo_name, name, strlen(name));
}


//Fill msg with given data
void FillMessage(struct Message* msg, int type, char* data, int length)
{
    if (data == NULL)
        length = 0;
    msg->length = 5+length;
    msg->type = type;
    if (data != NULL)
    {
        memset(&(msg->data), 0, sizeof(msg->data));
        memcpy(&(msg->data), data, length);
    }
}

void* ReadThread(void* arg)
{
    ReadTunnel(read_buf, MAX_FIFO_BUF);
}

void* WriteThread(void* arg)
{
    char* wbuf = (char*)arg;
    WriteTunnel(wbuf, strlen(wbuf));
}

//Server Thread
void* DataPackThread(void* arg)
{
    //Local variables
    int length, received_length, pack_type;
    int tid_tun;
    char ip[20], router[20], dns1[20], dns2[20], dns3[20];
    char tun_buffer[16];
    char recv_buffer[MAXBUF + 1];
    struct Message send_msg, recv_msg;

    //Initialize
    memset(recv_buffer, 0, sizeof(recv_buffer));

    FillMessage(&send_msg, 100, NULL, 0);
    if (send(sockfd, &send_msg, sizeof(send_msg), 0) < 0)
    {
        close(sockfd);
        socket_alive = 0;
    }
    send_data_byte_sec += sizeof(send_msg);
    send_pakage_number ++;
    send_package_total_size_byte += sizeof(send_msg);

    while (1)
    {
        if (socket_alive == 0)
        {
            break;
        }

        length = recv(sockfd, &recv_buffer, sizeof(recv_msg), 0);
        received_length = length;
        if (length < 0)
        {
            printf("Error while receiving data from server.\r\n");
            break;
        }
        memcpy(&recv_msg, recv_buffer, sizeof(int));
        while (received_length != recv_msg.length)
        {
            length = recv(sockfd, &(recv_buffer[received_length]), sizeof(recv_msg) - received_length, 0);
            received_length += length;
            if (length < 0)
            {
                printf("Error while receiving data from server.\r\n");
                break;
            }
        }
        memcpy(&recv_msg, recv_buffer, received_length);
        recv_pakage_number ++;
        recv_package_total_size_byte += recv_msg.length;
        recv_data_byte_sec += recv_msg.length;

        if (length == 0)
        {
          continue;
        }

        //Process pack
        switch (recv_msg.type)
        {
            case 101:
                sscanf(recv_msg.data, "%s%s%s%s%s", ip, router, dns1, dns2, dns3);
                WriteTunnel(recv_msg.data, strlen(recv_msg.data));

                ReadTunnel(tun_buffer, 16);
                memcpy(&tun_file_descriptor, tun_buffer, sizeof(int));
                tun = fdopen(tun_file_descriptor, "a+");
            break;

            case 103:
                recv_pakage_number ++;
                recv_package_total_size_byte += strlen(recv_msg.data);
            break;

            case 104:
                prev_heartbeat_time_sec = time(NULL);
            break;

            default:
            break;
        }
        sleep(0);
    }

}

//Heartbeat thread, run once per second
void* HeartbeatPackThread(void* arg)
{
    printf("Activating HeartbeatPackThread\r\n");
    //Local variables
    int timer_start_sec = time(NULL);
    int prev_time_sec = time(NULL);
    int current_time_sec = time(NULL);
    char traffic_data[1000];
    struct Message send_msg;

    //Make heartbeat pack
    FillMessage(&send_msg, 104, NULL, 0);

    //Get current time
    while (1)
    {
        printf("HeartbeatPackThread loop\r\n");
        if (socket_alive == 0)
        {
            break;
        }

        current_time_sec = time(NULL);
        if (current_time_sec != prev_time_sec)
        {
            prev_time_sec = current_time_sec;

            printf("Current time is: %d\r\n",current_time_sec);
            printf("Previous heartbeat receive time is: %d\r\n",prev_heartbeat_time_sec);
            printf("Previous heartbeat send time is: %d\r\n",timer_start_sec);

            //Send traffic data to front end
            //separated by \r\n

            memset(traffic_data, 0, sizeof(traffic_data));
            recv_data_byte_sec = recv_data_byte_sec/1024;
            send_data_byte_sec = send_data_byte_sec/1024;
            sprintf(traffic_data, "%d %d %d %d %d %d", recv_pakage_number, recv_package_total_size_byte,\
            recv_data_byte_sec, send_pakage_number, send_package_total_size_byte, send_data_byte_sec);
            WriteTunnel(traffic_data, strlen(traffic_data));
            recv_data_byte_sec = 0;
            send_data_byte_sec = 0;

            //Check latest heartbeat pack receive time
            if (current_time_sec - prev_heartbeat_time_sec > 60 && prev_heartbeat_time_sec >= 0)
            {
                //Close socket
                close(sockfd);
                socket_alive = 0;
                break;
            }
            else
            {
                //Start timer and check
                if (current_time_sec - timer_start_sec >= 20)
                {
                    //Send heartbeat pack to server
                    if (send(sockfd, &send_msg, 5 ,0) < 0)
                    {
                        //Send error
                        //LOGE("Error while sending data(104) to server.\r\n");
                        break;
                    }
                    send_data_byte_sec += 5;
                    send_pakage_number ++;
                    send_package_total_size_byte += 5;
                    timer_start_sec = time(NULL);
                }
            }
        }
        //sleep
        sleep(1);
    }
}


//Read from TUN file and send to server
void* TunThread(void* arg)
{
    printf("Activating TunThread\r\n");
    //Local variables
    struct Message send_msg;
    char tun_buffer[4096];
    int packlength;

    memset(&send_msg, 0, sizeof(send_msg));
    memset(tun_buffer, 0, sizeof(tun_buffer));

    //Make pack header(102)
    send_msg.length = htonl(sizeof(send_msg));
    send_msg.type = 102;

    while(1)
    {
        if (socket_alive == 0)
        {
            break;
        }
        if (tun == NULL)
        {
          sleep(0);
          continue;
        }

        //Clear send buffer(data part)
        if (send_msg.data[0] != 0)
            memset(&(send_msg.data), 0, sizeof(send_msg.data));

        //Try to read from tun
        fscanf(tun, "%s", tun_buffer);

        //Wrap and send to server if read something
        if (tun_buffer[0] != 0)
        {
            memcpy(&(send_msg.data), tun_buffer, strlen(tun_buffer));
            send_msg.length = 5 + strlen(send_msg.data);
            if (send(sockfd, &send_msg, send_msg.length, 0) < 0)
            {
                //Send error
                //LOGE("Error while sending data(102) to server.\r\n");
                break;
            }
            send_data_byte_sec += send_msg.length;
            send_pakage_number ++;
            send_package_total_size_byte += strlen(send_msg.data);
        }
        else
        {
            sleep(1);
        }
    }
}


//Read from tunnel
//returns length of string
//string length shall not be more than 4096, if so, then the exceeding part will be ignored
int ReadTunnel(char* buffer, int length)
{
    int fifo_handle = open(fifo_name, O_RDONLY);
    int bytes = 0, res;

    if (fifo_handle != -1)
    {
        do
        {
            printf("Reading into tunnel\r\n");
            res = read(fifo_handle, buffer, length);
            bytes += res;
        } while(res > 0);
        close(fifo_handle);
    }
    else
    {
        return -1;
    }

    return bytes;
}

void WriteTunnel(char* buffer, int length)
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
    }
    else {

    }
}

JNIEXPORT jstring JNICALL Java_com_cyz14_client4over6_MainActivity_RequestInfo
    (JNIEnv *env, jobject thiz) {
    int length;
    unsigned long int tid_data, tid_heart, tid_tun;
    struct sockaddr_in6 dest;
    char buffer[MAXBUF + 1];
    struct Message send_msg, recv_msg;
/*
    char cwd[MAX_FIFO_BUF + 1];
    getcwd(cwd, MAX_FIFO_BUF);
    return (*env)->NewStringUTF(env, cwd);
*/

    program_start_time_sec = time(NULL);

    if ((sockfd = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
    {
        printf("Failed to create socket\r\n");
        return NULL;
    }
    memset(&dest, 0, sizeof(dest));
    dest.sin6_family = AF_INET6;
    dest.sin6_port = htons(5678);
    if (inet_pton(AF_INET6, "2402:f000:1:4417::900", &dest.sin6_addr) < 0)
    {
        printf("Illegal address\r\n");
        return NULL;
    }

    int temp = connect(sockfd, (struct sockaddr *) &dest, sizeof(dest));
    if (temp < 0)
    {
        return NULL;
    }

    FillMessage(&send_msg, 100, NULL, 0);

    if (send(sockfd, &send_msg, sizeof(send_msg), 0) < 0)
    {
        close(sockfd);
        socket_alive = 0;
    }
    length = recv(sockfd, &recv_msg, sizeof(recv_msg), 0);

    char ip[20], router[20], dns1[20], dns2[20], dns3[20];
    sscanf(recv_msg.data, "%s%s%s%s%s", ip, router, dns1, dns2, dns3);

    if (access(fifo_name, F_OK) == -1)
    {
        mknod(fifo_name, S_IFIFO | 0666, 0);
    }

    //Activate Threads, and else
//    pthread_create(&tid_data, NULL, DataPackThread, NULL);
//    pthread_create(&tid_heart, NULL, HeartbeatPackThread, NULL);
//    pthread_create(&tid_tun, NULL, TunThread, NULL);

//    pthread_join(tid_data, NULL);
//    pthread_join(tid_heart, NULL);
//    pthread_join(tid_tun, NULL);
//    //WriteTunnel(recv_msg.data, strlen(recv_msg.data));
    return (*env)->NewStringUTF(env, &recv_msg.data[5]);
  }
