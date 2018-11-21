#include "client.h"
#include "ui_client.h"
#include <QScrollBar>
#include <ifaddrs.h>
#include <QHeaderView>
client::client(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::client)
{
    ui->setupUi(this);
    //初始化组件
    connect(ui->connect,SIGNAL(clicked()),this,SLOT(login()));
    connect(ui->request,SIGNAL(clicked()),this,SLOT(handleCmd()));
    connect(ui->disconnet,SIGNAL(clicked()),this,SLOT(cmd_quit()));
    connect(ui->comboBox,SIGNAL(currentIndexChanged(const QString &)),this,SLOT(comboBoxChange(const QString &)));
    connect(ui->toolButton,SIGNAL(clicked()),this,SLOT(selectFile()));
    //初始化变量
    memset(cmd,0,sizeof(cmd));
    memset(response,0,sizeof (response));
    //设置工作目录
    QDir::setCurrent(QCoreApplication::applicationDirPath());
}

client::~client()
{
    delete ui;
}

void client::comboBoxChange(const QString &text){
    if(text=="RETR"){
        ui->mode->setEnabled(true);
        ui->command->setEnabled(true);
        ui->toolButton->setEnabled(false);
    }
    else if(text=="STOR"){
        ui->mode->setEnabled(true);
        ui->command->setEnabled(true);
        ui->toolButton->setEnabled(true);
    }
    else if(text=="SYST"||text=="TYPE I"||text=="PWD"){
        ui->mode->setEnabled(false);
        ui->command->setEnabled(false);
        ui->toolButton->setEnabled(false);
    }
    else if(text=="LIST"){
        ui->mode->setEnabled(true);
        ui->command->setEnabled(false);
        ui->toolButton->setEnabled(false);
    }
    else{
        ui->mode->setEnabled(false);
        ui->command->setEnabled(true);
        ui->toolButton->setEnabled(false);
    }
}

void client::handleCmd(){
    if(!isLogin){
        QMessageBox::warning(this,tr("错误"),tr("please connect to server first!"));
        return;
    }
    QString verb=ui->comboBox->currentText();
    strcpy(cmd,verb.toStdString().c_str());
    if(ui->command->isEnabled()){
        if(ui->command->text().isEmpty()){
            QMessageBox::warning(this,tr("错误"),tr("please enter parameters!"));
            return;
        }
        strcat(cmd," ");
        strcat(cmd,ui->command->text().toStdString().c_str());
        ui->command->clear();
    }
    strcat(cmd,"\r\n");
    if(verb=="RETR"){
        if(ui->PORT->isChecked()){//PORT模式
            cmd_port();
        }
        else{//PASV模式
            cmd_pasv();
        }
        //取文件名
        char *ptr=strstr(cmd," ");
        char *filepath=ptr;
        while(ptr){
            filepath=ptr;
            ptr=strstr(++ptr,"/");
        }
        filepath++;
        filename=QString(filepath);
        filename.chop(2);
        if(ui->checkBox->isChecked()){//断点续传
            QFile file(filename);
            size_t size=file.size();
            char cmd_rest[256];
            sprintf(cmd_rest,"REST %zu\r\n",size);
            write(cmdSocket,cmd_rest,strlen(cmd_rest));
            receiveResponse();
            cmd_retr(true);
        }
        else{
            cmd_retr(false);
        }
    }
    else if(verb=="STOR"){
        if(ui->PORT->isChecked()){//PORT模式
            cmd_port();
        }
        else{//PASV模式
            cmd_pasv();
        }
        if(ui->checkBox->isChecked()){
            char* ptr=strstr(cmd, " ");
            char* filename_ptr=ptr;
            while(ptr){
                filename_ptr=ptr;
                ptr=strstr(++ptr,"/");
            }
            char cmd_size[256];
            sprintf(cmd_size,"SIZE %s\r\n",++filename_ptr);
            write(cmdSocket,cmd_size,strlen(cmd_size));
            receiveResponse();
            if(!strstr(response,"200")){
                qDebug()<<"wrong filepath in server!";
                closeSocket();
                return;
            }
            QString qResponse(response);
            int start=qResponse.indexOf(QRegExp("\\(.+bytes\\)"));
            int end=start;
            while(end&&qResponse[end]!=' ')
                end++;
            start++;
            size_t size=qResponse.mid(start,end-start).toULong();
            cmd[0]='A';cmd[1]='P';cmd[2]='P';cmd[3]='E';//发送APPE命令
            cmd_stor(size);
        }
        else{
            cmd_stor(0);
        }
    }
    else{
        write(cmdSocket,cmd,strlen(cmd));
        receiveResponse();
    }
    //更新file directory
    if(verb!="SYST"&&verb!="TYPE I"&&verb!="PWD"&&verb!="RNFR")
        updateFD();
    //更新working directory
    if(verb=="CWD")
        updateWD();
    //重置cmd
    memset(cmd,0,sizeof(cmd));
}

