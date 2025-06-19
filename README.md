gitkf - gitk-fast
==

`gitk` is awesome, but performance is not good for huge repository. This project is trying to provide a gitk like tool and resolve the performance pain point.

### Build

Currently, only works on windows, make sure you have Visual Studio 2022 and cmake installed, then run:

    git clone --recursive https://github.com/xieyubo/gitk-fast.git
    cd gitk-fast
    mkdir build
    cd build
    cmake ..
    cmake --build .

### Run

Just go to a repro, run:

    gitkf

or

    gitkf .

or

    gitkf <subfolder or file>

### Screenshot

![](screenshot/screenshot-1.png)