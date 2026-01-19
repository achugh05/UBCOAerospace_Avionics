#include <iostream>
#include <termios.h>

#include <Serial.h>

void callbackTest(std::string buf) {
    std::cout << "Test Json Received: "<< buf << std::endl;
}

int main () {
    Serial* s = new Serial("/dev/pts/4", B9600);

    s->setCallback(callbackTest);

    while (true);
}