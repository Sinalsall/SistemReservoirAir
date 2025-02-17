#ifndef WEBSERVERHANDLER_H
#define WEBSERVERHANDLER_H

#include <WebServer.h>

class WebServerHandler {
public:
    WebServerHandler();
    void begin();
    void handleClient();
    
private:
    WebServer server;  // Instance web server pada port 80

    String generateHTML();
    String getSensorDataJSON();

    void handleRoot();
    void handleSensor();
    void handleControl();
};

#endif // WEBSERVERHANDLER_H
