#ifndef CLIENT_H
#define CLIENT_H

#include <QMainWindow>
#include <string.h>
#include <QRegExp>
#include <QList>
#include <QFileDialog>
#include <QFile>
#include <QDir>
#include <QDebug>
#include <QMessageBox>

#include <sys/socket.h>
#include <netinet/in.h>

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

#define PORT_MODE 0
#define PASV_MODE 1
#define TO_FILE 1
#define TO_SCREEN 2

namespace Ui {
class client;
}

class client : public QMainWindow
{
    Q_OBJECT

public:
    explicit client(QWidget *parent = nullptr);
    ~client();
private slots:
    void receiveResponse();
    void newDataConnect();
    void selectFile();
    void comboBoxChange(const QString &text);
    void login();
    void closeSocket();
    void handleCmd();
    void cmd_port();
    void cmd_pasv();
    void cmd_stor(size_t);
    void cmd_retr(bool);
    void cmd_quit();
    void cmd_list();
    void updateFD();
    void updateWD();
private:
    Ui::client *ui;
    int cmdSocket=-1;
    int dataListenSocket=-1;
    int dataCnnSocket=-1;
    struct sockaddr_in addr;
    QString filename;
    char cmd[256];
    char response[1024];
    bool isLogin=false;
    bool mode;

};

#endif // CLIENT_H
