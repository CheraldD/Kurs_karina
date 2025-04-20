#pragma once
#include <stdexcept>
#include <string>
/** Класс ошибок
*  Используется для отлова специфических ошибок, возникающих в ходе работы модулей
*  В конструкторе указывается строка с сообщением ошибки
*/
class critical_error:public std::runtime_error{
    public:
    /** Конструктор ошибки
    * s Сообщение об ошибке
    */
    critical_error(const std::string& s):std::runtime_error(s){}
};