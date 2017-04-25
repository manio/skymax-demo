#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <thread>
#include "main.h"
#include "tools.h"

#include <pthread.h>
#include <signal.h>

cSkymax *ups = NULL;
atomic_bool ups_status_changed(false);

int main()
{
  bool ups_status_changed(false);
  ups = new cSkymax;
  ups->runMultiThread();

  lprintf("MAIN LOOP");
  while (true)
  {
    if (ups_status_changed)
    {
      string *mode = ups->GetMode();
      if (mode)
      {
        lprintf("SKYMAX: %s", mode->c_str());
        delete mode;
      }
      ups_status_changed = false;
    }
    sleep(3);
  }
  lprintf("MAIN LOOP END");

  if (ups)
    delete ups;

  return 0;
}
