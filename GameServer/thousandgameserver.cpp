/**
* @file thousandgameserver.cpp
* @author Kharkunov Eugene
* @date 4.06.2011
* @brief ���� �������� ���������� ������� ������ ThousandGameServer
*/

#include "thousandgameserver.h"
#include "config.h"
#include "thousandgamequeryhandler.h"
#include "gamethousand.h"
#include <QMessageBox>
#include <QSqlError>
#include <QDataStream>
#include <QFile>
#include <QMutex>

ThousandGameServer* ThousandGameServer::_mInstance = 0;

ThousandGameServer* ThousandGameServer::getInstance() {
    if (!_mInstance)
        _mInstance = new ThousandGameServer("Game: 1000", Config::portsForGameServers.value("Game: 1000"));
    return _mInstance;
}

void ThousandGameServer::destroy() {
}

ThousandGameServer::ThousandGameServer(QString name, int port, QObject *parent) :
    AbstractGameServer(name, port, parent),
    _mName(name),
    _mPort(port),
    state(AbstractGameServer::NotRunning),
    _mTimeStart(),
    _mTimer(0)
{
    //���������� ��� ���������� ��� ������� �������� ��
    //! TODO: ������� ����������� �� ����� ������� ����
    databaseNames << "1000_UserInformation.sqlite";
    _mManager = new ConnectionManager();
    connect(this, SIGNAL(connectionAborted(QTcpSocket*)), _mManager, SLOT(removeConnection(QTcpSocket*)));
    requestHandler = new ThousandGameQueryHandler(this);
    connect(requestHandler, SIGNAL(sendingDataChanged(quint16,QByteArray,QByteArray)),
            this, SLOT(sendData(quint16,QByteArray,QByteArray)), Qt::BlockingQueuedConnection);
    packInit();//�������������� ������ ����
}

ThousandGameServer::~ThousandGameServer() {
    delete _mManager;
    delete requestHandler;
    delete _mTimer;
    databaseNames.clear();
}

AbstractGameServer::States ThousandGameServer::serverState() const {
    return state;
}

QString ThousandGameServer::serverName() const {
    return _mName;
}

QDateTime ThousandGameServer::startTime() const {
    return _mTimeStart;
}

quint64 ThousandGameServer::runningTime() const {
    if (_mTimer)
        return _mTimer->elapsed();
    else return 0;
}

bool ThousandGameServer::startServer() {
    bool isStarted = listen(QHostAddress::Any, _mPort);
    if (!isStarted) {//���������, ������ �� ������ �����
        QMessageBox::critical(0,
                              tr("Game server error"),
                              tr("Unable to start the server:") + errorString());
        close();
        return false;
    }
    else {

        bool isDatabasesInit = initDatabases();
        if (!isDatabasesInit) {//���������, ������ �� ������������� ���� �� �������
            close();
            return false;
        }
    }
    connect(this, SIGNAL(newConnection()), this, SLOT(addNewConnection()));
    state = AbstractGameServer::Running;
    _mTimeStart = QDateTime::currentDateTime();
    _mTimer = new QTime();
    _mTimer->start();
    emit(stateChanged());
    return true;
}

void ThousandGameServer::stopServer() {
    disconnectDatabases();
    _mManager->closeAllConnections();
    state = AbstractGameServer::NotRunning;
    emit(stateChanged());
    delete _mTimer;
    _mTimer = 0;
    close();
}

bool ThousandGameServer::initDatabases() {
    QSqlDatabase tempDB;
    QList<QString>::iterator it = databaseNames.begin();
    for (; it != databaseNames.end(); ++it) {
        bool fileExist = QFile::exists(Config::pathDatabases.absolutePath() + "/" + *it);
        if (!fileExist) {//��������� �� ������� ������������� ����� ��
            QMessageBox::warning(0,
                                 tr("Warning"),
                                 tr("Database ") + *it + tr(" not found!"));
            return false;
        }
        else {
            tempDB = QSqlDatabase::addDatabase("QSQLITE", *it);
            tempDB.setDatabaseName(Config::pathDatabases.absolutePath() + "/" + *it);
            bool isOpened = tempDB.open();
            if (!isOpened) {//������� �� ������� �� ��� ������
                QMessageBox::critical(0,
                                      tr("Database initialization error"),
                                      tr("Unable to initialization a ")  + *it + "\n"
                                      + tempDB.lastError().text());
                return false;
            }
            else {
                mapName2Database[*it] = tempDB;
            }
        }
    }
    emit(newServerMessage("Database OK!"));
    return true;
}

void ThousandGameServer::disconnectDatabases() {
    QMap<QString, QSqlDatabase>::iterator it = mapName2Database.begin();
    for (; it != mapName2Database.end(); ++it) {
        it.value().close();
        QSqlDatabase::removeDatabase(it.key());
    }
    mapName2Database.clear();
}

