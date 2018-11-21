//
//  cmd.c
//  server
//
//  Created by 喻琳颖 on 2018/10/29.
//  Copyright © 2018年 喻琳颖. All rights reserved.
//

#include "cmd.h"
#include <ifaddrs.h>

int receive_fd(int fd,char* buf){
    //榨干socket传来的内容
    int p = 0,len;
    while (1) {
        int n = (int)read(fd, buf + p, 8191 - p);
        if (n < 0) {
            printf("Error read(): %s(%d)\n", strerror(errno), errno);
            close(fd);
            continue;
        } else if (n == 0) {
            break;
        } else {
            p += n;
            if (buf[p - 1] == '\n') {
                break;
            }
        }
    }
    if(buf[p-2] == '\r'){
        buf[p-2] = 0;
        len = p - 2;
    }
    else{
        buf[p - 1] = '\0';
        len = p - 1;
    }
    return len;
}

int send_fd(int fd,char* const buf,int len){
    //发送字符串到socket
    int p = 0;
    while (p < len) {
        int n = (int)write(fd, buf + p, len - p);//不包括结尾的'\0'
        if (n < 0) {
            printf("Error write(): %s(%d)\n", strerror(errno), errno);
            return -1;
        } else {
            p += n;
        }
    }
    return 1;
}

void close_socket(struct ftp_server*server){
    if(server->data_csocket!=-1){
        close(server->data_csocket);
        server->data_csocket=-1;
    }
    if(server->data_lsocket!=-1){
        close(server->data_lsocket);
        server->data_lsocket=-1;
    }
}

int get_path(char* wd,char* path){
    if(path[0]=='/'&&!access(path, F_OK)){//有效的绝对路径
        memset(wd, 0, sizeof(wd));
        strcpy(wd, path);
        return 0;
    }
    else if(!strstr(path,"../")&&!strstr(path,"./")){
        strcat(wd, "/");
        strcat(wd, path);
        if(!access(wd, F_OK)){
            return 0;
        }
    }
    return -1;
}

