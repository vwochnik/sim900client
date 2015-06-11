# SIM900Client library

> An Arduino Client implementation for the SIM900 GPRS module

## Installation

Just place the SIM900Client folder inside your Arduino library folder and restart Arduino.

## Features

1. One file. This library is very portable and leightweight.
2. Arduino Client interface compatible. Can be used with HttpClient.
3. Uses software flow control for data transport.

## Usage

1. Include the SoftwareSerial library as well as the SIM900Client library in your arduino sketch.

        #include <SoftwareSerial.h>
        #include <SIM900Client.h>

2. Create a global instance variable. Parameters are the receive pin, send pin and power pin respectively. Make sure that pins are correct. This library will only work with software serial. In the future, I may extend with hardware serial support.

        SIM900Client client(7, 8, 9);

3. Set up the GPRS module. Make sure that the baud rate matches the GPRS module's. If autobaud is activated, there will be no problems.

        client.begin(9600);

4. Prevent errors by freezing when there's an error.

        if (!client) {
            Serial.println(F("SIM900 could not be initialized. Check power and baud rate."));
            for (;;);
        }

5. Attach GPRS. Parameters are APN, username and password respectively.

        if (!client.attach("m2mdata", "", "")) {
            Serial.println(F("Could not attach GPRS."));
            for (;;);
        }

6. You can now start making tcp connections. Don't forget to call stop() after every connection.

        client.connect("arduino.cc", 80);
        client.println("GET /asciilogo.txt HTTP/1.0");
        client.println("Host: arduino.cc");
        client.println();
        client.flush();
        while (client.connected()) {
            if (client.available())
                Serial.print((char)client.read());
        }
        client.stop();

## Known Issues

1. You cannot pause and then print +++ to the client. This will end transparent mode.
2. You cannot write anything while not connected. This will send the data to the GPRS modem directly.
3. A HTTP response containing \r\nCLOSED\r\n will void the connection.

## Further Questions

Please create an issue if you have any questions or want to report a bug.
