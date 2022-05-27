#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
int const mb = 1048576;
int get_file_size(char *filename) {
    struct stat st;
    if (stat(filename, &st) == 0)
        return st.st_size;
    return -1;
}

int main(int argc, char *argv[])
{
    if(argc != 4){ printf("wrong number of arguements!\n"); exit(1); }

    struct sockaddr_in serv_addr;

    FILE * fd;
    int  socket_fd = -1;
    uint64_t file_size, N, response_size;

    char * server_ip = argv[1];
    unsigned short server_port = atoi(argv[2]);
    char * file_path = argv[3];

    fd = fopen(file_path, "r");

    if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    { perror("Could not create socket\n"); exit(1); }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(server_port); // Note: htons for endiannes
    serv_addr.sin_addr.s_addr = inet_addr(server_ip);

    if(connect(socket_fd, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) < 0)
    { perror("Error : Connect Failed. %d \n"); exit(1); }

    //write size of file to server
    if((file_size = get_file_size(file_path)) < 0) { perror("couldn't get file size!\n"); exit(1); }
    //send N to the server
    N = htonl(file_size);
    char * N_buff = (char *)&N;
    int sent = 0;
    int curr_send;
    while( sizeof(uint64_t) > sent )
    {
        curr_send = write(socket_fd, N_buff + sent, sizeof(uint64_t) - sent);
        if(curr_send < 0){ perror("write of file size to socket failed\n"); exit(1); }
        sent  += curr_send;
    }
    // write data from file to server
    char * buff = malloc(mb);
    int read_size = 0;
    while (feof(fd) == 0){
        read_size = fread(buff, 1, mb, fd);
        if((read_size < mb) && (feof(fd) == 0)){
            perror("there was a problem reading the file");
            exit(1);
        }
        sent = 0;
        while( mb > sent )
        {
            curr_send = write(socket_fd, buff + sent, mb - sent);
            if(curr_send < 0){ perror("write file data to socket failed\n"); exit(1); }
            sent  += curr_send;
        }

    }




    //read count of printable chars from server
    char * count_of_printable_chars_str = (char *)&response_size;
    int read_this_time;
    int read_so_far = 0;
    while(sizeof(uint64_t ) > read_so_far){
        read_this_time = read(socket_fd, count_of_printable_chars_str + read_so_far, sizeof(uint64_t ) - read_so_far);
        read_so_far += read_this_time;
    }

    response_size = ntohl(response_size);
    printf("# of printable characters: %u\n", response_size);

    close(socket_fd);
    exit(0);
}
//
// Created by student on 5/27/22.
//
