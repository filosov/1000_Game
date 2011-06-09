/**
* @file thousandgamequeryhandler.h
* @author Kharkunov Eugene
* @date 9.06.2011
* @brief ���� �������� �������� ������ ��������� ������� �������� ��� �������� ������� "������"
*/

#ifndef THOUSANDGAMEQUERYHANDLER_H
#define THOUSANDGAMEQUERYHANDLER_H

#include "abstractqueryhandler.h"

class ThousandGameServer;
class ThousandGameDataParser;

/**
* @class ThousandGameQueryHandler
* @brief ����� ��������� ������� ��������, ����������� �� ������� ������. ����������� � ��������� ������.
* @sa AbstractQueryHandler
*/
class ThousandGameQueryHandler : public AbstractQueryHandler
{
    Q_OBJECT
public:
    /**
    * @brief ����������� �����������
    * @param parentServer   ��������� �� ������ �������� ������� "������"
    * @param parent         �r������� �� ������������ ������
    */
    explicit ThousandGameQueryHandler(ThousandGameServer *parentServer, QObject *parent = 0);

    /**
    * @brief ����������� ����������
    */
    ~ThousandGameQueryHandler();

    /**
    * @brief ������������ ������� �������� �� ��� ���, ���� ��� �� ������ ������
    * @sa ThousandGameServer::_mRequestQueries
    */
    void run();
private:
    //! ��������� �� ������ �������� �������, ������� �������� �������� ���������� ����������
    ThousandGameServer *server;
    //! �r������� �� ���������� ������
    ThousandGameDataParser *parser;
};

#endif // THOUSANDGAMEQUERYHANDLER_H
