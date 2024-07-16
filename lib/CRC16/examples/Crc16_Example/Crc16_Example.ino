#include <Crc16.h>
//Crc 16 library (XModem)
Crc16 crc; 

void setup()
{
    Serial.begin(38400); 
    Serial.println("CRC-16 bit test program");
    Serial.println("=======================");
  
}

void loop()
{
  /*
    Examples of crc-16 configurations
    Kermit: width=16 poly=0x1021 init=0x0000 refin=true  refout=true  xorout=0x0000 check=0x2189
    Modbus: width=16 poly=0x8005 init=0xffff refin=true  refout=true  xorout=0x0000 check=0x4b37
    XModem: width=16 poly=0x1021 init=0x0000 refin=false refout=false xorout=0x0000 check=0x31c3
    CCITT-False:width=16 poly=0x1021 init=0xffff refin=false refout=false xorout=0x0000 check=0x29b1
    see http://www.lammertbies.nl/comm/info/crc-calculation.html
  */
  //calculate crc incrementally
  byte data[] = "123456789";
  
  Serial.println("Calculating crc incrementally");
  
  crc.clearCrc();
  for(byte i=0;i<9;i++)
  {
     Serial.print("byte ");
     Serial.print(i);
     Serial.print(" = ");
     Serial.println(data[i]);
     crc.updateCrc(data[i]);
  }
  unsigned short value = crc.getCrc();
  Serial.print("crc = 0x");
  Serial.println(value, HEX);
  
  Serial.println("Calculating crc in a single call");
  
  //XModem
  value = crc.XModemCrc(data,0,9);
  Serial.print("XModem crc = 0x");    
  Serial.println(value, HEX);
  //Reference xmodem
  Serial.println("Reference XModem crc");
  value = calcrc((char*)data, 9);
  Serial.print("crc = 0x");    
  Serial.println(value, HEX);

  //Modbus
  value = crc.Modbus(data,0,9);
  Serial.print("Modbus crc = 0x");    
  Serial.println(value, HEX);

  //Mcrf4XX
  value = crc.Mcrf4XX(data,0,9);
  Serial.print("Mcrf4XX crc = 0x");    
  Serial.println(value, HEX);

  while(true);
}
//Check routine taken from
//http://web.mit.edu/6.115/www/miscfiles/amulet/amulet-help/xmodem.htm
int calcrc(char *ptr, int count)
{
    int  crc;
    char i;
    crc = 0;
    while (--count >= 0)
    {
        crc = crc ^ (int) *ptr++ << 8;
        i = 8;
        do
        {
            if (crc & 0x8000)
                crc = crc << 1 ^ 0x1021;
            else
                crc = crc << 1;
        } while(--i);
    }
    return (crc);
}