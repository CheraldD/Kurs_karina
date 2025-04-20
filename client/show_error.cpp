#include "show_error.h"
int show_error::show_error_information(std::string function, std::string data, std::string type)
{
    // Генерируем исключение типа client_error, передавая строку с описанием:
    // имя функции / описание данных / тип ошибки
    throw client_error(function + " / " + data + " / " + type);
    return 0;
}
