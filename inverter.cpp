#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "inverter.h"
#include "tools.h"
#include "main.h"

#include <termios.h>

cInverter::cInverter(std::string devicename) {
    device = devicename;
    status1[0] = 0;
    status2[0] = 0;
    warnings[0] = 0;
    mode = 0;
}

string *cInverter::GetQpigsStatus() {
    m.lock();
    string *result = new string(status1);
    m.unlock();
    return result;
}

string *cInverter::GetQpiriStatus() {
    m.lock();
    string *result = new string(status2);
    m.unlock();
    return result;
}

string *cInverter::GetWarnings() {
    m.lock();
    string *result = new string(warnings);
    m.unlock();
    return result;
}

void cInverter::SetMode(char newmode) {
    m.lock();
    if (mode && newmode != mode)
        ups_status_changed = true;
    mode = newmode;
    m.unlock();
}

int cInverter::GetMode() {
    int result;
    m.lock();

    switch (mode) {
        case 'P': result = 1;   break;  // Power_On
        case 'S': result = 2;   break;  // Standby
        case 'L': result = 3;   break;  // Line
        case 'B': result = 4;   break;  // Battery
        case 'F': result = 5;   break;  // Fault
        case 'H': result = 6;   break;  // Power_Saving
        default:  result = 0;   break;  // Unknown
    }

    m.unlock();
    return result;
}

bool cInverter::query(const char *cmd) {
    time_t started;
    int fd;
    int i=0, n, replysize;

    fd = open(this->device.data(), O_RDWR | O_NONBLOCK);
    if (fd == -1) {
        lprintf("INVERTER: Unable to open device file (errno=%d %s)", errno, strerror(errno));
        sleep(5);
        return false;
    }


    // Once connected, set the baud rate and other serial config (Don't rely on this being correct on the system by default...)
    speed_t baud = B2400;

    // Speed settings (in this case, 2400 8N1)
    struct termios settings;
    tcgetattr(fd, &settings);

    cfsetspeed(&settings, baud);      // baud rate
    settings.c_cflag &= ~PARENB;       // no parity
    settings.c_cflag &= ~CSTOPB;       // 1 stop bit
    settings.c_cflag &= ~CSIZE;        // Clear all bits that set the data size
    settings.c_cflag |= CS8 | CLOCAL;  // 8 bits

    settings.c_oflag &= ~OPOST;        // Prevent special interpretation of output bytes (e.g. newline chars)
    settings.c_lflag &= ~ICANON;
    settings.c_iflag &= ~(IXON | IXOFF | IXANY); // Turn off s/w flow ctrl
    settings.c_lflag &= ~ISIG;
    settings.c_oflag &= ~ONLCR; // Prevent conversion of newline to carriage return/line feed
    settings.c_lflag &= ~ECHO;             // turn off Echo input characters.
    settings.c_lflag &= ~IEXTEN;           // disable implementation-defined input processing.
    settings.c_iflag &= ~(IGNBRK|BRKINT|PARMRK|ISTRIP|INLCR|IGNCR|ICRNL); // Disable any special handling of received bytes
    tcsetattr(fd, TCSANOW, &settings); // apply the settings
    tcflush(fd, TCOFLUSH);

    // ---------------------------------------------------------------

    // Generating CRC for a command
    uint16_t crc = cal_crc_half((uint8_t*)cmd, strlen(cmd));
    n = strlen(cmd);
    memcpy(&buf, cmd, n);
    lprintf("INVERTER: Current CRC: %X %X", crc >> 8, crc & 0xff);

    buf[n++] = crc >> 8;
    buf[n++] = crc & 0xff;
    buf[n++] = 0x0d;

    //send a command
    write(fd, &buf, n);
    time(&started);

    const int READ_BUFFER_SIZE = 15;
    bool reading = true;
    bool timeout = false;
    do {
        n = read(fd, (void*)buf+i, READ_BUFFER_SIZE);
        if (n < 0) {
            if (time(NULL) - started > 2) {
                timeout = true;
                lprintf("INVERTER: %s read timeout", cmd);
                break;
            } else {
                usleep(2000);
                continue;
            }
        } else {
           for (int j=i; j<i+n; j++) {
                if (buf[j] == 0x0d){
                    reading = false;
                    replysize = j+1;
                    lprintf("INVERTER: stop byte detected, buffersize might be %d for %s ", replysize, cmd);
                    break;
                }
            }

        }
        usleep(2000);

        i += n;
    } while (reading);
    close(fd);

    if (timeout) {
        lprintf("INVERTER: %s command timeout, or couldn't find stop byte. Byte read (%d bytes). Buffer: %s ", cmd, i, buf);
        return false;
    }

    lprintf("INVERTER: %s reply size (%d bytes)", cmd, replysize);

    if (buf[0]!='(') {
        lprintf("INVERTER: %s: incorrect start bytes.  Buffer: %s ", cmd, buf);
        return false;
    }
    if (!(CheckCRC(buf, replysize))) {
        lprintf("INVERTER: %s: CRC Failed!  Reply size: %d  Buffer: %s ", cmd, replysize, buf);
        return false;
    }

    buf[replysize-3] = '\0'; //nullterminating on first CRC byte
    lprintf("INVERTER: %s: %d bytes read: %s ", cmd, replysize, buf);

    lprintf("INVERTER: %s query finished", cmd);
    return true;
}

