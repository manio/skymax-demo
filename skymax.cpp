#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "skymax.h"
#include "tools.h"
#include "main.h"

cSkymax::cSkymax(std::string devicename)
{
  device = devicename;
  status[0] = 0;
  mode = 0;
}

string *cSkymax::GetStatus()
{
  m.lock();
  string *result = new string(status);
  m.unlock();
  return result;
}

void cSkymax::SetMode(char newmode)
{
  m.lock();
  if (mode && newmode != mode)
    ups_status_changed = true;
  mode = newmode;
  m.unlock();
}

int cSkymax::GetMode()
{
  int result;
  m.lock();
  switch (mode)
  {
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

bool cSkymax::query(const char *cmd)
{
  time_t started;
  int fd;
  int i=0, n;

  fd = open(this->device.data(), O_RDWR | O_NONBLOCK);  // device is provided by program arg (usually /dev/hidraw0)
  if (fd == -1)
  {
    lprintf("Unable to open device file (errno=%d %s)", errno, strerror(errno));
    sleep(5);
    return false;
  }

  // Generating CRC for a command
  uint16_t crc = cal_crc_half((uint8_t*)cmd, strlen(cmd));
  n = strlen(cmd);
  memcpy(&buf, cmd, n);
  buf[n++] = crc >> 8;
  buf[n++] = crc & 0xff;
  buf[n++] = 0x0d;

  // Send a command
  write(fd, &buf, n);
  time(&started);

  // Instead of using a fixed size for expected response length, lets find it
  // by searching for the first returned <cr> char instead.
  char *startbuf;
  char *endbuf;
  do
  {
    // According to protocol manual, it appears no query should ever exceed 150 byte size in response
    n = read(fd, (void*)buf+i, 150 - i);
    if (n < 0)
    {
      if (time(NULL) - started > 8)     // Wait 8 secs before timeout
      {
        lprintf("Skymax: %s read timeout", cmd);
        break;
      }
      else
      {
        usleep(10);
        continue;
      }
    }
    i += n;

    startbuf = (char *)&buf[0];
    endbuf = strchr(startbuf, '\r');
  } while (endbuf == NULL);     // Still haven't found end <cr> char as long as pointer is null
  close(fd);

  int replysize = endbuf - startbuf + 1;
  //printf("Found <cr> at: %d\n", replysize);
  
  if (buf[0]!='(' || buf[replysize-1]!=0x0d)
  {
    //lprintf("Skymax: %s: incorrect start/stop bytes", cmd);
    return false;
  }
  if (!(CheckCRC(buf, replysize)))
  {
    //lprintf("Skymax: %s: CRC Failed!!!!", cmd);
    return false;
  }
  buf[replysize-3] = '\0';      //nullterminating on first CRC byte
  //printf("Skymax: %s: %d bytes read: %s\n", cmd, i, buf);
  return true;
}

void cSkymax::poll()
{
  int n,j;

  while (true)
  {
    // Reading mode
    if (query("QMOD"))
      SetMode(buf[1]);

    // Reading status
    if (query("QPIGS"))
    {
      m.lock();
      strcpy(status, (const char*)buf+1);
      m.unlock();
      ups_data_changed = true;
    }

    sleep(10);
  }
}

void cSkymax::ExecuteCmd(const string cmd)
{
  // Sending any command raw
  if (query(cmd.data()))
  {
    m.lock();
    strcpy(status, (const char*)buf+1);
    m.unlock();
  }
}

uint16_t cSkymax::cal_crc_half(uint8_t *pin, uint8_t len)
{
  uint16_t crc;

  uint8_t da;
  uint8_t *ptr;
  uint8_t bCRCHign;
  uint8_t bCRCLow;

  uint16_t crc_ta[16]=
  {
    0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
    0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef
  };
  ptr=pin;
  crc=0;

  while(len--!=0)
  {
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

bool cSkymax::CheckCRC(unsigned char *data, int len)
{
  uint16_t crc = cal_crc_half(data, len-3);
  return data[len-3]==(crc>>8) && data[len-2]==(crc&0xff);
}
