#include <windows.h>
#include <stdio.h>
#include <conio.h>
#include <stdint.h>
#include "math.h"

enum
{
  kTestWaveform_Constant=0,
  kTestWaveform_Sine,
  kTestWaveform_Square
};

enum
{
  kTestMode_ConstantCurrent=1,
  kTestMode_ConstantPower,
  kTestMode_ConstantResistance
};

// if csv_file is a valid filename the sample data will be written to it as a CSV which can be loaded in to excel/google docs etc
char* csv_file = 0;

// COM port settings, if you want to use a com port above COM9 you have to use the full device name such as //./COM14
char* comm_port = "COM1";   //COM5 or //./COM14
char* comm_string = "9600,n,8,1";

// set the termination values, these can be set to -1
float terminate_total_time=-1.0f;             //
float terminate_low_voltage=-1.0f;            // low limit in volts
float terminate_low_current=-1.0f;            // low limit in amps

// how many milliseconds between samples
uint32_t sample_delay_ms = 1000;

// set this to terminate after a number of samples (set to 0xffffffff) to never terminate on sample count.
uint32_t terminate_sample_count = 0xffffffff;

// the load and the type of load (default is 1amp, constant current)
uint32_t test_load = 1000;         
uint32_t test_mode = kTestMode_ConstantCurrent;

// the load wave type
uint32_t test_wave = kTestWaveform_Constant;      
float wave_period = 60.0f;                // 60 seconds for one full cycle

// the max settings are useful when doing something like constant resistance and you don't want certain limits to be exceeded.
// by default they are set to the maximum of the instrument.
uint32_t max_current = 30000;             // 30amps
uint32_t max_power   = 200000;            // 200watts

uint8_t device_address = 0x01;            // default

//---------------------------------------------------------------------------------------------------------------------
// checksum 25 bytes of data by adding everything together and returning the bottom byte of the result
uint8_t Checksum(uint8_t* data)
{
  uint32_t check=0;
  for (int i=0;i<25;i++)
  {
    check+=data[i];
  }

  return (uint8_t)(check&0xff);
}

//---------------------------------------------------------------------------------------------------------------------
// data should be 26 bytes, the checksum is written to the last byte
bool WriteData(HANDLE comm, uint8_t* data)
{
  uint32_t written=0;

  // put the checksum in the last byte
  data[25] = Checksum(data);    

  //write 26 bytes to the instrument
  WriteFile(comm,data,26,(DWORD*)&written,0);

  //if we wrote 26 bytes we are good, if the checksum is not valid the instrument will ignore the packet
  return (written==26);
}

//---------------------------------------------------------------------------------------------------------------------
bool RequestStatus(HANDLE comm, uint8_t adr, uint8_t* data)
{
  //write the request packet command 0x91 (see data sheet), the 0xff at the end will get overritten by the checksum
  uint8_t request[26] = {0xaa,adr,0x91,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff};
  WriteData(comm,request);

  // now read 26 bytes to get the data we asked for.
  uint32_t read=0;
  ReadFile(comm,data,26,(DWORD*)&read,0);

  // check the checksum and check we got the correct amount of data
  uint8_t check = Checksum(data);  
  return ((read==26) && (check==data[25]));
}

//---------------------------------------------------------------------------------------------------------------------
// activate the load with command 0x92, this command sets the load and weather or not the load is under control of the
// serial port (PC) or the front panel.
void ActivateLoad(HANDLE comm, uint8_t adr, bool load, bool pc_control)
{
  uint8_t state=0;
  if (load)
    state|=0x1;
  if (pc_control)
    state|=0x2;
  uint8_t data[26] = {0xaa,adr,0x92,state,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff};
  WriteData(comm,data);
}

//---------------------------------------------------------------------------------------------------------------------
// Set the load on the instrument with command 0x90
//
// max_current sets the limit for the current, if this is exceeded it will be flagged in the status, the load will be
// switched off and the app will terminate.
//
// max_power sets the limit for power, if this is exceeded it will be flagged in the status (the app will terminate)
//
// The load value passed to this function is always in milli units, either milliwatts, milliamps or milliohms. The value
// is converted as documented in the data sheet.
//
// Type is the type of load, kTestMode_ConstantCurrent, kTestMode_ConstantPower, kTestMode_ConstantResistance
void SetLoad(HANDLE comm, unsigned char adr, uint32_t max_current, uint32_t max_power, uint32_t load, uint32_t type)
{
  max_power/=100;

  // when type is 1 the input range is in mA

  if (type==kTestMode_ConstantPower)      // if the type is constant power (2) then we need the power in the range 0 to 2000 for 0 to 200.0W, mW are input
    load/=100;

  if (type==kTestMode_ConstantResistance) // if the type is constant resistance (3) then we need the resistance in the range of 0 to 50000 for 0 to 500.00 Ohm, mO are input
    load/=10;

  uint8_t data[26] = {0xaa,adr,0x90,
    max_current&0xff,
    (max_current>>8)&0xff,
    max_power&0xff,
    (max_power>>8)&0xff,
    adr,
    type&0xff,
    load&0xff,
    (load>>8)&0xff,
    0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xff};
  WriteData(comm,data);
}


