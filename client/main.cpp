#include "ui.h"
#include "client.h"
#include "application_protocol.h"
#include "error.h"
int main(int argc, char* argv[])
{
    try{
        UI interface (argc,argv);
        client CL;
        CL.run(interface);
    }catch (po::error& e) {
        std::cout << e.what() << std::endl;
    }
    catch(speciphic_error &e){
        std::cout<<"Критическая ошибка: "<<e.what()<<std::endl;
    }
    return 0;
}