int cmd_user(struct ftp_server* server){
    char* start=strstr(server->cmd," ");
    if(!start){//unacceptable syntax
        sprintf(server->response,"500 request cannot be parsed\r\n");
    }
    else{
        start++;
        if(strstr(start,"anonymous")){//correct
            sprintf(server->response,"331 Guest login ok, send your complete e-mail address as password\r\n");
            server->login_flag=1;
        }
        else{
            sprintf(server->response,"504 Only support anonymous\r\n");
            server->login_flag=0;
        }
    }
    printf("%s\n",server->cmd);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int cmd_pass(struct ftp_server* server){
    char* start=strstr(server->cmd," ");
    char* sym=strstr(server->cmd,"@");
    if(!start)
        sprintf(server->response,"500 request cannot be parsed\r\n");
    else if(!sym)//不含@或者@后没有其他字符
        sprintf(server->response,"501 illegal parameter\r\n");
    else {
        if(!server->login_flag)
            sprintf(server->response,"332 username required\r\n");
        else{
            server->login_flag=2;//先输入用户名
            sprintf(server->response,"230 Guest login ok, access restrictions apply.\r\n");
        }
    }
    printf("%s\n",server->cmd);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int cmd_syst(struct ftp_server* server){
    sprintf(server->response,"215 UNIX Type: L8\r\n");
    printf("%s\n",server->cmd);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int cmd_type(struct ftp_server* server){
    char* start=strstr(server->cmd," ");
    if(!start)
        sprintf(server->response,"500 request cannot be parsed\r\n");
    else if(!strstr(start+1,"I"))
        sprintf(server->response,"501 illegal parameter\r\n");
    else
        sprintf(server->response,"200 Type set to I.\r\n");
    printf("%s\n",server->cmd);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int cmd_pwd(struct ftp_server* server){
    sprintf(server->response,"257 current directory:\"%s\" \r\n",server->wd);
    printf("%s\n",server->cmd);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int cmd_cwd(struct ftp_server* server){
    char* start=strstr(server->cmd," ");
    if(!start)
        sprintf(server->response,"500 request cannot be parsed\r\n");
    else{
        start++;
        char temp[512];
        memset(temp, 0, sizeof(temp));
        strcpy(temp, server->wd);
        if(!get_path(temp, start)){
            memset(server->wd, 0, sizeof(server->wd));
            if(temp[strlen(temp)-1]=='/')
                temp[strlen(temp)-1]=0;
            strcpy(server->wd, temp);
            sprintf(server->response,"250 CWD command successful\r\n");
        }
        else
            sprintf(server->response,"550 No such file or directory\r\n");
    }
    printf("%s\n",server->cmd);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int cmd_mkd(struct ftp_server* server){
    char* start=strstr(server->cmd," ");
    if(!start)
        sprintf(server->response,"500 request cannot be parsed\r\n");
    else{
        start++;
        char temp[512];
        memset(temp, 0, sizeof(temp));
        if(*start=='/'){
            strcpy(temp, start);
        }
        else{
            strcpy(temp, server->wd);
            strcat(temp, "/");
            strcat(temp, start);
        }
        if(!mkdir(temp,0755))
            sprintf(server->response,"257 CMD command successful\r\n");
        else
            sprintf(server->response,"550 Permission denied\r\n");
    }
    printf("%s\n",server->cmd);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int cmd_port(struct ftp_server* server){
    char* start=strstr(server->cmd," ");
    int err=0;
    if(!start){
        sprintf(server->response,"500 request cannot be parsed\r\n");
        printf("%s",server->response);
        err=-1;
    }
    else{
        start++;
        printf("%s\n",server->cmd);
        char*pos=start;
        for(int i=0;i<4;i++){
            pos=strstr(pos,",");
            if(pos)
                *pos='.';
        }
        *pos=0;
        server->addr.sin_family=AF_INET;
        if(inet_pton(AF_INET, start, &(server->addr.sin_addr))<=0){
            printf("Error inet_pton(): %s(%d)\n", strerror(errno), errno);
            sprintf(server->response,"501 illegal parameter\r\n");
            err=-1;
        }
        else{
            pos++;
            server->addr.sin_port=atoi(pos);
            pos=strstr(pos,",");
            if(!pos){
                sprintf(server->response,"501 illegal parameter\r\n");
                err=-1;
            }
            else{
                server->addr.sin_port<<=8;
                server->addr.sin_port+=atoi(++pos);
                server->addr.sin_port=htons(server->addr.sin_port);
                server->mode=PORT_MODE;
                sprintf(server->response,"200 PORT command successful\r\n");
            }
            printf("%s",server->response);
        }
    }
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return err;
}

//获取ipv4地址
void getIPAddr(struct sockaddr_in* addr){
    struct ifaddrs* ifAddrstruct=NULL;
    getifaddrs(&ifAddrstruct);
    while(ifAddrstruct!=NULL){
        if(ifAddrstruct->ifa_addr->sa_family==AF_INET&&!strcmp(ifAddrstruct->ifa_name,"en0"))//ipv4地址
            addr->sin_addr=((struct sockaddr_in*)ifAddrstruct->ifa_addr)->sin_addr;
        ifAddrstruct=ifAddrstruct->ifa_next;
    }
}

int cmd_pasv(struct ftp_server* server){
    if ((server->data_lsocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        printf("Error socket(): %s(%d)\r\n", strerror(errno), errno);
        sprintf(server->response,"Error socket(): %s(%d)\r\n", strerror(errno), errno);
        send_fd(server->cmd_socket, server->response, (int)strlen(server->response));
        return -1;
    }
    //设置本机的ip和port
    struct sockaddr_in addr;
    getIPAddr(&addr);
    //inet_pton(AF_INET, "127.0.0.1", &(addr.sin_addr));
    addr.sin_port=rand()%45536+20000;
    server->addr.sin_addr.s_addr=htonl(INADDR_ANY);
    server->addr.sin_port=htons(addr.sin_port);
    server->addr.sin_family=AF_INET;
    
    //将本机的ip和port与socket绑定
    if (bind(server->data_lsocket, (struct sockaddr*)&server->addr, sizeof(server->addr)) == -1) {
        printf("server Error bind(): %s(%d)\n", strerror(errno), errno);
        sprintf(server->response,"server Error bind(): %s(%d)\r\n", strerror(errno), errno);
        send_fd(server->cmd_socket, server->response, (int)strlen(server->response));
        return -1;
    }
    //开始监听socket
    if (listen(server->data_lsocket, 10) == -1) {
        printf("Error listen(): %s(%d)\n", strerror(errno), errno);
        sprintf(server->response,"Error listen(): %s(%d)\r\n", strerror(errno), errno);
        send_fd(server->cmd_socket, server->response, (int)strlen(server->response));
        close(server->data_lsocket);
        server->data_lsocket=-1;
        return -1;
    }
    server->mode=PASV_MODE;
    
    sprintf(server->response,"227 Entering Passive Mode(%s,%d,%d)\r\n",inet_ntoa(addr.sin_addr),0xff&(addr.sin_port>>8),0xff&addr.sin_port);
    for(int i=0;i<(int)strlen(server->response);i++){
        if(server->response[i]=='.')
            server->response[i]=',';
    }
    printf("%s",server->response);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int connect_data_fd(struct ftp_server*server){
    if(server->mode==PORT_MODE){
        if ((server->data_csocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
            printf("Error socket(): %s(%d)\n", strerror(errno), errno);
            return -1;
        }
        //连接socket
        if (connect(server->data_csocket, (struct sockaddr*)&server->addr, sizeof(server->addr)) < 0) {
            printf("Error connect(): %s(%d)\n", strerror(errno), errno);
            close(server->data_csocket);
            server->data_csocket=-1;
            return -1;
        }
        return 0;
    }
    else if(server->mode==PASV_MODE){
        if ((server->data_csocket= accept(server->data_lsocket, NULL, NULL)) == -1) {
            printf("Error accept(): %s(%d)\n", strerror(errno), errno);
            return -1;
        }
        return 0;
    }
    else{
        //error
        return -1;
    }
}

//另开线程发送数据
void sendData(struct ftp_server*server){
    int len=0;
    char buf[256];
    lseek(server->file,server->offset,SEEK_SET);
    while((len=(int)read(server->file,buf,sizeof(buf)-1))>0){
        if(send_fd(server->data_csocket,buf,len)<0)
            break;
    }
    server->offset=0;
    sprintf(server->response,"226 Transfer complete\r\n");
    close(server->file);
    //关闭端口
    close_socket(server);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    printf("%s",server->response);
}

int cmd_retr(struct ftp_server *server){
    char*start=strstr(server->cmd," ");
    char*filename=start;
    //打开文件
    char temp[512];

    memset(temp, 0, sizeof(temp));
    strcpy(temp, server->wd);
    if(get_path(temp, start+1)){
        sprintf(server->response, "451 file  %s read error\r\n", start);
        //关闭端口
        close_socket(server);
        send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
        return -1;
    }
    server->file=open(temp,O_RDONLY);
    //取文件名
    while(start){
        filename=start;
        start=strstr(++start, "/");
    }
    size_t size=lseek(server->file, 0, SEEK_END);
    sprintf(server->response,"150 Opening BINARY mode data connection for %s(%zu bytes).\r\n",++filename,size);
    send_fd(server->cmd_socket, server->response, (int)strlen(server->response));
    
    //连接数据传输socket
    if(connect_data_fd(server)==-1){
        sprintf(server->response,"426 TCP connection error\r\n");
        //printf("%s",server->response);
        send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
        close_socket(server);
        return -1;
    }
    pthread_t id;
    pthread_create(&id,NULL,(void*)sendData,server);
    return 0;
}

//另开线程收数据
void receiveData(struct ftp_server*server){
    int len=0;
    char buf[256];
    while ((len =(int)read(server->data_csocket,buf,sizeof(buf)-1))  > 0) {
        write(server->file, buf, len);
        server->offset+=len;
    }
    sprintf(server->response, "226 Transfer complete\r\n");
    close(server->file);
    //关闭端口
    close_socket(server);
    printf("%s",server->response);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
}

int cmd_stor(struct ftp_server*server){
    char *start=strstr(server->cmd," ");
    //提取文件名
    char* fn=start;
    while(fn){
        start=fn;
        fn=strstr(++fn, "/");
    }
    start++;
    sprintf(server->response,"150 Opening BINARY mode data connection for %s.\r\n",start);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    
    //连接数据传输socket
    if(connect_data_fd(server)==-1){
        sprintf(server->response,"426 TCP connection error\r\n");
        send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
        close_socket(server);
        return -1;
    }
    char temp[512];
    memset(temp, 0, sizeof(temp));
    strcpy(temp, server->wd);
    strcat(temp, "/");
    strcat(temp, start);
    if((server->file=open(temp, O_CREAT|O_TRUNC|O_WRONLY, 0664))==-1){
        sprintf(server->response, "451 read file error\r\n");
        close_socket(server);
        printf("%s",server->response);
        send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
        return -1;
    }
    pthread_t id;
    pthread_create(&id,NULL,(void*)receiveData,server);
    return 0;
}

int cmd_quit(struct ftp_server*server){
    sprintf(server->response, "221-Thank you for using the FTP service on miniftp\r\n221 Goodbye.\r\n");
    send_fd(server->cmd_socket, server->response, (int)strlen(server->response));
    printf("%s",server->cmd);
    close_socket(server);
    close(server->cmd_socket);
    server->cmd_socket=-1;
    return 0;
}

int cmd_rmd(struct ftp_server*server){
    char* start=strstr(server->cmd," ");
    if(!start)
        sprintf(server->response,"500 request cannot be parsed\r\n");
    else{
        start++;
        //判断目录是否存在
        char temp[512];
        memset(temp, 0, sizeof(temp));
        strcpy(temp, server->wd);
        if(get_path(temp, start)){
            sprintf(server->response,"550 No such File or directory exists\r\n");
        }
        else{
            if(!rmdir(temp))
                sprintf(server->response,"250 RMD command successful\r\n");
            else
                sprintf(server->response,"550 Permission denied\r\n");
        }
    }
    printf("%s\n",server->cmd);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int cmd_rnfr(struct ftp_server*server){
    char* start=strstr(server->cmd," ");
    if(!start)
        sprintf(server->response,"500 request cannot be parsed\r\n");
    else{
        start++;
        char temp[512];
        memset(temp, 0, sizeof(temp));
        strcpy(temp, server->wd);
        if(get_path(temp, start)){
            sprintf(server->response,"550 No such File or directory exists\r\n");
        }
        else{
            sprintf(server->response,"350 file exits\r\n");
            sprintf(server->file_path,"%s",temp);
            server->rnfr_flag=1;
        }
    }
    printf("%s\n",server->cmd);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int cmd_rnto(struct ftp_server*server){
    char* start=strstr(server->cmd," ");
    if(!start)
        sprintf(server->response,"500 request cannot be parsed\r\n");
    else if(server->rnfr_flag!=1)
        sprintf(server->response,"503 enter RNFR cmmand first\r\n");
    else{
        start++;
        char temp[512];
        memset(temp, 0, sizeof(temp));
        if(*start=='/'){
            strcpy(temp, start);
        }
        else{
            strcpy(temp, server->wd);
            strcat(temp, "/");
            strcat(temp, start);
        }
        if(rename(server->file_path, temp))
            sprintf(server->response,"550 file rename error\r\n");
        else{
            sprintf(server->response,"250 RNTO command successful \r\n");
            server->rnfr_flag=0;
        }
    }
    printf("%s\n",server->cmd);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    return 0;
}

int cmd_list(struct ftp_server* server){
    //开始
    sprintf(server->response,"150 Opening BINARY mode data connection for.\r\n");
    send_fd(server->cmd_socket, server->response, (int)strlen(server->response));
    
    DIR *dir=NULL;
    struct dirent * ent=NULL;
    char buf[256]={};
    
    if(!(dir=opendir(server->wd))){
        sprintf(server->response,"451 read diectory error.\r\n");
        send_fd(server->cmd_socket, server->response, (int)strlen(server->response));
        return -1;
    }
    
    //连接数据传输socket
    if(connect_data_fd(server)==-1){
        sprintf(server->response,"426 TCP connection error\r\n");
        send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
        close_socket(server);
        return -1;
    }
    
    while ((ent = readdir(dir))) {
        if (!strcmp(ent->d_name, ".") || !strcmp(ent->d_name, "..")) continue;
        sprintf(buf, "%s",ent->d_name);
        strcat(buf, "\r\n");
        //printf("%s",buf);
        send_fd(server->data_csocket, buf, (int)strlen(buf));
    }
    closedir(dir);
    close_socket(server);
    sprintf(server->response, "226 Transfer complete.\r\n");
    send_fd(server->cmd_socket, server->response, (int)strlen(server->response));
    return 0;
}

int cmd_rest(struct ftp_server*server){
    char* start=strstr(server->cmd," ");
    start++;
    printf("%s\r\n",server->cmd);
    server->offset=atol(start);
    sprintf(server->response, "200 REST command successfull\r\n");
    send_fd(server->cmd_socket, server->response, (int)strlen(server->response));
    return 0;
}

int cmd_size(struct ftp_server*server){
    char* start=strstr(server->cmd," ");
    start++;
    char temp[512];
    memset(temp, 0, sizeof(temp));
    strcpy(temp, server->wd);
    if(get_path(temp, start))
        sprintf(server->response,"550 No such File or directory exists\r\n");
    else{
        int file;
        if((file=open(temp, O_RDONLY))==-1)
            sprintf(server->response,"550 No such File or directory exists\r\n");
        else{
            size_t size=lseek(file, 0, SEEK_END);
            sprintf(server->response, "200 SIZE command:%s(%zu bytes)\r\n",start,size);
            close(file);
        }
    }
    send_fd(server->cmd_socket, server->response, (int)strlen(server->response));
    return 0;
}

int cmd_appe(struct ftp_server*server){
    char *start=strstr(server->cmd," ");
    //提取文件名
    char* fn=start;
    while(fn){
        start=fn;
        fn=strstr(++fn, "/");
    }
    start++;
    sprintf(server->response,"150 Opening BINARY mode data connection for %s.\r\n",start);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    
    //连接数据传输socket
    if(connect_data_fd(server)==-1){
        sprintf(server->response,"426 TCP connection error\r\n");
        send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
        close_socket(server);
        return -1;
    }
    char temp[512];
    memset(temp, 0, sizeof(temp));
    strcpy(temp, server->wd);
    strcat(temp, "/");
    strcat(temp, start);
    if((server->file=open(temp, O_WRONLY|O_APPEND, 0664))==-1){
        sprintf(server->response, "451 read file error\r\n");
        close_socket(server);
        printf("%s",server->response);
        send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
        return -1;
    }
    //新线程收取数据
    pthread_t id;
    pthread_create(&id,NULL,(void*)receiveData,server);
    return 0;
}
