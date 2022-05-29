#define  _DEFAULT_SOURCE
#include <sys/socket.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <endian.h>


uint64_t pcc_statistics[126];
int connection = -1;
int has_recieved_sigint = 0;
int const mb = 1048576;

int write_or_read_error(int num){
    if( errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE ){
        fprintf(stderr, "Read/Write data to/from client error: %s\n", strerror(errno));
        return errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE;
    }
    else if(num == 0){
        fprintf(stderr, "client died unexpectedly: %s\n", strerror(errno));
        return 1;
    }
    return 0;
}

void print_printable_chars(){
    for(int i = 32; i < 126; i++)
        printf("char '%c' : %lu times\n", i, pcc_statistics[i]);
    exit(0);
}

void handler(){
    if(connection >= 0)
        has_recieved_sigint = 1;
    else{
        print_printable_chars();
        exit(0);
    }
}

uint64_t update_statistics(char * data_buff, int file_size){
    uint64_t count = 0;
    int b;
    for(int i = 0; i < file_size; i++){
        if(!has_recieved_sigint){
            b = (int) data_buff[i];
            if(b >= 32 && b <= 126){
                count++;
                pcc_statistics[b]++;
            }
        }
    }

    return count;
}
void read_file_size_from_client(int fd,char ** buffer){
    int curr_read;
    int read_so_far = 0;
    while(sizeof(uint64_t) > read_so_far){
        curr_read = read(fd, *buffer + read_so_far, sizeof(uint64_t) - read_so_far);
        if(write_or_read_error(curr_read)) { return; }
        read_so_far += curr_read;
    }
}
void send_to_client(int fd, char ** buffer_ptr){
    int sent = 0;
    int sent_this_iteration;
    while( sizeof(uint64_t) > sent )
    {
        sent_this_iteration = write(fd, *buffer_ptr + sent, sizeof(uint64_t) - sent);
        if(write_or_read_error(sent_this_iteration)){ return; }
        if(sent_this_iteration < 0){ perror("write of file size to socket failed\n"); exit(-1); }
        sent  += sent_this_iteration;
    }
}

void handle_request(int fd){
    //read file_size from client
    uint64_t num_read_from_client;
    char * file_size_str = (char *)&num_read_from_client;
    read_file_size_from_client(fd,&file_size_str);
    int file_size = be64toh(num_read_from_client);
    //read file data from client
    char * data_buff = malloc(mb);
    int read_so_far = 0;
    int curr_read = 0;
    int totally_read_from_file = 0;
    uint64_t count_of_printable_chars = 0;
    uint64_t curr_count_of_printable_chars = 0 ;
    while (file_size > totally_read_from_file){
        read_so_far = 0;
        while(mb > read_so_far){
            curr_read = read(fd, data_buff + read_so_far, mb - read_so_far);
            if(write_or_read_error(curr_read)){ return; }
            read_so_far += curr_read;
        }

        curr_count_of_printable_chars = update_statistics(data_buff, mb);
        count_of_printable_chars += curr_count_of_printable_chars;
        totally_read_from_file += read_so_far;
    }



    count_of_printable_chars = htobe64(count_of_printable_chars);
    char * count_of_printable_chars_buff = (char *)&count_of_printable_chars;
    send_to_client(fd,&count_of_printable_chars_buff);
}
void init_server_address(struct sockaddr_in *serv_addr,int *listen_socket, short port){
    *listen_socket = socket( AF_INET, SOCK_STREAM, 0 );
    memset( serv_addr, 0, sizeof(*serv_addr));
    serv_addr -> sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr -> sin_port = htons(port);
    serv_addr -> sin_family = AF_INET;
}
int main(int argc, char *argv[])
{
    if(argc != 2){ printf("wrong argument count!\n"); exit(-1); }
    memset(&pcc_statistics, 0, sizeof(pcc_statistics));
    //used my hw 2 as a refrence
    struct sigaction sigint;
    sigint.sa_handler = &handler;
    sigemptyset(&sigint.sa_mask);
    sigint.sa_flags = SA_RESTART;
    if (sigaction(SIGINT, &sigint, 0) != 0) {
        fprintf(stderr, "Signal handle registration failed. Error: %s\n", strerror(errno));
        exit(1);
    }

    int rt = 1;
    int listen_socket  = -1;
    short port = atoi(argv[1]);

    struct sockaddr_in serv_addr;
    struct sockaddr_in client_addr;
    socklen_t addrsize = sizeof(struct sockaddr_in );
    init_server_address(&serv_addr,&listen_socket,port);

    if(setsockopt(listen_socket, SOL_SOCKET, SO_REUSEADDR, &rt, sizeof(int)) < 0){
        fprintf(stderr, "setsockopt Error: %s\n", strerror(errno));
        exit(1);
    }


    if(0 != bind(listen_socket, (struct sockaddr*) &serv_addr, addrsize))
    { printf("\n Error : Bind Failed. %s \n", strerror(errno)); return 1; }

    if(0 != listen(listen_socket, 10))
    { printf("\n Error : Listen Failed. %s \n", strerror(errno)); return 1; }

    while( 1 )
    {
        if(has_recieved_sigint)
            print_printable_chars();

        // Accept a connection.
        connection = accept(listen_socket, (struct sockaddr*) &client_addr, &addrsize);
        if(connection < 0) { printf("Accept Failed. %s \n", strerror(errno)); return 1; }

        handle_request(connection);

        // close socket
        close(connection);
        connection = -1;
    }
}
//
// Created by student on 5/27/22.
//
