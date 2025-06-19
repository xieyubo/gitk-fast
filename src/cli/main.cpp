#include <iostream>

import gitkf;

int main(int argc, char* argv[])
{
    try {
        return gitkf_main(argc, argv);
    } catch (std::exception ex) {
        std::cerr << "Exception: " << ex.what() << std::endl;
        return 1;
    }
}