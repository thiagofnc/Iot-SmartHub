#ifndef WEBSERVERMANAGER_H
#define WEBSERVERMANAGER_H

#include <Arduino.h>

class WebServerManager {
private:
    String ip;
    int port;

public:
    WebServerManager();
    void init();
    void sendData(String data);
    void getRequest();
};

#endif // WEBSERVERMANAGER_H
