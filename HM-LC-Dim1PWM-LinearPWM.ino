//- -----------------------------------------------------------------------------------------------------------------------
// AskSin++
// 2016-10-31 papa Creative Commons - http://creativecommons.org/licenses/by-nc-sa/3.0/de/
//- -----------------------------------------------------------------------------------------------------------------------

// define this to read the device id, serial and device type from bootloader section
// #define USE_OTA_BOOTLOADER

#define EI_NOTEXTERNAL
#include <Wire.h>
#include <EnableInterrupt.h>
#include <AskSinPP.h>
#include <sensors/Ds18b20.h>
#include <Dimmer.h>


// we use a Pro Mini
#define LED_PIN 4
#define DIMMER_PIN 3
#define DS18B20_PIN 6
#define CONFIG_BUTTON_PIN 8

#define PWM_INVERSE true


// number of available peers per channel
#define PEERS_PER_CHANNEL 4

// all library classes are placed in the namespace 'as'
using namespace as;

// define all device properties
const struct DeviceInfo PROGMEM devinfo = {
    {0x11,0x12,0x5A},       // Device ID
    "konstant22",           // Device Serial
    {0x00,0x67},            // Device Model
    0x25,                   // Firmware Version
    as::DeviceType::Dimmer, // Device Type
    {0x01,0x00}             // Info Bytes
};

/**
 * Configure the used hardware
 */
typedef AvrSPI<10,11,12,13> SPIType;
typedef Radio<SPIType,2> RadioType;
typedef StatusLed<LED_PIN> LedType;
typedef AskSin<LedType,NoBattery,RadioType> HalType;
typedef DimmerChannel<HalType,PEERS_PER_CHANNEL> ChannelType;
typedef DimmerDevice<HalType,ChannelType,3,3> DimmerType;

HalType hal;
DimmerType sdev(devinfo,0x20);
DimmerControl<HalType,DimmerType,PWM8<200,true,PWM_INVERSE> > control(sdev);
ConfigToggleButton<DimmerType> cfgBtn(sdev);

class TempSens : public Alarm {
  Ds18b20  temp;
  OneWire  ow;
  bool     measure;
public:
  TempSens () : Alarm(0), ow(DS18B20_PIN), measure(false) {}
  virtual ~TempSens () {}

  void init () {
    Ds18b20::init(ow, &temp, 1);
    if( temp.present()==true ) {
      set(seconds2ticks(15));
      sysclock.add(*this);
    } else {
      DPRINTLN("WARN: No Tempsensor found!");
    }
  }

  virtual void trigger (AlarmClock& clock) {
    if( measure == false ) {
      temp.convert();
      set(millis2ticks(800));
    }
    else {
      temp.read();
      DPRINT("Temp: ");DDECLN(temp.temperature());
      control.setTemperature(temp.temperature());
      set(seconds2ticks(60));
    }
    measure = !measure;
    clock.add(*this);
  }
};
TempSens tempsensor;

void setup () {
  DINIT(57600,ASKSIN_PLUS_PLUS_IDENTIFIER);
  Wire.begin();
  if( control.init(hal,DIMMER_PIN) ) {
    // first init - setup connection between config button and first channel
    sdev.channel(1).peer(cfgBtn.peer());
  }
  buttonISR(cfgBtn,CONFIG_BUTTON_PIN);

  tempsensor.init();
  sdev.initDone();
  // sdev.led().invert(true);
  DDEVINFO(sdev);

  if(PWM_INVERSE) {
    // Dimm to off after boot
    sdev.channel(1).set(200, 0b01000001, 0xffff);
  }
}

void loop() {
  bool worked = hal.runready();
  bool poll = sdev.pollRadio();
  if( worked == false && poll == false ) {
    hal.activity.savePower<Idle<true> >(hal);
  }
}