void ThousandGameServer::addRequestQuery() {
    QTcpSocket *socket = static_cast<QTcpSocket*>(sender());
    // ��������� ��������� ������
    if (_mManager->socketState(socket) != WaitForQueryTransmission) return;
    quint16 blockSize = 0;//������ ����� ������������ ������
    quint16 requestSize = sizeof(QueryStruct) - 2 * sizeof(quint16);//���������� ����, ������� ��� ���� �������
    QueryStruct requestQuery;// ��������������� ������
    QByteArray incomingRequest;//������, ���� ����������� ������ �� ������
    QDataStream stream(socket);//����� ���������� ������
    while (requestSize) {
        blockSize = socket->bytesAvailable();
        if (blockSize > requestSize) blockSize = requestSize;
        if (!blockSize) {
            emit(newServerMessage(tr("Invalid size of query")));
            break;
        }
        char *buffer = new char[blockSize];
        stream.readRawData(buffer, blockSize);
        incomingRequest += QByteArray::fromRawData(buffer, blockSize);
        requestSize -= blockSize;
        blockSize = 0;
        delete []buffer;
    }
    QDataStream byteHandler(&incomingRequest, QIODevice::ReadOnly);
    byteHandler>>requestQuery;
    if (requestQuery.size != 0) {//� ������ �������, ����� �������� �� ��������� �������� ������ size == -1
        requestQuery.socketDescriptor = socket->socketDescriptor();
        Q_ASSERT(requestQuery.socketDescriptor != -1);
        //������ ������ � ������� ��������� ���� ����� ��� ������������(���� ��� ��������� � ���)
        switch (requestQuery.type) {
        case MOVE : {
            locker.lockForWrite();
            _mMoveQueries.append(requestQuery);
            locker.unlock();
            emit (moveListChanged());
            break;
        }
        case MESSAGE : {// ��������� ���������
            QString message = "";
            QByteArray incomingMessage;
            blockSize = 0;
            requestSize = requestQuery.size;
            while (requestSize) {
                blockSize = socket->bytesAvailable();
                if (blockSize > requestQuery.size) blockSize = requestQuery.size;
                if (!blockSize) {
                    emit(newServerMessage(tr("Invalid size of query")));
                    break;
                }
                char *buffer = new char[blockSize];
                stream.readRawData(buffer, blockSize);
                incomingMessage += QByteArray::fromRawData(buffer, blockSize);
                requestSize -= blockSize;
                blockSize = 0;
                delete []buffer;
            }
            QDataStream reader(&incomingMessage, QIODevice::ReadOnly);
            quint32 strSize = 0;
            reader>>strSize;
            message.resize(strSize);
            reader>>message;
            //���������� ����� �������� � ��� ������������
            QDateTime sendingTime = QDateTime::currentDateTime();
            QString userNick = _mManager->userNick(socket);
            QString resultMessage = "[" + sendingTime.time().toString("hh:mm") + "] " + userNick + "->" + message;
            QByteArray outcomingMessage;
            QDataStream writer(&outcomingMessage, QIODevice::WriteOnly);
            writer<<resultMessage.size();
            writer<<resultMessage;
            //������� ���� ������������ �������������
            QMutex mutex;
            mutex.lock();
            QList<UserDescription> list = _mManager->getUserList();
            for (int i = 0; i < list.size(); i++) {
                QTcpSocket *userSocket = list.at(i).socket;
                _mManager->setSocketState(userSocket, WaitForDataTransmission);
                requestQuery.socketDescriptor = socket->socketDescriptor();
                QByteArray outcomingRequest;
                QDataStream outStream(&outcomingRequest, QIODevice::WriteOnly);
                requestQuery.size = outcomingMessage.size();
                outStream<<requestQuery;
                userSocket->write(outcomingRequest);
                _mManager->setSocketState(socket, WaitForQueryTransmission);
                userSocket->write(outcomingMessage);
            }
            mutex.unlock();
            break;
        }
        default : {
            locker.lockForWrite();
            _mRequestQueries.append(requestQuery);
            locker.unlock();
            if (!requestHandler->isRunning()) requestHandler->start(QThread::HighPriority);
        }
        }
        if (requestQuery.size != -1 && requestQuery.type != MESSAGE) {
            locker.lockForWrite();
            _mManager->setSocketState(socket, WaitForDataTransmission);
            locker.unlock();
        }
    }
}
void ThousandGameServer::sendToClient(QByteArray &array, QTcpSocket *socket) {
    socket->write(array);
}

void ThousandGameServer::addNewConnection() {
    QTcpSocket *socket = nextPendingConnection();
    if(!socket) return;
    _mManager->addConnection(socket);
    connect(socket, SIGNAL(disconnected()), this, SLOT(slotConnectionAborted()));
    connect(socket, SIGNAL(disconnected()), socket, SLOT(deleteLater()));
    connect(socket, SIGNAL(readyRead()), this, SLOT(addRequestQuery()));
    //! TODO: ������� ������� ���������� � ��������� �������
    emit(newServerMessage("New client connected"));
}

void ThousandGameServer::slotConnectionAborted() {
    QTcpSocket *socket = static_cast<QTcpSocket*>(sender());
    emit(connectionAborted(socket));
}

GameThousand* ThousandGameServer::findGame(QList<GameThousand *> *list, quint16 ID) {
    QList<GameThousand*>::iterator it = list->begin();
    for (; it != list->end(); ++it) {
        GameThousand *game = *it;
        if (game->gameID() == ID)
            return game;
    }
    return 0;
}

bool ThousandGameServer::createNewGame(UserDescription creater, GameSettings settings) {
    GameThousand *game = new GameThousand(creater, settings.playersNumber, settings.timeout);
    listNewGame.append(game);
    return true;
}

bool ThousandGameServer::connectToGame(quint16 gameID, UserDescription user) {
    GameThousand *game = findGame(&listNewGame, gameID);
    if (game)
        return game->addPlayer(user);
    else return false;
}

void ThousandGameServer::setServerPort(quint16 port) {
    _mPort = port;
}

void ThousandGameServer::sendData(quint16 descriptor, QByteArray query, QByteArray data) {
    QTcpSocket *socket = _mManager->findSocket(descriptor);
    _mManager->setSocketState(socket, WaitForQueryTransmission);
    socket->write(query);
    _mManager->setSocketState(socket, WaitForDataTransmission);
    socket->write(data);
    _mManager->setSocketState(socket, WaitForQueryTransmission);
}
