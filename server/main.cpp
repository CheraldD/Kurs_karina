#include "ui.h"
#include "server.h"
#include "error.h"
int main(int argc, char* argv[])
{
    try{
        UI interface (argc,argv);
        server serv {interface.get_port(),interface.get_seed(),interface.get_buff_size(),interface.get_interval(),interface.get_min(),interface.get_max()};
        serv.run();
    }catch (po::error& e) {
        std::cout << e.what() << std::endl;
    }
    catch(server_error &e){
        std::cout<<"Критическая ошибка: "<<e.what()<<std::endl;
    }
    return 0;
}
