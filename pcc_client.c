#define  _DEFAULT_SOURCE
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <endian.h>
int const mb = 1048576;
int get_file_size(char *filename) {
    struct stat *buffer = malloc(sizeof *buffer);
    if (stat(filename, buffer) == 0)
        return buffer->st_size;
    return -2;
}
void init_sock_addr(struct sockaddr_in *sock_addr, unsigned short server_port,char* server_ip){
    memset(sock_addr, 0, sizeof(*sock_addr));
    sock_addr -> sin_family = AF_INET;
    sock_addr -> sin_port = htons(server_port);
    sock_addr -> sin_addr.s_addr = inet_addr(server_ip);
}
void send_n_to_server(uint64_t file_size,uint64_t N,int socket_fd){
    N = htobe64(file_size);
    char * N_buff = (char *)&N;
    int sent = 0;
    int curr_send;
    while( sizeof(uint64_t) > sent )
    {
        curr_send = write(socket_fd, N_buff + sent, sizeof(uint64_t) - sent);
        if(curr_send < 0){ perror("there was a problem writing to the socket\n"); exit(1); }
        sent  += curr_send;
    }
}
void read_from_server(int socket_fd,char ** buffer_ptr){
    int current_read = 0;
    int totally_read = 0;
    while(sizeof(uint64_t ) > totally_read){
        current_read = read(socket_fd, *buffer_ptr + totally_read, sizeof(uint64_t ) - totally_read);
        totally_read += current_read;
    }
}

int main(int argc, char *argv[])
{
    if(argc != 4){ printf("wrong number of arguements!\n"); exit(1); }
    struct sockaddr_in sock_addr;
    FILE * fd;
    int  socket_fd = -1;
    uint64_t file_size, N = 0, response_size;

    char * ip = argv[1];
    unsigned short port = atoi(argv[2]);
    char * path = argv[3];

    fd = fopen(path, "r");

    if((socket_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    { perror("creating socket failed\n"); exit(1); }

    init_sock_addr(&sock_addr, port, ip);

    if(connect(socket_fd, (struct sockaddr*) &sock_addr, sizeof(sock_addr)) < 0)
    { perror("failed to connect.\n"); exit(1); }

    if((file_size = get_file_size(path)) < 0) { perror("couldn't get file size!\n"); exit(1); }
    //send N to the server
    send_n_to_server(file_size,N,socket_fd);
    // write data from file to server
    char * buff = malloc(mb);
    int read_size = 0;
    //read from file
    while (feof(fd) == 0){
        read_size = fread(buff, 1, mb, fd);
        if((read_size < mb) && (feof(fd) == 0)){
            perror("there was a problem reading the file");
            exit(1);
        }
        int sent = 0;
        int curr_send = 0;
        while( mb > sent )
        {
            curr_send = write(socket_fd, buff + sent, mb - sent);
            if(curr_send < 0){ perror("there was a problem writing to server\n"); exit(1); }
            sent  += curr_send;
        }

    }
    //read count of printable chars from server
    char * count_of_printable_chars_str = (char *)&response_size;
    read_from_server(socket_fd,&count_of_printable_chars_str);
    response_size = be64toh(response_size);
    printf("# of printable characters: %lu\n", response_size);
    close(socket_fd);
    exit(0);
}
//
// Created by student on 5/27/22.
//vvv
