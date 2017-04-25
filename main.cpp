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
  float voltage_grid;
  float freq_grid;
  float voltage_out;
  float freq_out;

  int load_va;
  int load_watt;
  int load_percent;
  int voltage_bus;

  float voltage_batt;
  int batt_charge_current;
  int batt_capacity;
  int temp_heatsink;
  int pv1;
  float pv2;
  float scc;
  int batt_discharge_current;

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

    string *reply = ups->GetStatus();
    if (reply)
    {
      lprintf("QPIGS: %s", reply->c_str());

      //parse and display values
      sscanf(reply->c_str(), "(%f %f %f %f %d %d %d %d %f %d %d %d %d %f %f %d", &voltage_grid, &freq_grid, &voltage_out, &freq_out, &load_va, &load_watt, &load_percent, &voltage_bus, &voltage_batt, &batt_charge_current, &batt_capacity, &temp_heatsink, &pv1, &pv2, &scc, &batt_discharge_current);
      printf("\tAC Grid voltage: %.1f\n", voltage_grid);
      printf("\tAC Grid frequency: %.1f\n", freq_grid);
      printf("\tAC out voltage: %.1f\n", voltage_out);
      printf("\tAC out frequency: %.1f\n", freq_out);
      printf("\tLoad [%]: %d\n", load_percent);
      printf("\tLoad [W]: %d\n", load_watt);
      printf("\tLoad [VA]: %d\n", load_va);
      printf("\tBus voltage: %d\n", voltage_bus);
      printf("\tHeatsink temperature: %d\n", temp_heatsink);
      printf("\tBattery capacity [%]: %d\n", batt_capacity);
      printf("\tBattery voltage: %.2f\n", voltage_batt);
      printf("\tBattery charge current [A]: %d\n", batt_charge_current);
      printf("\tBattery discharge current [A]: %d\n", batt_discharge_current);

      delete reply;
    }

    sleep(3);
  }
  lprintf("MAIN LOOP END");

  if (ups)
    delete ups;

  return 0;
}
