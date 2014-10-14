/*
 * SIM900Client.cpp
 *
 * Created on: Nov 13, 2013
 * Author: Vincent Wochnik
 * E-Mail: v.wochnik@gmail.com
 *
 * Copyright (c) 2013 Vincent Wochnik
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "SIM900Client.h"
#include <avr/pgmspace.h>

#define STATE_INACTIVE 0
#define STATE_ACTIVE 1
#define STATE_SETUP 2
#define STATE_IDLE 3
#define STATE_CONNECTED 4
#define STATE_CLOSED 5
#define STATE_EOT 6

SIM900Client::SIM900Client(uint8_t receivePin, uint8_t transmitPin, uint8_t powerPin) : _modem(receivePin, transmitPin), _pwrPin(powerPin), _state(STATE_INACTIVE)
{
    // set power pin to output to be able to set LOW and HIGH
    pinMode(powerPin, OUTPUT);
    _bufindex = _buflen = 0;
    _flowctrl = 1;
}

void SIM900Client::begin(int speed)
{
    uint8_t tries, res;
    unsigned long start;

    // begin software serial
    _modem.begin(speed);

    tries = 3;
    while ((_state == STATE_INACTIVE) && (tries-- > 0)) {
        if (sendAndAssert(F("AT"), F("OK"), 500, 10) == _S900_RECV_OK) {
            _state = STATE_SETUP;
            break;
        }
        digitalWrite(_pwrPin, HIGH);
        delay(1200);
        digitalWrite(_pwrPin, LOW);
        delay(6000);
    }
    if (_state < STATE_ACTIVE)
        return;
        
    // baud rate setting
    tries = 3;
    do {
        _modem.print(F("AT+IPR="));
        _modem.println(speed);
        _modem.flush();
        res = recvExpected(F("OK"), 1000);
    } while ((res != _S900_RECV_OK) && (--tries > 0));

    if (res != _S900_RECV_OK)
        return;

    // factory settings
    if (sendAndAssert(F("AT&F"), F("OK"), 1000, 3) != _S900_RECV_OK)
        return;

    // flow control
    if (sendAndAssert(F("AT+IFC=1,1"), F("OK"), 1000, 3) != _S900_RECV_OK)
        return;

    _state = STATE_SETUP;
}

uint8_t SIM900Client::pin(const char *pin)
{
    uint8_t res, tries;

    if (_state != STATE_SETUP)
        return 0;

    tries = 3;
    do {
        voidReadBuffer();
        _modem.print(F("AT+CPIN="));
        _modem.println(pin);
        _modem.flush();
        res = recvExpected(F("OK"), 5000);
    } while ((res != _S900_RECV_OK) && (--tries > 0));

    if (res != _S900_RECV_OK)
        return 0;
    return 1;
}

uint8_t SIM900Client::attach(const char *apn, const char *user, const char *pass)
{
    uint8_t tries, res;

    if (_state != STATE_SETUP)
        return 0;

    if (sendAndAssert(F("AT+CPIN?"), F("+CPIN: READY"), 1000, 3, 2000) != _S900_RECV_OK)
        return 0;

    if (sendAndAssert(F("AT+CIPSHUT"), F("SHUT OK"), 1000, 3) != _S900_RECV_OK)
        return 0;

    if (sendAndAssert(F("AT+CIPMODE=1"), F("OK"), 1000, 3, 5000) != _S900_RECV_OK)
        return 0;

    if (sendAndAssert(F("AT+CGATT=1"), F("OK"), 1000, 3, 5000) != _S900_RECV_OK)
        return 0;

    tries = 3;
    do {
        delay(5000);
        voidReadBuffer();
        _modem.print(F("AT+CSTT=\""));
        _modem.print(apn);
        _modem.print(F("\",\""));
        _modem.print(user);
        _modem.print(F("\",\""));
        _modem.print(pass);
        _modem.println(F("\""));
        _modem.flush();
        res = recvExpected(F("OK"), 1000); 
    } while ((res != _S900_RECV_OK) && (--tries > 0));

    if (res != _S900_RECV_OK)
        return 0;

    if (sendAndAssert(F("AT+CIICR"), F("OK"), 2000, 3, 5000) != _S900_RECV_OK)
        return 0;

    delay(1000);
    if (sendAndAssert(F("AT+CIFSR"), F("ERROR"), 1000, 3) == _S900_RECV_OK)
        return 0;

    _state = STATE_IDLE;
    return 1;
}

int SIM900Client::connect(IPAddress ip, uint16_t port)
{
    uint8_t res, tries;

    if (_state != STATE_IDLE)
        return 0;

    tries = 3;
    do {
        voidReadBuffer();
        _modem.print(F("AT+CIPSTART=\"TCP\",\""));
        _modem.print(ip);
        _modem.print(F("\",\""));
        _modem.print(port);
        _modem.println(F("\""));
        _modem.flush();
        res = recvExpected(F("OK"), 1000);
        if (res != _S900_RECV_OK)
            continue;
        res = recvExpected(F("CONNECT"), 60000);
        delay(500);
    } while ((res != _S900_RECV_OK) && (--tries > 0));
    if (res == _S900_RECV_OK) {
        _state = STATE_CONNECTED;
        return 1;
    }
    return 0;
}

int SIM900Client::connect(const char *host, uint16_t port)
{
    uint8_t res, tries;

    if (_state != STATE_IDLE)
        return 0;

    tries = 3;
    do {
        voidReadBuffer();
        _modem.print(F("AT+CIPSTART=\"TCP\",\""));
        _modem.print(host);
        _modem.print(F("\",\""));
        _modem.print(port);
        _modem.println(F("\""));
        _modem.flush();
        res = recvExpected(F("OK"), 1000);
        if (res != _S900_RECV_OK)
            continue;
        res = recvExpected(F("CONNECT"), 60000);
        delay(500);
    } while ((res != _S900_RECV_OK) && (--tries > 0));
    if (res == _S900_RECV_OK) {
        _state = STATE_CONNECTED;
        return 1;
    }
    return 0;
}

size_t SIM900Client::write(uint8_t w)
{
    if (_state != STATE_CONNECTED)
        return 0;
    return _modem.write(w);
}

size_t SIM900Client::write(const uint8_t *buf, size_t size)
{
    if (_state != STATE_CONNECTED)
        return 0;
    return _modem.write(buf, size);
}

void SIM900Client::flush()
{
    if (_state != STATE_CONNECTED)
        return;
    _modem.write(0x1a);
    _modem.flush();
}

void SIM900Client::stop()
{
    if (_flowctrl == 0) {
        _flowctrl = 1;
        _modem.write(0x11);//XON
        _modem.flush();
    }
    if (_state == STATE_CONNECTED) {
        do {
            _buflen = 0;
        } while (fillBuffer());
        if (_state == STATE_CONNECTED) {
            delay(1000);
            _modem.print(F("+++"));
            delay(500);
            sendAndAssert(F("AT+CIPCLOSE"), F("OK"), 1000, 3);
        }
    }
    _buflen = _bufindex = 0;
    _flowctrl = 1;
    _state = STATE_IDLE;
}

int SIM900Client::available()
{
    if ((_state < STATE_CONNECTED) || (_state == STATE_EOT))
        return -1;
    if (_buflen < 10)
        fillBuffer();
        return _buflen;
}

int SIM900Client::read() { 
    int p = peek();
    if (p < 0)
        return p;

    --_buflen;
    return p; 
}

int SIM900Client::read(uint8_t *buf, size_t size)
{
    int ret = 0, r;
    while ((r = read()) >= 0)
        buf[ret++] = r;
    return ret;
}

int SIM900Client::peek()
{
    size_t i;

    if ((_state < STATE_CONNECTED) || (_state == STATE_EOT))
        return -1;
    if ((_buflen < 10) && (fillBuffer() == 0))
        return -1;
    i = _S900_READ_BUFFER_SIZE + _bufindex - _buflen;

    if (i >= _S900_READ_BUFFER_SIZE)
        i -= _S900_READ_BUFFER_SIZE;

    return _buf[i]; 
}

uint8_t SIM900Client::connected()
{
    return ((_state >= STATE_CONNECTED) && (_state < STATE_EOT));
}

SIM900Client::operator bool()
{
    return _state >= STATE_SETUP;
}

void SIM900Client::voidReadBuffer()
{
    while (_modem.available())
        _modem.read();
}

uint8_t SIM900Client::sendAndAssert(const __FlashStringHelper* cmd, const __FlashStringHelper* exp, uint16_t timeout, uint8_t tries, uint16_t failDelay)
{
    uint8_t res;
    do {
        voidReadBuffer();
        _modem.println(cmd);
        _modem.flush();
        res = recvExpected(exp, timeout);
        if (res != _S900_RECV_OK)
            delay(failDelay);
    } while ((res != _S900_RECV_OK) && (--tries > 0));
}

uint8_t SIM900Client::recvExpected(const __FlashStringHelper *exp, uint16_t timeout)
{
    uint8_t ret = _S900_RECV_RETURN;
    unsigned long start;
    const char PROGMEM *ptr = (const char PROGMEM*)exp;
    char c = pgm_read_byte(ptr), r;

    while ((ret != _S900_RECV_OK) && (ret != _S900_RECV_INVALID)) {
        start = millis();
        while ((!_modem.available()) && (millis()-start < timeout));
        if (!_modem.available())
            return _S900_RECV_TIMEOUT;
        
        r = _modem.read();
        switch (ret) {
        case _S900_RECV_RETURN:
            if (r == '\n')
                ret = _S900_RECV_READING;
            else if (r != '\r')
                ret = _S900_RECV_ECHO;
            break;
        case _S900_RECV_ECHO:
            if (r == '\n')
                ret = _S900_RECV_RETURN;
            break;
        case _S900_RECV_READING:
            if (r == c) {
                c = pgm_read_byte(++ptr);
                if (c == 0) {
                    ret = _S900_RECV_READ;
                }
            } else {
                ret = _S900_RECV_NO_MATCH;
            }
            break;
        case _S900_RECV_READ:
            if (r == '\n')
                ret = _S900_RECV_OK;
            break;
        case _S900_RECV_NO_MATCH:
            if (r == '\n')
                ret = _S900_RECV_INVALID;
            break;
        }
    }
    return ret;
}

size_t SIM900Client::fillBuffer()
{
    unsigned long start;

    if ((_buflen == 0) && (_state == STATE_CLOSED))
        _state = STATE_EOT;

    if (_state != STATE_CONNECTED)
        return _buflen;

    while (_buflen < _S900_READ_BUFFER_SIZE) {
        if (_modem.available() > _SS_MAX_RX_BUFF-16) {
            if (_buflen+8 == _S900_READ_BUFFER_SIZE) {
                _modem.write(0x13);
                _modem.flush();
                _flowctrl = 0;
            }
        } else if (_modem.available() < 8) {
            if (_flowctrl == 0) {
                _flowctrl = 1;
                _modem.write(0x11);//XON
                _modem.flush();
            }

            start = millis();
            while ((!_modem.available()) && (millis() - start < 50));
        }

        if (!_modem.available())
            break;

        _buf[_bufindex++] = _modem.read();
        if (_bufindex == _S900_READ_BUFFER_SIZE)
            _bufindex = 0;
        _buflen++;
    }

    if (detectClosed()) {
        if (_buflen)
            _state = STATE_CLOSED;
        else
            _state = STATE_EOT;
    }

    return _buflen;
}

uint8_t SIM900Client::detectClosed()
{
    const char PROGMEM *find = PSTR("\r\nCLOSED\r\n");
    const char PROGMEM *ptr;
    size_t len, i; char c;

    for (len = _buflen; len >= 10; --len) {
        i = _S900_READ_BUFFER_SIZE + _bufindex - len;
        if (i >= _S900_READ_BUFFER_SIZE)
            i -= _S900_READ_BUFFER_SIZE;
        ptr = find;
        while ((c = pgm_read_byte(ptr++)) > 0) {
            if (_buf[i++] != c)
                break;
            if (i == _S900_READ_BUFFER_SIZE)
                i = 0;
        }
        if (c > 0)
            continue;

        // cut the CLOSED off
        _buflen -= len;
        _bufindex += _S900_READ_BUFFER_SIZE - len;
        if (_bufindex >= _S900_READ_BUFFER_SIZE)
            _bufindex -= _S900_READ_BUFFER_SIZE;

        return 1;
    }
    return 0;
}

uint8_t SIM900Client::sendQuery(const __FlashStringHelper* cmd,
	const __FlashStringHelper* exp1,
	const __FlashStringHelper* exp2,
	uint8_t *buf,
	size_t size,
	uint16_t timeout,
	uint8_t tries,
	uint16_t failDelay)
{
    uint8_t res;
    do {
	voidReadBuffer();
	_modem.println(cmd);
	_modem.flush();
	res = recvQuery(exp1, exp2, buf, size, timeout);
	if (res != _S900_RECV_OK)
	    delay(failDelay);
    } while ((res != _S900_RECV_OK) && (--tries > 0));
    return res;
}

uint8_t SIM900Client::recvQuery(const __FlashStringHelper* exp1,
	const __FlashStringHelper* exp2,
	uint8_t *buf,
	size_t size,
	uint16_t timeout)
{
    uint8_t ret = _S900_RECV_RETURN;
    unsigned long start;
    const char PROGMEM *ptr = (const char PROGMEM*)exp1;
    char c = pgm_read_byte(ptr), r;
    int i = 0;
    bool first = true;

    if (size == 0)
	return 0;

    while ((ret != _S900_RECV_OK) && (ret != _S900_RECV_INVALID)) {
	start = millis();
	while ((!_modem.available()) && (millis()-start < timeout));
	if (!_modem.available())
	    return _S900_RECV_TIMEOUT;

	r = _modem.read();
	switch (ret) {
	    case _S900_RECV_RETURN:
		if (r == '\n') {
		    if (first && c == 0) {
			ret = _S900_RECV_READ;
		    }
		    else
		    ret = _S900_RECV_READING;
		}
		else if (r != '\r')
		    ret = _S900_RECV_ECHO;
		break;
	    case _S900_RECV_ECHO:
		if (r == '\n')
		    ret = _S900_RECV_RETURN;
		break;
	    case _S900_RECV_READING:
		if (r == c) {
		    c = pgm_read_byte(++ptr);
		    if (c == 0) {
			ret = _S900_RECV_READ;
		    }
		} else {
		    ret = _S900_RECV_NO_MATCH;
		}
		break;
	    case _S900_RECV_READ:
		if (first) {
		    if (r == '\n' || r == '\r' || i >= size)
		    {
			first = false;
			ptr = (const char PROGMEM*)exp2;
			c = pgm_read_byte(ptr);
			buf[i] = 0x00;
			ret = _S900_RECV_ECHO;
		    }
		    else
			buf[i++] = r;
		}
		else if (r == '\n') {
		    ret = _S900_RECV_OK;
		}
		break;
	    case _S900_RECV_NO_MATCH:
		if (r == '\n')
		    ret = _S900_RECV_INVALID;
		break;
	}
    }
    return ret;
}

int SIM900Client::getIMEI(uint8_t *buf, size_t size)
{
    if(!(_state == STATE_SETUP || _state == STATE_IDLE))
	return 0;
    if (sendQuery(F("AT+GSN"), F(""), F("OK"), buf, size, 1000, 3, 5000) != _S900_RECV_OK)
	return 0;
    return 1;
}

int SIM900Client::setupClock()
{
    if(!(_state == STATE_SETUP || _state == STATE_IDLE))
	return 0;
    if (sendAndAssert(F("AT+CLTS=1"), F("OK"), 1000, 3, 5000) != _S900_RECV_OK)
	return 0;
    return 1;
}

int SIM900Client::getClock(uint8_t *buf, size_t size)
{
    if(!(_state == STATE_SETUP || _state == STATE_IDLE))
	return 0;

    if (sendQuery(F("AT+CCLK?"), F("+CCLK: "), F("OK"), buf, size, 1000, 3, 5000) != _S900_RECV_OK)
	return 0;
    return 1;
}
