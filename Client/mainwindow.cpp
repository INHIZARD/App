#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    socket = new QTcpSocket(this);

    connect(this, &MainWindow::newMessage, this, &MainWindow::displayMessage);
    connect(socket, &QTcpSocket::readyRead, this, &MainWindow::readSocket);
    connect(socket, &QTcpSocket::disconnected, this, &MainWindow::discardSocket);
    connect(socket, &QAbstractSocket::errorOccurred, this, &MainWindow::displayError);

    socket->connectToHost(QHostAddress::LocalHost,8080);

    if(socket->waitForConnected())
        ui->statusBar->showMessage("Соединение с сервером установлено!");
    else{
        QMessageBox::critical(this,"Client", QString("Произошла следующая ошибка: %1.").arg(socket->errorString()));
        exit(EXIT_FAILURE);
    }
}

MainWindow::~MainWindow()
{
    if(socket->isOpen())
    {
        QDataStream socketStream(socket);
        socketStream.setVersion(QDataStream::Qt_5_15);
        socket->close();
    }
    delete ui;
}

void MainWindow::readSocket()
{
    QByteArray statusBuffer;
    QByteArray buffer;

    QDataStream socketStream(socket);
    socketStream.setVersion(QDataStream::Qt_5_15);

    socketStream.startTransaction();
    socketStream >> statusBuffer;
    socketStream >> buffer;

    while(statusBuffer != "")
    {

        if (statusBuffer.toStdString() == "Message")
        {
            QByteArray user;
            socketStream >> user;
            QString message = QString("%1 :: %2").arg(QString::fromStdString(user.toStdString())).arg(QString::fromStdString(buffer.toStdString()));
            emit newMessage(message);
        }
        else if (statusBuffer.toStdString() == "New Connect")
        {
            QString message = QString("%1 :: %2").arg(QString::fromStdString(buffer.toStdString())).arg(QString::fromStdString("Привет! Я онлайн"));
            emit newMessage(message);
            connection_set.insert(QString::fromStdString(buffer.toStdString()));
            refreshComboBox();
        }
        else if (statusBuffer.toStdString() == "Old Connect")
        {
            connection_set.insert(QString::fromStdString(buffer.toStdString()));
            refreshComboBox();
        }
        else if (statusBuffer.toStdString() == "Disconnect")
        {
            QString message = QString("%1 :: %2").arg(QString::fromStdString(buffer.toStdString())).arg(QString::fromStdString("*Покинул чат*"));
            emit newMessage(message);
            auto it  = connection_set.find(QString::fromStdString(buffer.toStdString()));
            connection_set.erase(it);
            refreshComboBox();
        }
        else if (statusBuffer.toStdString() == "New Socket")
        {
            id = QString::fromStdString(buffer.toStdString());
            this->setWindowTitle(QString::fromStdString("Client %1").arg(id));
        }

        socketStream >> statusBuffer;
        socketStream >> buffer;
    }
}

void MainWindow::discardSocket()
{
    socket->deleteLater();
    socket=nullptr;

    ui->statusBar->showMessage("Соединение потеряно!");
}

void MainWindow::displayError(QAbstractSocket::SocketError socketError)
{
    switch (socketError) {
        case QAbstractSocket::RemoteHostClosedError:
        break;
        case QAbstractSocket::HostNotFoundError:
            QMessageBox::information(this, "Client", "Сервер не найден. Проверьте настройки порта и IP.");
        break;
        case QAbstractSocket::ConnectionRefusedError:
            QMessageBox::information(this, "Client", "Соединение прервано. Убедитесь запущен ли сервер, и проверьте настройки порта и IP.");
        break;
        default:
            QMessageBox::information(this, "Client", QString("Произошла следующая ошибка: %1.").arg(socket->errorString()));
        break;
    }
}

void MainWindow::on_pushButton_sendMessage_clicked()
{
    if(socket)
    {
        if(socket->isOpen())
        {
            QString str = ui->lineEdit_message->text();
            QString recipient = QString::fromStdString(ui->comboBox_receiver->currentText().toStdString());

            QDataStream socketStream(socket);
            socketStream.setVersion(QDataStream::Qt_5_15);

            QByteArray byteRecipient = recipient.toUtf8();
            QByteArray byteArray = str.toUtf8();

            socketStream << byteRecipient;
            socketStream << byteArray;

            ui->lineEdit_message->clear();
        }
        else
            QMessageBox::critical(this,"Client","Сокет не был открыт");
    }
    else
        QMessageBox::critical(this,"Client","Нет соединения");
}

void MainWindow::displayMessage(const QString& str)
{
    ui->textBrowser_receivedMessages->append(str);
}

void MainWindow::refreshComboBox(){
    ui->comboBox_receiver->clear();
    ui->comboBox_receiver->addItem("Broadcast");
    foreach(const QString& users_socket, connection_set)
        ui->comboBox_receiver->addItem(users_socket);
}