//---------------------------------------------------------------------------------------------------------------------
//    Test the discharge of a AA battery:
//    -port COM5 -CC -load 1.0 -waveform 0 -sample_period 1.0 -min_volts 0.75 -csv d:/test.csv
//     a constant current load of 1.0 amps, terminate when the voltage gets to 0.75volts, 
//     take a sample every second, write the log to test.csv
void ParseCommandLine(int argc, char* argv[])
{
   int arg = 1;

   while (arg < argc)
   {
      if (_stricmp(argv[arg],"-csv")==0)
      {
        arg++;
        csv_file = argv[arg];
      } 
      else if (_stricmp(argv[arg],"-port")==0)
      {
         arg++;
         comm_port = argv[arg];
      }
      else if (_stricmp(argv[arg],"-spec")==0)
      {
         arg++;
         comm_string = argv[arg];
      }
      else if (_stricmp(argv[arg],"-total_time")==0)
      {
         arg++;
         terminate_total_time = (float)atof( argv[arg] );
      }
      else if (_stricmp(argv[arg],"-min_volts")==0)
      {
         arg++;
         terminate_low_voltage = (float)atof( argv[arg] );
      }
      else if (_stricmp(argv[arg],"-min_amps")==0)
      {
         arg++;
         terminate_low_current = (float)atof( argv[arg] );
      }
      else if (_stricmp(argv[arg],"-total_samples")==0)
      {
         arg++;
         terminate_sample_count = atoi( argv[arg] );
      }
      else if (_stricmp(argv[arg],"-sample_period")==0)
      {
         arg++;
         float t = (float)atof( argv[arg] );
         if (t<0)
         {
           printf("Invalid sample period\n");
           t=0.0f;
         }
         sample_delay_ms = (uint32_t)(1000.0f*t);
      }
      else if (_stricmp(argv[arg],"-load")==0)
      {
         float t = (float)atof( argv[arg] );
         if (t<0)
         {
           printf("Invalid load\n");
           t=1.0f;
         }
         test_load = (uint32_t)(1000.0f*t);
      }
      else if (_stricmp(argv[arg],"-CC")==0)
      {
         test_mode = kTestMode_ConstantCurrent;
      }
      else if (_stricmp(argv[arg],"-CP")==0)
      {
         test_mode = kTestMode_ConstantPower;
      }
      else if (_stricmp(argv[arg],"-CR")==0)
      {
         test_mode = kTestMode_ConstantResistance;
      }
      else if (_stricmp(argv[arg],"-waveform")==0)
      {
         arg++;
         test_wave = (uint32_t)atoi( argv[arg] );
         if (test_wave>2)
         {
           printf("Unknown waveform\n");
           test_wave = 0;
         }
      }
      else if (_stricmp(argv[arg],"-waveform_period")==0)
      {
         arg++;
         float t = (float)atof( argv[arg] );
         if (t<0)
         {
           printf("Invalid waveform period - default 60 seconds\n");
           t=60.0f;
         }
         wave_period = t;
      }
      else if (_stricmp(argv[arg],"-current_limit")==0)
      {
         arg++;
         max_current = (uint32_t)(1000.0f*atof( argv[arg] ));
         if (max_current>30.0f)
         {
           printf("Invalid current limit: 0.0 to 30.0 amps\n");
           exit(0);
         }
      }
      else if (_stricmp(argv[arg],"-power_limit")==0)
      {
         arg++;
         max_power = (uint32_t)(1000.0f*atof( argv[arg] ));
         if (max_power>200.0f)
         {
           printf("Invalid power limit: 0.0 to 200.0 watts\n");
           exit(0);
         }
      }

      arg++;
   }
}