void cInverter::poll() {
    int n,j;
    extern const bool runOnce;

    while (true) {

        // Reading mode
        if (!ups_qmod_changed) {
            if (query("QMOD")) {
                SetMode(buf[1]);
                ups_qmod_changed = true;
            }
        }

        // reading status (QPIGS)
        if (!ups_qpigs_changed) {
            if (query("QPIGS")) {
                m.lock();
                strcpy(status1, (const char*)buf+1);
                m.unlock();
                ups_qpigs_changed = true;
            }
        }

        // Reading QPIRI status
        if (!ups_qpiri_changed) {
            if (query("QPIRI")) {
                m.lock();
                strcpy(status2, (const char*)buf+1);
                m.unlock();
                ups_qpiri_changed = true;
            }
        }

        // Get any device warnings...
        if (!ups_qpiws_changed) {
            if (query("QPIWS")) {
                m.lock();
                strcpy(warnings, (const char*)buf+1);
                m.unlock();
                ups_qpiws_changed = true;
            }
        }
        if (quit_thread) return;
        sleep(5);
        // leave after delay for main thread having time to printout data
        if (runOnce) {
            ups_leave = true;
            exit(0);
        }
    }
}

void cInverter::ExecuteCmd(const string cmd) {
    // Sending any command raw
    if (query(cmd.data())) {
        m.lock();
        strcpy(status2, (const char*)buf+1);
        m.unlock();
    }
}

uint16_t cInverter::cal_crc_half(uint8_t *pin, uint8_t len) {
    uint16_t crc;

    uint8_t da;
    uint8_t *ptr;
    uint8_t bCRCHign;
    uint8_t bCRCLow;

    uint16_t crc_ta[16]= {
        0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
        0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
    };

    ptr=pin;
    crc=0;

    while(len--!=0) {
        da=((uint8_t)(crc>>8))>>4;
        crc<<=4;
        crc^=crc_ta[da^(*ptr>>4)];
        da=((uint8_t)(crc>>8))>>4;
        crc<<=4;
        crc^=crc_ta[da^(*ptr&0x0f)];
        ptr++;
    }

    bCRCLow = crc;
    bCRCHign= (uint8_t)(crc>>8);

    if(bCRCLow==0x28||bCRCLow==0x0d||bCRCLow==0x0a)
        bCRCLow++;
    if(bCRCHign==0x28||bCRCHign==0x0d||bCRCHign==0x0a)
        bCRCHign++;

    crc = ((uint16_t)bCRCHign)<<8;
    crc += bCRCLow;
    return(crc);
}

bool cInverter::CheckCRC(unsigned char *data, int len) {
    uint16_t crc = cal_crc_half(data, len-3);
    return data[len-3]==(crc>>8) && data[len-2]==(crc&0xff);
}
