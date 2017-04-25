#include <unistd.h>
#include <fcntl.h>
#include <string.h>
#include "skymax.h"
#include "tools.h"
#include "main.h"

cSkymax::cSkymax()
{
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

string *cSkymax::GetMode()
{
  string *result;
  m.lock();
  switch (mode)
  {
    case 'P': result = new string("Power On Mode");     break;
    case 'S': result = new string("Standby Mode");      break;
    case 'L': result = new string("Line Mode");         break;
    case 'B': result = new string("Battery Mode");      break;
    case 'F': result = new string("Fault Mode");        break;
    case 'H': result = new string("Power Saving Mode"); break;
    default:  result = new string("Unknown");           break;
  }
  m.unlock();
  return result;
}

bool cSkymax::query(const char *cmd, int replysize)
{
  time_t started;
  int fd;
  int i=0, n;

  fd = open("/dev/skymax", O_RDWR | O_NONBLOCK);
  if (fd == -1)
  {
    lprintf("Unable to open device file (errno=%d %s)", errno, strerror(errno));
    sleep(5);
    return false;
  }

  //generating CRC for a command
  uint16_t crc = cal_crc_half((uint8_t*)cmd, strlen(cmd));
  n = strlen(cmd);
  memcpy(&buf, cmd, n);
  buf[n++] = crc >> 8;
  buf[n++] = crc & 0xff;
  buf[n++] = 0x0d;

  //send a command
  write(fd, &buf, n);
  time(&started);

  do
  {
    n = read(fd, (void*)buf+i, replysize-i);
    if (n < 0)
    {
      if (time(NULL) - started > 2)
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
  } while (i<replysize);
  close(fd);

  if (i==replysize)
  {
    if (buf[0]!='(' || buf[replysize-1]!=0x0d)
    {
      lprintf("Skymax: %s: incorrect start/stop bytes", cmd);
      return false;
    }
    if (!(CheckCRC(buf, replysize)))
    {
      lprintf("Skymax: %s: CRC Failed!!!!", cmd);
      return false;
    }
    buf[i-3] = '\0'; //nullterminating on first CRC byte
    printf("Skymax: %s: %d bytes read: %s\n", cmd, i, buf);
    return true;
  }
  else
    lprintf("Skymax: %s reply too short (%d bytes)", cmd, i);
  return false;
}

void cSkymax::poll()
{
  int n,j;

  while (true)
  {
    // reading status (QPIGS)
    if (query("QPIGS", 110))
    {
      m.lock();
      strcpy(status, (const char*)buf+1);
      m.unlock();
      ups_data_changed = true;
    }

    // reading mode (QMOD)
    if (query("QMOD", 5))
      SetMode(buf[1]);

    sleep(10);
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
