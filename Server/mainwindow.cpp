#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <iostream>

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent), ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_server = new QTcpServer();

    if(m_server->listen(QHostAddress::Any, 8080))
    {
       connect(this, &MainWindow::newMessage, this, &MainWindow::displayMessage);
       connect(m_server, &QTcpServer::newConnection, this, &MainWindow::newConnection);
       ui->statusBar->showMessage("Server is listening...");
    }
    else
    {
        QMessageBox::critical(this,"Server",QString("Unable to start the server: %1.").arg(m_server->errorString()));
        exit(EXIT_FAILURE);
    }
}

MainWindow::~MainWindow()
{
    foreach(QPair socket, connection_set)
    {
        socket.first->close();
        socket.first->deleteLater();
    }

    m_server->close();
    m_server->deleteLater();

    delete ui;
}

void MainWindow::newConnection()
{
    while (m_server->hasPendingConnections())
        appendToSocketList(m_server->nextPendingConnection());
}

void MainWindow::appendToSocketList(QTcpSocket* socket)
{
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);
    connect(socket, &QAbstractSocket::errorOccurred, this, &MainWindow::displayError);

    QString id = QString::number(socket->socketDescriptor());
    sendMessage(socket, "New Socket", id);

    foreach(QPair users_socket, connection_set)
    {
        sendMessage(users_socket.first, "New Connect", QString::number(socket->socketDescriptor()));
        sendMessage(socket, "Old Connect", QString::number(users_socket.first->socketDescriptor()));
    }

    connection_set.insert(qMakePair(socket, id));
    ui->comboBox_receiver->addItem(id);
    displayMessage(QString("INFO :: Client with sockd:%1 has just entered the room").arg(socket->socketDescriptor()));
}

void MainWindow::readSocket()
{
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());

    QByteArray byteArray;
    QByteArray byteRecipient;

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_15);

    socketStream.startTransaction();

    socketStream >> byteRecipient;
    socketStream >> byteArray;
    QString receiver = QString::fromStdString(byteRecipient.toStdString());

    while(byteArray != "")
    {
        buffer = QString::fromStdString(byteArray.toStdString());
        if(receiver=="Broadcast")
        {
            foreach (QPair users_socket,connection_set)
            {
                if (socket != users_socket.first)
                {
                    sendMessage(users_socket.first, "Message", QString::number(socket->socketDescriptor()));
                }
            }
        }
        else
        {
            foreach (QPair users_socket,connection_set)
            {
                if(users_socket.first->socketDescriptor() == receiver.toLongLong())
                {
                    sendMessage(users_socket.first, "Message", QString::number(socket->socketDescriptor()));
                    break;
                }
            }
        }
        socketStream >> byteArray;
    }
}

void MainWindow::discardSocket()
{
    QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());
    QPair<QTcpSocket*, QString> elem = qMakePair(socket, QString::number(socket->socketDescriptor()));
    foreach(QPair user_socket, connection_set){
        if (socket == user_socket.first){
            auto it = connection_set.find(user_socket);
            buffer = user_socket.second;

            connection_set.remove(*it);
            break;
        }
    }
    displayMessage(QString("INFO :: A client %1 has just left the room").arg(buffer));
    foreach(QPair user_socket, connection_set){
        sendMessage(user_socket.first, "Disconnect", buffer);
    }

    refreshComboBox();

    socket->deleteLater();
}

void MainWindow::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
        case QAbstractSocket::RemoteHostClosedError:
        break;
        case QAbstractSocket::HostNotFoundError:
            QMessageBox::information(this, "Server", "The host was not found. Please check the host name and port settings.");
        break;
        case QAbstractSocket::ConnectionRefusedError:
            QMessageBox::information(this, "Server", "The connection was refused by the peer. Make sure Server is running, and check that the host name and port settings are correct.");
        break;
        default:
            QTcpSocket* socket = qobject_cast<QTcpSocket*>(sender());
            QMessageBox::information(this, "Server", QString("The following error occurred: %1.").arg(socket->errorString()));
        break;
    }
}

void MainWindow::on_pushButton_sendMessage_clicked()
{
    QString receiver = ui->comboBox_receiver->currentText();
    QString status = "Message";
    buffer = ui->lineEdit_message->text();

    if(receiver=="Broadcast")
    {
        foreach (QPair socket,connection_set)
        {
            sendMessage(socket.first, status, "Server");
        }
    }
    else
    {
        foreach (QPair socket,connection_set)
        {
            if(socket.first->socketDescriptor() == receiver.toLongLong())
            {
                sendMessage(socket.first, status, "Server");
                break;
            }
        }
    }
    ui->lineEdit_message->clear();
}

void MainWindow::sendMessage(QTcpSocket* socket, QString status, QString num_socket)
{
    if(socket)
    {
        if(socket->isOpen())
        {
            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            QByteArray byteStatus = status.toUtf8();
            QByteArray byteArray = buffer.toUtf8();
            QByteArray userNum = num_socket.toUtf8();

            socketStream << byteStatus;
            if (status == "New Connect")
                socketStream << userNum;
            else if (status == "Message")
            {
                socketStream << byteArray;
                socketStream << userNum;
            }
            else if (status == "Old Connect")
                socketStream << userNum;
            else if (status == "Disconnect")
                socketStream << userNum;
            else if (status == "New Socket")
                socketStream << userNum;
        }
        else
            QMessageBox::critical(this,"Server","Socket doesn't seem to be opened");
    }
    else
        QMessageBox::critical(this,"Server","Not connected");
}

void MainWindow::displayMessage(const QString& str)
{
    ui->textBrowser_receivedMessages->append(str);
}

void MainWindow::refreshComboBox(){
    ui->comboBox_receiver->clear();
    ui->comboBox_receiver->addItem("Broadcast");
    foreach(QPair socket, connection_set)
        ui->comboBox_receiver->addItem(QString::number(socket.first->socketDescriptor()));
}