void client::selectFile(){
    QString selectedfile=QFileDialog::getOpenFileName(this,tr("select upload file"),".");
    if(ui->command->isEnabled())
        ui->command->setText(selectedfile);
}

void client::receiveResponse(){
   int len=read(cmdSocket,response,sizeof(response)-1);
   response[len]=0;
   QString text=ui->response->text().replace(QRegExp("<font color=green>"),"<font color=black>");
   QString newArrival=QString(response);
   if(newArrival.startsWith("4")||newArrival.startsWith("5"))
       text.append("<font color=red>"+QString(response)+"</font>");
   else
       text.append("<font color=green>"+QString(response)+"</font>");
   text.replace(QRegExp("\r\n"),"<br>");
   ui->response->setText(text);
   QScrollBar*pScrollBar=ui->scrollArea->verticalScrollBar();
   pScrollBar->setValue(pScrollBar->maximum());
}

void client::closeSocket(){
    if(dataListenSocket!=-1){
        ::close(dataListenSocket);
        dataListenSocket=-1;
    }
    if(dataCnnSocket!=-1){
        ::close(dataCnnSocket);
        dataCnnSocket=-1;
    }
}

void client::login(){
    if(isLogin)
        return;
    QString ip=ui->ip->text();
    QString port=ui->port->text();
    QString username=ui->username->text();
    QString password=ui->password->text();
    //任何一个为空则返回
    if(ip.isEmpty()||port.isEmpty()||username.isEmpty()||password.isEmpty()){
        QMessageBox::warning(this,tr("错误"),tr("please input complete information about ftp server!"));
        return;
    }
    //初始化命令socket
    if ((cmdSocket= socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        qDebug("Error socket(): %s(%d)\n", strerror(errno), errno);
    }
    struct sockaddr_in serverAddr;
    serverAddr.sin_family=AF_INET;
    serverAddr.sin_port=htons(port.toInt());
    if(inet_pton(AF_INET, ip.toStdString().c_str(), &serverAddr.sin_addr)< 0){
        QMessageBox::warning(this,tr("错误"),tr("invalid ip address!"));
        return;
    }
    //连接上目标主机（将socket和目标主机连接）-- 阻塞函数
    if (::connect(cmdSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        QMessageBox::warning(this,tr("错误"),tr("invalid ip address!"));
        return;
    }
    //连续发送USER、PASS指令
    receiveResponse();
    sprintf(cmd,"USER %s\r\n",username.toStdString().c_str());
    write(cmdSocket,cmd,strlen(cmd));
    receiveResponse();
    sprintf(cmd,"PASS %s\r\n",password.toStdString().c_str());
    write(cmdSocket,cmd,strlen(cmd));
    receiveResponse();
    isLogin=true;
    //显示server端WD的文件目录
    updateFD();
    //显示server端工作目录
    updateWD();
}

void client::cmd_quit(){
    if(!isLogin){
        return;
    }
    write(cmdSocket,"QUIT\r\n",strlen("QUIT\r\n"));
    receiveResponse();
    closeSocket();
    ::close(cmdSocket);
    cmdSocket=-1;
    isLogin=false;
}

void getIPAddr(struct sockaddr_in* addr){
    struct ifaddrs* ifAddrstruct=NULL;
    getifaddrs(&ifAddrstruct);
    while(ifAddrstruct!=NULL){
        if(ifAddrstruct->ifa_addr->sa_family==AF_INET&&!strcmp(ifAddrstruct->ifa_name,"en0"))//ipv4地址
            addr->sin_addr=((struct sockaddr_in*)ifAddrstruct->ifa_addr)->sin_addr;
        ifAddrstruct=ifAddrstruct->ifa_next;
    }
}

void client::cmd_port(){
    mode=PORT_MODE;
    //初始化命令socket
    if ((dataListenSocket= socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) == -1) {
        qDebug("Error socket(): %s(%d)\n", strerror(errno), errno);
    }
    int port=rand()%45536+20000;
    addr.sin_family=AF_INET;
    addr.sin_port=htons(port);
    getIPAddr(&addr);
    if(bind(dataListenSocket,(struct sockaddr*)&addr,sizeof(addr))<0){
        QMessageBox::warning(this,tr("错误"),tr("network error!"));
        return;
    }
    if(listen(dataListenSocket,1)<0){
        qDebug("ERROR: listen(): %s(%d)\n", strerror(errno), errno);
        QMessageBox::warning(this,tr("错误"),tr("invalid ip address!"));
        ::close(dataListenSocket);
        dataListenSocket=-1;
        return;
    }
    sprintf(response,"PORT %s,%d,%d\r\n",inet_ntoa(addr.sin_addr),0xff&(port>>8),0xff&port);
    char* comma=strstr(response,".");
    while(comma){
        *comma=',';
        comma=strstr(++comma,".");
    }
    write(cmdSocket,response,strlen(response));
    receiveResponse();
}

void client::cmd_pasv(){
    mode=PASV_MODE;
    write(cmdSocket,"PASV\r\n",strlen("PASV\r\n"));
    receiveResponse();
    char*ip_ptr=strstr(response,"(");
    char* ip=ip_ptr+1;
    for(int i=0;i<4;i++){
        ip_ptr=strstr(++ip_ptr,",");
        if(ip_ptr)*ip_ptr='.';
    }
    *ip_ptr=0;
    inet_pton(AF_INET, ip, &addr.sin_addr);
    char* port=ip_ptr+1;
    addr.sin_port=atoi(port)<<8;
    port=strstr(port,",");
    addr.sin_port+=atoi(++port);
    addr.sin_port=htons(addr.sin_port);
    addr.sin_family=AF_INET;
}

void client::newDataConnect(){
    if(mode==PORT_MODE){
        dataCnnSocket=accept(dataListenSocket,NULL,NULL);
    }
    else{
        dataCnnSocket=socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        ::connect(dataCnnSocket,(struct sockaddr*)&addr,sizeof(addr));
    }
}

void client::cmd_retr(bool isResume){
    //发送命令
    write(cmdSocket,cmd,strlen(cmd));
    receiveResponse();
    if(!strstr(response,"150"))//只有收到150才继续收数据
        return;
    //连接数据socket
    newDataConnect();
    //收取剩余的响应
    if(!strstr(response,"226")||!strstr(response,"426")||!strstr(response,"451"))
        receiveResponse();
    //收取文件内容
    char buf[1024];
    int len=0;
    QFile file(filename);
    if(isResume){//断点续传
        if(!file.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Append)){
            qDebug("ERROR:File operation");
            closeSocket();
            return;
        }
    }
    else{
        if(!file.open(QIODevice::WriteOnly|QIODevice::Text|QIODevice::Truncate)){
            qDebug("ERROR:File operation");
            closeSocket();
            QMessageBox::warning(this,tr("错误"),filename);
            return;
        }
    }
    QDataStream out(&file);
    while((len=read(dataCnnSocket,buf,sizeof(buf)-1))>0){
        out.writeRawData(buf,len);
    }
    file.close();
    closeSocket();
}

void client::cmd_stor(size_t offset){
    //取文件路径
   char* ptr=strstr(cmd," ");
   filename=QString(++ptr);
   filename.chop(2);
   //发送指令
   write(cmdSocket,cmd,strlen(cmd));
   newDataConnect();
   //发送文件内容
   char buf[1024];
   int len=0;
   QFile file(filename);
   if(!file.open(QIODevice::ReadOnly|QIODevice::Text)){
       qDebug("ERROR:File operation");
       closeSocket();
       return;
   }
   //设置文件指针
   file.seek(offset);
   QDataStream in(&file);
   while((len=in.readRawData(buf,sizeof(buf)-1))>0){
       if(write(dataCnnSocket,buf,len)<0)
           break;
   }
   file.close();
   closeSocket();
   receiveResponse();
   if(!strstr(response,"226")||!strstr(response,"426")||!strstr(response,"451"))
       receiveResponse();
}

void client::cmd_list(){
    //发送命令
    write(cmdSocket,"LIST\r\n",strlen("LIST\r\n"));
    newDataConnect();
    receiveResponse();
    if(!strstr(response,"226")||!strstr(response,"426")||!strstr(response,"451"))
        receiveResponse();
    //收取目录内容
    char buf[1024];
    int len=0;
    while((len=read(dataCnnSocket,buf,sizeof(buf)-1))>0){
        buf[len]=0;
        char*ptr=strstr(buf,"\r\n");
        char*entry=buf;
        while(ptr){
            *ptr=0;
            QListWidgetItem*item=new QListWidgetItem;
            item->setText(entry);
            ui->fileDirectory->addItem(item);
            entry=ptr+2;
            ptr=strstr(++ptr,"\r\n");
        }
        memset(buf,0,sizeof(buf));
    }
    /*QScrollBar*pScrollBar=ui->scrollArea_2->verticalScrollBar();
    pScrollBar->setValue(pScrollBar->maximum());
    closeSocket();*/
}

void client::updateFD(){
    //清除原有的items
    //ui->fileDirectory->clear();
    ui->fileDirectory->clear();
    bool type=rand()%2;
    if(type)
        cmd_pasv();
    else
        cmd_port();
    cmd_list();
}

void client::updateWD(){
    write(cmdSocket,"PWD\r\n",strlen("PWD\r\n"));
    receiveResponse();
    char* start=strstr(response,"\"");
    char* end=strstr(++start,"\"");
    *end=0;
    ui->currentDirectory->setText(QString(start));
}

