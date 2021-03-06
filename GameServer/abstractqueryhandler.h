/**
* @file abstractqueryhandler.h
* @author Kharkunov Eugene
* @date 6.06.2011
* @brief �������� �������� ������������� ������ ��� ����������� ��������
*/

#ifndef ABSTRACTQUERYHANDLER_H
#define ABSTRACTQUERYHANDLER_H

#include <QThread>

/**
* @class AbstractQueryHandler
* @brief ����� ��������� ������� ��������, ����������� �� ������. ����������� � ��������� ������
*/
class AbstractQueryHandler : public QThread
{
    Q_OBJECT
public:
    /**
    * @brief ����������� �����������
    * @param parent ��������� �� ������������ ������
    */
    explicit AbstractQueryHandler(QObject *parent = 0);

    //! ����������� ����������
    virtual ~AbstractQueryHandler();

    /**
    * @brief �������� ���, ������� ���������� ��������� ��� ������� ������
    *
    * ���������� ����������� � ����������� �������
    */
    virtual void run() = 0;
signals:

public slots:

};

#endif // ABSTRACTQUERYHANDLER_H
