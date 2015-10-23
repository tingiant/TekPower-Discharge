# discharge

A tool to discharge batteries using TekPower DC load meter.. BK/ITECH version is coming soon.

This source only supports windows but it should be trivial to change out the code to open the serial port, the remainder of the code will work on Linux and OSX. It should even be possible to run this on an embedded controller for a stand alone testing, however do note the instrument output true RS232 and not TTL levels. 

***Please use an isolated serial port for the your PC be it a proper serial port or a serial to usb converter. We don't want a 30A load accidently grounding through you PC motherboard***

By default data is logged to the terminal, in the following format:

TIME         VOLTAGE   CURRENT   POWER     AMP/HOUR   WATT/HOUR   STATUS 

16.2870 sec, 0.985V,   0.997A,   0.982W - [0.004 AHr, 0.004 WHr], 0x03

17.2850 sec, 0.985V,   0.997A,   0.982W - [0.005 AHr, 0.004 WHr], 0x03

18.2990 sec, 0.985V,   0.997A,   0.982W - [0.005 AHr, 0.005 WHr], 0x03

19.2970 sec, 0.983V,   0.997A,   0.980W - [0.005 AHr, 0.005 WHr], 0x03

The power column is computed by the software because the instrument only reports power to a resolution of 100mW (0.1Watts). This is not enough precision to compute a decent figure A/Hr and W/Hr, voltage and current are reported much more accurately in milliamps and millivolts so computed power is much more accurate.

The same data can optionally be logged to a csv file and loaded in to excel or some other tool.

This tool has many options:

***-port <>***

Specifies the com port to use. If the port is COM9 or lower it can be specified directly, to use a port of COM10 or higher a fully qualified device name is required such as \\.\COM15

default: COM1

Example: 

    -port COM4
    -port \\.\COM15

***-spec <>***

Specifices the com port speed and format. By default the instrument uses 9600 baud, no parity, 8 data bits, 1 stop bit (9600,n,8,1). The app uses the same defaults so unless the instrument has been changed there is no need to specifiy this. 

default: 9600,n,8,1

example: 

    -spec 115200,n,8,1

***-CC***

Set the load to constant current. The load parameter will be interpreted as amps with 0.001 (milliamp) accuracy. The allowable current range is from 0.000 to 30.000.

***-CP***

Set the load to constant power. The load parameter will be interpreted as watts with 0.1 (100 milliwatt) accuracy.  The allowable power range is from 0.0 to 200.0

***-CR***

Set the load to constant resistance. The load parameter will be interpreted as ohms with 0.01 (10 milliohm) accuracy. The allowable resistance range is 0.00 to 500.00 ohms.

***-load <>***

Specifies the load parameter, how this is interpreted depends on if the load type is constant current, constant power or constant resistance. See -CC, -CP, -CR above.

default: -CC -load 1.0

example: 

    -CP -load 5.5    (constant power at 5.5 watts)
    -CR -load 100.0  (constant resistance of 100 ohms)
         

***-csv <>***

The same data written to the terminal can be optionally written to a csv file and further processed or graphed in execel or some other tool. If the specified file cannot be opened the option is ignored and the discharge app runs as normal just logging to the terminal.

default: none (don't write file)

example: 

    -csv c:\aa-discharge.csv

***-total_time <>***

Specifies how long the application should run for in seconds before automatically switching off the load and terminating.

default: no time limit

example: 

    -total_time 300.0   (run for 5 minutes)


***-total_samples <>***

Specifies how many samples should be taken before automatically switching off the load and terminating.

default: no limit

example: 

    -total_samples 100   (take 100 samples)


***-sample_period <>***

Specifies how often to take a sample in seconds.

default: 1.0

example: 

    -total_period 10.0   (take a sample every 10 seconds)
    -total_period 0.5    (take 2 samples per second)


***-min_volts <>***

Specifies the minimum voltage, as soon as the specified voltage is reached the load is switched off and the application terminates.

default: no minimum voltage

example:

    -min_volts 0.75   (switch off at anything lower 0.75V, a good value for a AA battery)
    -min_volts 3.00   (switch off at anything lower 3.00V, a good value for a lipo battery)

***-min_amps <>***

Specifies the minimum amperage, as soon as the specified current limit is reached the load is switched off and the application terminates.

default: no minimum current

example:  

    -min_amps  0.5    (switch off at anything lower than 0.5A)
  

***-waveform <>***

While the instrument can only do constant loads the app can continually change the load to simulate various waveforms. The type of waveform to use is indicated by the value

0 - DC (no waveform)
1 - Sine Wave - the load specifies the peak, the minium is zero.
2 - Square Wave - the load is switched on and off (useful for measuring unloaded voltage while discharging)

default: DC (no waveform)

***-waveform_period <>***

Specifies the frequency of the above waveform in seconds.

default: 60 seconds

example:

    -waveform 1 -waveform_period 120.0 -CC -load 1000  (a constant current load in a sine wave between 0 and 1 amp, a full wave takes 2 minutes)
    -waveform 2 -waveform_period 120.0 -CC -load 1000  (a constant current load of 1Amp, switch on and off every 60 seconds)


***-current_limit <>***

specifies the maximum current, the instrument will switch off if this limit is reached. This value is sent to the instrument and is not handled by the application. The appplication will terminate with an over current error. This is useful when in constant resistance mode or constant power mode to add a little safety net and prevent smoking your device under test.

default: 30.0 (30 amps is the maximum current supported by the instrument)

***-power_limit <>***

specifies the maximum power, the instrument will switch off if this limit is reached. This value is sent to the instrument and is not handled by the application. The appplication will terminate with an over power error. This is useful when in constant resistance mode or constant current mode to add a little safety net and prevent smoking your device under test.

default: 200.0 (200 watts is the maximum supported by the instrument)



Full example:

Dischage a single AA battery at 500mA, terminate the test when the battery voltage gets to 0.75V, takes a sample every second, write the result to a CSV file.

discahrge.exe -port COM5 -CC -load 1.0 -waveform 0 -sample_period 1.0 -min_volts 0.75 -csv d:/AA-500.csv


