#include <iostream>
#include <termios.h>

#include <NetworkLink.h>

void callbackTest(std::string buf) {
    std::cout << "Test Json Received: "<< buf << std::endl;
}

int main () {
    // Serial* s = new Serial("/dev/pts/4", B9600);
    //
    // s->setCallback(callbackTest);
    //
    // while (true);
    
    NetworkLink clientTest(Protocol::UDP,
                            "127.0.0.1", 5001,
                            "127.0.0.1", 5000,
                            false);

    clientTest.setCallback(callbackTest);
    while (true);
}