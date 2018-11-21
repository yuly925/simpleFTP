//
//  cmd.h
//  server
//
//  Created by 喻琳颖 on 2018/10/29.
//  Copyright © 2018年 喻琳颖. All rights reserved.
//

#ifndef cmd_h
#define cmd_h

#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>
#include <sys/stat.h>
#include <ctype.h>
#include <string.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <arpa/inet.h>
#include <dirent.h>

#define PORT_MODE 1
#define PASV_MODE 2
struct ftp_server{
    int cmd_socket;
    int data_lsocket;
    int data_csocket;        //监听socket和连接socket不一样，后者用于数据传输
    struct sockaddr_in addr;
    char cmd[8192];
    char response[8192];
    int login_flag;
    int mode;
    size_t offset;
    int file;
    char file_path[256];
    char wd[512];
    int rnfr_flag;
};


struct cmd_handle{
    const char* cmd;
    int (*func)(struct ftp_server* server);
};

int receive_fd(int fd,char* buf);

int send_fd(int fd,char* const buf,int len);

int cmd_user(struct ftp_server* server);

int cmd_pass(struct ftp_server* server);

int cmd_syst(struct ftp_server* server);

int cmd_type(struct ftp_server* server);

int cmd_pwd(struct ftp_server* server);

int cmd_cwd(struct ftp_server* server);

int cmd_mkd(struct ftp_server* server);

int cmd_port(struct ftp_server* server);

int cmd_pasv(struct ftp_server* server);

int cmd_retr(struct ftp_server *server);

int cmd_stor(struct ftp_server*server);

int cmd_quit(struct ftp_server*server);

int cmd_rmd(struct ftp_server*server);

int cmd_rnfr(struct ftp_server*server);

int cmd_rnto(struct ftp_server*server);

int cmd_list(struct ftp_server* server);

int cmd_rest(struct ftp_server* server);

int cmd_size(struct ftp_server* server);

int cmd_appe(struct ftp_server* server);

#endif /* cmd_h */
