#pragma once           
#include <stdexcept> 
#include <string>     
// Собственное исключение сервера, расширяет стандартное runtime_error
class server_error : public std::runtime_error {
public:
    // Конструктор принимает текст ошибки и передаёт его базовому классу
    server_error(const std::string& s)
      : std::runtime_error(s)
    {}
};
