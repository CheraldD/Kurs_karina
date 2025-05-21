#pragma once           
#include <stdexcept> 
#include <string>     
// Собственное исключение клиента, расширяет стандартное runtime_error
class speciphic_error : public std::runtime_error {
public:
    // Конструктор принимает текст ошибки и передаёт его базовому классу
    speciphic_error(const std::string& s)
      : std::runtime_error(s)
    {}
};
