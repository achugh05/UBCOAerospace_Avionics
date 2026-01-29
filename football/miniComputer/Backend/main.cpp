#include <iostream>
#include <termios.h>

#include <NetworkLink.h>
#include <unistd.h>
#include <Logger.h>

Logger logger = Logger("./", "test");

void callbackTest(std::string buf) {
    std::cout << "Test Json Received: "<< buf << std::endl;
    logger.println(VerbosityLevel::INFO, SubsystemTag::LOGGING, "Test Json Received: "+buf);
}

int main () {
    // Serial* s = new Serial("/dev/pts/4", B9600);
    //
    // s->setCallback(callbackTest);
    //
    // while (true);
    
    NetworkLink clientTest(Protocol::TCP,
                            "127.0.0.1", 5000,
                            "0.0.0.0", 0,
                            true);

    clientTest.setCallback(onJsonReceive, callbackTest);
    clientTest.setCallback(onConnection,
        [](std::string _) {
            std::cout<<"Successfully Connected"<<std::endl;
            logger.println(VerbosityLevel::WARNING, SubsystemTag::LOGGING, "Successfully Connected");
    });
    clientTest.setCallback(onDisconnection,
        [](std::string _) {
            std::cout<<"Unexpected disconnection occured, attempting recconnection"<<std::endl;
            logger.println(VerbosityLevel::FAULT, SubsystemTag::LOGGING, "Unexpected disconnection occured, attempting recconnection");
    });
    sleep(10);
    std::string s ="test message: {test:test} \n";
    clientTest.sendData(s.c_str(),s.size());
    std::cout << "test Message Sent"<<std::endl;
    while (true);
}