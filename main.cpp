#include <algorithm>
#include <fstream>
#include <iostream>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <thread>
#include <unistd.h>
#include "main.h"
#include "inputparser.h"
#include "tools.h"


bool debugFlag = false;
cSkymax *ups = NULL;
atomic_bool ups_status_changed(false);
atomic_bool ups_qmod_changed(false);
atomic_bool ups_qpiri_changed(false);
atomic_bool ups_qpigs_changed(false);
atomic_bool ups_cmd_executed(false);
string devicename;
int runinterval;
float ampfactor;
float wattfactor;


void attemptAddSetting(int *addTo, string addFrom)
{
  try
  {
    *addTo = stof(addFrom);
  }
  catch (exception e)
  {
    cout << e.what() << '\n';
    cout << "There's probably a string in the settings file where an int should be.\n";
  }
}


void attemptAddSetting(float *addTo, string addFrom)
{
  try
  {
    *addTo = stof(addFrom);
  }
  catch (exception e)
  {
    cout << e.what() << '\n';
    cout << "There's probably a string in the settings file where a floating point should be.\n";
  }
}


void getSettingsFile(string filename)
{
  try
  {
    string fileline, linepart1, linepart2;
    ifstream infile;
    infile.open(filename);
    while(!infile.eof())
    {
      getline(infile, fileline);
      size_t firstpos = fileline.find("#");
      if(firstpos != 0 && fileline.length() != 0)    // Ignore lines starting with # (comment lines)
      {
        size_t delimiter = fileline.find("=");
        linepart1 = fileline.substr(0, delimiter);
        linepart2 = fileline.substr(delimiter+1, string::npos - delimiter);

        if(linepart1 == "device")
          devicename = linepart2;
        else if(linepart1 == "run_interval")
          attemptAddSetting(&runinterval, linepart2);
        else if(linepart1 == "amperage_factor")
          attemptAddSetting(&ampfactor, linepart2);
        else if(linepart1 == "watt_factor")
          attemptAddSetting(&wattfactor, linepart2);
        else
          continue;
      }
    }
    infile.close();
  }
  catch (...)
  {
      cout << "Settings could not be read properly...\n";
  }
}


int main(int argc, char **argv)
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
  float pv_input_current;
  float pv_input_voltage;
  float pv_input_watts;
  float pv_input_watthour;
  float load_watthour = 0;
  float scc;
  int batt_discharge_current;

  // Get command flag settings from the arguments (if any)
  InputParser cmdArgs(argc, argv);
  const string &rawcmd = cmdArgs.getCmdOption("-r");
  if(cmdArgs.cmdOptionExists("-h") || cmdArgs.cmdOptionExists("--help"))
  {
    return print_help();
  }
  if(cmdArgs.cmdOptionExists("-d"))
  {
    debugFlag = true;
  }
  lprintf("SKYMAX:  Debug set");

  // Get the rest of the settings from the conf file
  getSettingsFile("/opt/skymax/bin/skymax.conf");

  bool ups_status_changed(false);
  ups = new cSkymax(devicename);
  
  if (!rawcmd.empty())
  {
      ups->ExecuteCmd(rawcmd);
      // We can piggyback on either GetStatus() function to return our result, it doesn't matter which
      printf("Reply:  %s\n", ups->GetQpigsStatus()->c_str());
      goto endloop;
  }

  ups->runMultiThread();
  
  while (true)
  {
    lprintf("SKYMAX:  Start loop");
    // If inverter mode changes print it to screen
    if (ups_status_changed)
    {
      int mode = ups->GetMode();
      if (mode)
        lprintf("SKYMAX: %d", mode);
      ups_status_changed = false;
    }
    
    // Once we receive all queries print it to screen
    if (ups_qmod_changed && ups_qpiri_changed && ups_qpigs_changed)
    {
      ups_qmod_changed = false;
      ups_qpiri_changed = false;
      ups_qpigs_changed = false;
      
      int mode = ups->GetMode();
      string *reply1 = ups->GetQpigsStatus();
      string *reply2 = ups->GetQpiriStatus();
      if (reply1 && reply2)
      {
        // Parse and display values
        sscanf(reply1->c_str(), "%f %f %f %f %d %d %d %d %f %d %d %d %f %f %f %d", &voltage_grid, &freq_grid, &voltage_out, &freq_out, &load_va, &load_watt, &load_percent, &voltage_bus, &voltage_batt, &batt_charge_current, &batt_capacity, &temp_heatsink, &pv_input_current, &pv_input_voltage, &scc, &batt_discharge_current);

        // There appears to be a large discrepancy in actual DMM
        // measured current vs what the meter is telling me it's getting
        pv_input_current = pv_input_current * ampfactor;
        // Calculate wattage (assume 95% efficiency)
        pv_input_watts = (pv_input_voltage * pv_input_current) * wattfactor;
        // Calculate watt-hours generated per run interval period (given as program argument)
        pv_input_watthour = pv_input_watts / (3600 / runinterval);
        // Only calculate load watt-hours if we are in battery mode (line mode doesn't count towards money savings)
        if (mode == 4)
            load_watthour = (float)load_watt / (3600 / runinterval);

        // Print as JSON
        printf("{\n");
        printf("\"Inverter_mode\":%d,\n", mode);
        printf("\"AC_grid_voltage\":%.1f,\n", voltage_grid);
        printf("\"AC_grid_frequency\":%.1f,\n", freq_grid);
        printf("\"AC_out_voltage\":%.1f,\n", voltage_out);
        printf("\"AC_out_frequency\":%.1f,\n", freq_out);
        printf("\"PV_in_voltage\":%.1f,\n", pv_input_voltage);
        printf("\"PV_in_current\":%.1f,\n", pv_input_current);
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

        delete reply1;
        delete reply2;

        // Do once and exit instead of loop endlessly
        lprintf("SKYMAX:  All queries complete, exiting using goto");
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
