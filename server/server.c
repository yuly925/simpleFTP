#include "cmd.h"

struct ftp_server* client[100];//client
char root[256];//根目录

struct cmd_handle cmd_table[]={
	[0]={
		.cmd="USER",
		.func=cmd_user
	},
	[1]={
		.cmd="PASS",
		.func=cmd_pass
	},
	[2]={
		.cmd="SYST",
		.func=cmd_syst
	},
	[3]={
		.cmd="TYPE",
		.func=cmd_type
	},
	[4]={
		.cmd="PWD",
		.func=cmd_pwd
	},
	[5]={
		.cmd="CWD",
		.func=cmd_cwd
	},
	[6]={
		.cmd="MKD",
		.func=cmd_mkd
	},
    [7]={
        .cmd="PORT",
        .func=cmd_port
    },
    [8]={
        .cmd="PASV",
        .func=cmd_pasv
    },
    [9]={
        .cmd="RETR",
        .func=cmd_retr
    },
    [10]={
        .cmd="STOR",
        .func=cmd_stor
    },
    [11]={
        .cmd="QUIT",
        .func=cmd_quit
    },
    [12]={
        .cmd="ABOR",
        .func=cmd_quit
    },
    [13]={
        .cmd="RMD",
        .func=cmd_rmd
    },
    [14]={
        .cmd="RNFR",
        .func=cmd_rnfr
    },
    [15]={
        .cmd="RNTO",
        .func=cmd_rnto
    },
    [16]={
        .cmd="LIST",
        .func=cmd_list
    },
    [17]={
        .cmd="REST",
        .func=cmd_rest
    },
    [18]={
        .cmd="APPE",
        .func=cmd_appe
    },
    [19]={
        .cmd="SIZE",
        .func=cmd_size
    }
};

//解析命令
int parse_cmd(char*sentence,char*cmd){
	char* start=strstr(sentence," ");
	int len=0;
	if(!start){
		len=(int)strlen(sentence);
		if(len>5)
			return -1;
		strcpy(cmd,sentence);
	}
	else{
		char*p=sentence;
		while(p<start&&len<=5){
			*cmd++=*p++;
			len++;
		}
		if(p!=start)
			return -1;
		*cmd=0;
	}
	return len;
}

//匹配命令
//返回NULL表示匹配失败
struct cmd_handle* match_cmd(struct ftp_server*server){
	char* end=strstr(server->cmd,"\r\n");
	if(!end)
		end=strstr(server->cmd,"\n");
    if(end)
        *end=0;
	//解析命令
	char verb[5];
	int i;
	if(parse_cmd(server->cmd,verb)<0){
		sprintf(server->response,"502 verb cannot be recognized\r\n");
		printf("%s",server->response);
		send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
		return NULL;
	}
	else{
		for(i=0;i<(sizeof(cmd_table)/sizeof(cmd_table[0]));i++){
			if(!strncmp(cmd_table[i].cmd,verb,(int)strlen(cmd_table[i].cmd))){
				return (cmd_table+i);
			}
		}
		sprintf(server->response,"502 verb cannot be recognized\r\n");
		printf("%s",server->response);
		send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
		return NULL;
	}
}

void new_client(struct ftp_server*server){
    struct cmd_handle* handler=NULL;
    char* msg="220 ftp.ssast.org FTP server ready\r\n";
    
    server->login_flag=0;
    server->data_csocket=-1;
    server->data_lsocket=-1;
    server->file=-1;
    strcpy(server->wd, root);
    sprintf(server->response,"%s",msg);
    send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
    while(server->cmd_socket!=-1){
        memset(server->response, 0, sizeof(server->response));
        memset(server->cmd, 0, sizeof(server->cmd));
        receive_fd(server->cmd_socket,server->cmd);
        handler=match_cmd(server);
        if(handler&&server->login_flag!=2&&handler!=cmd_table&&handler!=(cmd_table+1)&&handler!=(cmd_table+11)){
            sprintf(server->response,"332 login required\r\n");
            send_fd(server->cmd_socket,server->response,(int)strlen(server->response));
            continue;
        }
        if(handler){
            if(handler!=cmd_table+14&&handler!=cmd_table+15)
                server->rnfr_flag=0;
            if(handler!=cmd_table+9&&handler!=cmd_table+17)
                server->offset=0;
            handler->func(server);
        }
    }
}

int main(int argc, char **argv) {
    //当前用户数量
    int num=0;
    struct sockaddr_in addr;
    int listen_socket;
	//创建socket
	if ((listen_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		printf("Error socket(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//设置本机的ip和port
	addr.sin_family = AF_INET;
	addr.sin_port = htons(21);
	addr.sin_addr.s_addr = htonl(INADDR_ANY);	//监听"0.0.0.0"
    strcpy(root,"/tmp");
    
    if(argc >= 3){
       for(int i=0;i<argc;i++){
           if(strcmp(argv[i], "-port") == 0){
               addr.sin_port=atoi(argv[i+1]);
               addr.sin_port=htons(addr.sin_port);
           }
           if(strcmp(argv[i], "-root") == 0){
               strcpy(root,argv[i+1]);
               printf("%s",argv[i+1]);
           }
       }
    }
    
	//将本机的ip和port与socket绑定
	if (bind(listen_socket,(struct sockaddr*)&addr, sizeof(addr)) == -1) {
		printf("Error bind(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}

	//开始监听socket
	if (listen(listen_socket, 100) == -1) {
		printf("Error listen(): %s(%d)\n", strerror(errno), errno);
		return 1;
	}
    
    printf("start listening......\n");
    
	while (1) {
        client[num]=malloc(sizeof(struct ftp_server));
        memset(client[num], 0, sizeof(struct ftp_server));
        
		//等待client的连接 -- 阻塞函数
		if ((client[num]->cmd_socket= accept(listen_socket, NULL, NULL)) == -1) {
			printf("Error accept(): %s(%d)\n", strerror(errno), errno);
			continue;
		}
        //多用户
        pthread_t id;
        pthread_create(&id,NULL,(void*)new_client,client[num++]);
	}
	close(listen_socket);
}

