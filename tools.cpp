#include <stdio.h>
#include <unistd.h>
#include <stdint.h>
#include <stdarg.h>
#include <time.h>
#include <sys/time.h>
#include <string.h>
#include "tools.h"
#include <mutex>
#include <string>

std::mutex log_mutex;

void lprintf(const char *format, ...)
{
  va_list ap;
  char fmt[2048];

  //actual time
  time_t rawtime;
  struct tm *timeinfo;
  time(&rawtime);
  timeinfo = localtime(&rawtime);
  char buf[256];
  strcpy(buf, asctime(timeinfo));
  buf[strlen(buf)-1] = 0;

  //connect with args
  snprintf(fmt, sizeof(fmt), "%s %s\n", buf, format);

  //put on screen:
  va_start(ap, format);
  vprintf(fmt, ap);
  va_end(ap);

  //to the logfile:
  static FILE *log;
  log_mutex.lock();
  log = fopen(LOG_FILE, "a");
  va_start(ap, format);
  vfprintf(log, fmt, ap);
  va_end(ap);
  fclose(log);
  log_mutex.unlock();
}
