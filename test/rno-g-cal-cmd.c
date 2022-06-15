#include "rno-g-cal.h" 

#include <stdio.h> 
#include <stdlib.h> 
#include <unistd.h> 
#include <string.h> 



void usage() 
{
  fprintf(stderr,"Usage: rno-g-cal-cmd [--bus=2] [--gpio=49] [--rev=D] CMD0 [VAL]  [ CMD1 ...] \n"); 
  fprintf(stderr,"      CMD can be: off, on, setup, temp, select-coax, select-fiber0, select-fiber1, select-none , atten val, pulse, vco, sleep N\n"); 
  fprintf(stderr,"      Up to 1024 commands are supported\n"); 
}

const char * valid_cmds[] = {"off","on","setup","temp","select-coax","select-fiber0","select-fiber1","select-none", "atten", "pulse", "vco", "sleep",}; 
enum { CMD_OFF, CMD_ON, CMD_SETUP, CMD_TEMP, CMD_COAX, CMD_FIB0, CMD_FIB1, CMD_NONE, CMD_ATTEN, CMD_PULSE, CMD_VCO, CMD_SLEEP} e_cmds; 

int is_valid(const char * cmd) 
{
  for (int i = 0;  i <  (int) (sizeof(valid_cmds)/ sizeof(*valid_cmds)) ; i++)
  {
    if (!strcmp(cmd,valid_cmds[i])) return i; 

  }

  return -1; 

}

int main (int nargs, char ** args) 
{

  if (nargs <2) 
  {
    usage(); 
    exit(1); 
  }


  uint8_t bus = 2; 
  int dbg = 0; 
  uint16_t gpio = 49; 
  char rev='D'; 
  
  static uint8_t cmds[1024]; 
  static uint16_t vals[1024]; 
  
  int ncmds = 0; 

  for (int iarg = 1; iarg < nargs; iarg++) 
  {
    if (!strcmp(args[iarg],"--bus"))
    {
      bus = atoi(args[++iarg]);
    }

    else if (!strcmp(args[iarg],"--debug"))
    {
      dbg =1; 
    }
 
    else if (!strcmp(args[iarg],"--gpio"))
    {
      gpio = atoi(args[++iarg]);
    }
    else if (!strcmp(args[iarg],"--rev"))
    {
      rev = args[++iarg][0];
    }
    //argument to atten? 
    else if (ncmds && ( (cmds[ncmds-1] == CMD_ATTEN) || (cmds[ncmds-1] == CMD_SLEEP)))
    {
      vals[ncmds-1] = atoi(args[iarg]); 
    }
    else 
    {
      int cmdcode = is_valid(args[iarg]); 
      if (cmdcode < 0) 
      {
        usage(); 
        return 1; 
      }

      if (ncmds >= (int) sizeof(cmds) )break; // only support up to 1024 commands... 
      cmds[ncmds++] = cmdcode; 

    }
  }

  rno_g_cal_dev_t * dev = rno_g_cal_open(bus,gpio, rev); 

  rno_g_cal_enable_dbg(dev,dbg); 

  for (int i = 0; i < ncmds; i++) 
  {
    float T; 
    switch (cmds[i])
    {
      case CMD_OFF: 
        rno_g_cal_disable(dev); 
        break; 
      case CMD_ON: 
        rno_g_cal_enable(dev); 
        break; 
      case CMD_SETUP: 
        rno_g_cal_setup(dev); 
        break; 
      case CMD_TEMP: 
        rno_g_cal_read_temp(dev,&T); 
        printf("Temperature: %f\n", T); 
        break; 
      case CMD_COAX: 
        rno_g_cal_select(dev, RNOG_CAL_CH_COAX); 
        break; 
      case CMD_FIB0: 
        rno_g_cal_select(dev, RNOG_CAL_CH_FIBER0); 
        break; 
      case CMD_FIB1: 
        rno_g_cal_select(dev, RNOG_CAL_CH_FIBER1); 
        break; 
     case CMD_NONE: 
        rno_g_cal_select(dev, RNOG_CAL_CH_NONE); 
        break; 
     case CMD_ATTEN: 
        rno_g_cal_set_atten(dev, vals[i]); 
        break; 
     case CMD_PULSE: 
        rno_g_cal_set_pulse(dev);
        break; 
     case CMD_VCO: 
        rno_g_cal_set_vco(dev);
        break; 
     case CMD_SLEEP: 
        sleep(vals[i]); 
        break; 
    }
  }

  rno_g_cal_close(dev); 

  return 0; 
} 