//---------------------------------------------------------------------------------------------------------------------
int main(int argc, char* argv[])
{
  // parse the command line and check limits
  ParseCommandLine(argc,argv);

  // open the com port
  HANDLE hComm;
  hComm = CreateFile( comm_port,  
                      GENERIC_READ | GENERIC_WRITE, 
                      0, 
                      0, 
                      OPEN_EXISTING,
                      0,
                      0);
  if (hComm == INVALID_HANDLE_VALUE)
  {
    printf("Error opening COM port '%s'\n",comm_port);
    exit(0);
  }

  // set the baud rate, data format etc
  DCB dcb;
  FillMemory(&dcb, sizeof(dcb), 0);
  dcb.DCBlength = sizeof(dcb);
  if (!BuildCommDCB(comm_string, &dcb)) 
  {   
    // Couldn't build the DCB. Usually a problem
    // with the communications specification string.
    printf("Error building DCM - check the comm spec string\n");
    CloseHandle(hComm);
    exit(0);
  }

  if (!SetCommState(hComm, &dcb))
  {
    CloseHandle(hComm);
    exit(0);
  }

  // 26 byte buffer for the device status
  uint8_t status[26];

  // open the CSV file
  FILE* csv = 0;   
  if (csv_file)
  csv = fopen(csv_file,"w");

  if (csv)
    fprintf(csv,"time,volts,current,power,amphour, watthour, status\n");
  else
    printf("Failed to open csv file '%s'\n",csv_file);

  // set the load to zero (who knows how the front panel is set), activate the load and remote control
  // the real load will be set in the main loop
  SetLoad(hComm,device_address,max_current,max_power,0,test_mode);
  ActivateLoad(hComm,device_address,true,true);

  // sleep and request a status, the first one seems to be garbage
  Sleep(250);
  RequestStatus(hComm,device_address,status);
  Sleep(250);

  uint32_t count=0;
  ULONGLONG start = GetTickCount64();
  double last_time = 0.0;
  double capacity = 0.0;
  double watthour = 0.0;
  while(1)
  {
    ULONGLONG now = GetTickCount64();
    ULONGLONG elapsed = now-start;
    double elapsed_time = double(elapsed)/1000.0;

    double delta_time = elapsed_time - last_time;
    last_time = elapsed_time;

    switch (test_wave)
    {
    case kTestWaveform_Constant:
      {
        SetLoad(hComm,device_address,max_current,max_power,test_load,test_mode);
        break;
      }

    case kTestWaveform_Sine:
    case kTestWaveform_Square:
      {
        double s = sin( 2*3.14 * elapsed_time/wave_period);
        uint32_t new_load;
        if (test_wave==kTestWaveform_Sine)
          new_load = (uint32_t)(((s+1.0)/2.0)*(double)test_load);
        else
          new_load = s>0.0f?test_load:0;
        SetLoad(hComm,device_address,30000,200000,new_load,test_mode);
        break;
      }
    }

     if (_kbhit())
     {
       if (_getch()==27)
       break;
     }
     RequestStatus(hComm,device_address,status);

     // the following conditons don't terminate. The load should get turned off and then load off check will terminate.
     if (status[17]&4)
     {
       printf("Reversed Polarity\n");
     }

     if (status[17]&8)
     {
       printf("Excessive Temperature\n");
     }

     if (status[17]&16)
     {
       printf("Excessive Voltage\n");
     }

     if (status[17]&16)
     {
       printf("Excessive Power\n");
     }

     if ((status[17] & 2)==0)  // load on/off
     {
       // load is off, terminate
       printf("Load OFF - terminating\n");
       if (csv)
         fprintf(csv,"Load turned off\n");
       break;
     }

     //
     // Pull the current/voltage/power settings from the status
     //
     DWORD current_low = status[3];
     DWORD current_high = status[4];
     float current = float((current_low) | (current_high<<8))/1000.0f;

     DWORD power_low = status[9];
     DWORD power_high = status[10];
     float power = float((power_low) | (power_high<<8))/10.0f;

     DWORD v1_low = status[5];
     DWORD v1_high = status[6];
     DWORD v2_low = status[7];
     DWORD v2_high = status[8];
     DWORD v1 = ((v1_low) | (v1_high<<8));
     DWORD v2 = ((v2_low) | (v2_high<<8));
     float volts = float(v1 | (v2<<16))/1000.0f;

     // the power reported in the status is only in units of 100mW, we can compute a better power value
     // to use in watt hour calculations.
     float computed_power = current*volts;

     capacity+=((current*delta_time)/3600.0);      // curent*time (in hours)
     watthour+=((computed_power*delta_time)/3600.0);

     printf("%.4f sec, %.3fV, %.3fA, %.3fW - [%.3f AHr, %.3f WHr], 0x%02x\n", elapsed_time, volts, current,computed_power, capacity,watthour,status[17]);
     if (csv)
       fprintf(csv,"%.3f, %.3f, %.3f, %.3f, %.3f, %.3f, 0x%02x\n", elapsed_time, volts, current,computed_power, capacity,watthour,status[17]);

     if (elapsed_time>3.0f)
     {
       if (volts<=terminate_low_voltage)
       {
         printf("Low voltage limit hit - terminating\n");
         if (csv)
           fprintf(csv,"Low voltage limit\n");
         break;
       }

       if (current<=terminate_low_current)
       {
         printf("Low current limit hit - terminating\n");
         if (csv)
           fprintf(csv,"Low current limit\n");
         break;
       }
     }

     if (terminate_total_time>0.0f)
     {
       if (elapsed_time>terminate_total_time)
       {
          printf("Total time limit - terminating\n");
           if (csv)
             fprintf(csv,"Time limit\n");
          break;
       }
     }

     if (terminate_sample_count!=0xffffffff)
     {
       if (count>terminate_sample_count)
       {
          printf("Sample count limit - terminating\n");
          if (csv)
            fprintf(csv,"Sample count limit\n");
          break;
       }
     }
     Sleep(sample_delay_ms);   
     count++;
   }

   printf("Turning off load\n");
   ActivateLoad(hComm,device_address,false,true);

   printf("Turning off remote control\n");
   ActivateLoad(hComm,device_address,false,false);

   if (csv)
     fclose(csv);

   CloseHandle(hComm);

	return 0;
}
