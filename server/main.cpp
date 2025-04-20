#include "ui.h"
#include "communicator.h"
#include "error.h"
int main(int argc, char* argv[])
{
    UI interface (argc,argv);
    communicator server (interface.get_port(),interface.get_seed(),interface.get_buff_size());
    server.work();
    return 0;
}
