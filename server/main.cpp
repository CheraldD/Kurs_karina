#include "ui.h"
#include "server.h"
#include "error.h"
int main(int argc, char* argv[])
{
    UI interface (argc,argv);
    server serv (interface.get_port(),interface.get_seed(),interface.get_buff_size());
    serv.work();
    return 0;
}
