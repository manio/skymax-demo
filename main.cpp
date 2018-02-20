#include <algorithm>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include "main.h"
#include "inputparser.h"
#include "tools.h"

#include <pthread.h>
#include <signal.h>

cSkymax *ups = NULL;
atomic_bool ups_status_changed(false);
atomic_bool ups_data_changed(false);
atomic_bool ups_cmd_executed(false);

int print_help()
{
  printf("USAGE:  skymax -d <device ex: /dev/hidraw0> [-i <run interval> | -r <raw command>] [-h | --help]\n\n");
  return 1;
}

int main(int argc, char** argv)
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
  int pv_input_current;
  float pv_input_voltage;
  float pv_input_watts;
  float pv_input_watthour;
  float load_watthour = 0;
  float scc;
  int batt_discharge_current;
  
  InputParser cmdArgs(argc, argv);
  // Get (non-optional) device to read (probably /dev/hidraw0 or /dev/hidraw1)
  const std::string& devicename = cmdArgs.getCmdOption("-d");
  const std::string& rawcmd = cmdArgs.getCmdOption("-r");
  const std::string& runinterval = cmdArgs.getCmdOption("-i");
  
  if(cmdArgs.cmdOptionExists("-h") || cmdArgs.cmdOptionExists("--help"))
  {
    return print_help();
  }

  if (devicename.empty())
  {
    printf("Device must be provided.\n");
    return print_help();
  }
  
  // Now get EITHER '-r' if we are executing a command.
  if(cmdArgs.cmdOptionExists("-r"))
  {
    if (rawcmd.empty())
    {
      printf("raw command must be provided.\n");
      return print_help();
    }
  }
  else  // OR get '-i' if we are polling instead.
  {
    // Check run interval for correctness
    if (runinterval.empty())
    {
      printf("Run interval must be provided.\n");
      return print_help();
    }
    else
    {
      bool has_only_digits = true;
      for (size_t n = 0; n < runinterval.length(); n++)
      {
      if (!isdigit(runinterval[n]))
        {
          has_only_digits = false;
          break;
        }
      }
      
      if (!has_only_digits)
      {
        printf("Run interval must be all digits.\n");
        return 0;
      }
    }
  }

  bool ups_status_changed(false);
  ups = new cSkymax(devicename);
  
  if (!rawcmd.empty())
  {
      ups->ExecuteCmd(rawcmd);
      printf("Reply:  %s\n", ups->GetStatus()->c_str());
      goto endloop;
  }
  
  ups->runMultiThread();

  while (true)
  {
    // If inverter mode changes print it to screen
    if (ups_status_changed)
    {
      int mode = ups->GetMode();
      if (mode)
        lprintf("SKYMAX: %d", mode);
      ups_status_changed = false;
    }
    
    // If we recieve QPIGs data print it to screen
    if (ups_data_changed)
    {
      ups_data_changed = false;
      
      int mode = ups->GetMode();
      string *reply = ups->GetStatus();
      if (reply)
      {
        // Parse and display values
        sscanf(reply->c_str(), "%f %f %f %f %d %d %d %d %f %d %d %d %d %f %f %d", &voltage_grid, &freq_grid, &voltage_out, &freq_out, &load_va, &load_watt, &load_percent, &voltage_bus, &voltage_batt, &batt_charge_current, &batt_capacity, &temp_heatsink, &pv_input_current, &pv_input_voltage, &scc, &batt_discharge_current);

        // Calculate wattage (assume 92% efficiency)
        pv_input_watts = (pv_input_voltage * pv_input_current) * .92;
        // Calculate watthours generated per run interval period (given as program argument)
        pv_input_watthour = pv_input_watts / (3600 / stoi(runinterval.data()));
        // Only calculate load watthours if we are in battery mode (line mode doesn't count towards money savings)
        if (mode == 4)
            load_watthour = (float)load_watt / (3600 / stoi(runinterval.data()));

        // Print as JSON
        printf("{\n");
        printf("\"Inverter_mode\":%d,\n", mode);
        printf("\"AC_grid_voltage\":%.1f,\n", voltage_grid);
        printf("\"AC_grid_frequency\":%.1f,\n", freq_grid);
        printf("\"AC_out_voltage\":%.1f,\n", voltage_out);
        printf("\"AC_out_frequency\":%.1f,\n", freq_out);
        printf("\"PV_in_voltage\":%.1f,\n", pv_input_voltage);
        printf("\"PV_in_current\":%d,\n", pv_input_current);
        printf("\"PV_in_watts\":%.1f,\n", pv_input_watts);
        printf("\"PV_in_watthour\":%.4f,\n", pv_input_watthour);
        printf("\"Load_pct\":%d,\n", load_percent);
        printf("\"Load_watt\":%d,\n", load_watt);
        printf("\"Load_watthour\":%.4f,\n", load_watthour);
        printf("\"Load_va\":%d,\n", load_va);
        printf("\"Bus_voltage\":%d,\n", voltage_bus);
        printf("\"Heatsink_temperature\":%d,\n", temp_heatsink);
        printf("\"Battery_capacity\":%d,\n", batt_capacity);
        printf("\"Battery_voltage\":%.2f,\n", voltage_batt);
        printf("\"Battery_charge_current\":%d,\n", batt_charge_current);
        printf("\"Battery_discharge_current\":%d\n", batt_discharge_current);
        printf("}\n");

        delete reply;

        // Do once and exit instead of loop endlessly
        goto endloop;
      }
    }

    sleep(1);
  }
endloop:

  if (ups)
    delete ups;

  return 0;
}
