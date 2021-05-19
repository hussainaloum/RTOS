// AP_Lab6.c
// Runs on either MSP432 or TM4C123
// see GPIO.c file for hardware connections 

// Daniel Valvano and Jonathan Valvano
// November 20, 2016
// CC2650 booster or CC2650 LaunchPad, CC2650 needs to be running SimpleNP 2.2 (POWERSAVE)

#include <stdint.h>
#include "../inc/UART0.h"
#include "../inc/UART1.h" 
#include "../inc/AP.h"
#include "AP_Lab6.h"
//**debug macros**APDEBUG defined in AP.h********
#ifdef APDEBUG
#define OutString(STRING) UART0_OutString(STRING)
#define OutUHex(NUM) UART0_OutUHex(NUM)
#define OutUHex2(NUM) UART0_OutUHex2(NUM)
#define OutChar(N) UART0_OutChar(N)
#else
#define OutString(STRING)
#define OutUHex(NUM)
#define OutUHex2(NUM)
#define OutChar(N)
#endif

//****links into AP.c**************
extern const uint32_t RECVSIZE;
extern uint8_t RecvBuf[];
typedef struct characteristics{
  uint16_t theHandle;          // each object has an ID
  uint16_t size;               // number of bytes in user data (1,2,4,8)
  uint8_t *pt;                 // pointer to user data, stored little endian
  void (*callBackRead)(void);  // action if SNP Characteristic Read Indication
  void (*callBackWrite)(void); // action if SNP Characteristic Write Indication
}characteristic_t;
extern const uint32_t MAXCHARACTERISTICS;
extern uint32_t CharacteristicCount;
extern characteristic_t CharacteristicList[];
typedef struct NotifyCharacteristics{
  uint16_t uuid;               // user defined 
  uint16_t theHandle;          // each object has an ID (used to notify)
  uint16_t CCCDhandle;         // generated/assigned by SNP
  uint16_t CCCDvalue;          // sent by phone to this object
  uint16_t size;               // number of bytes in user data (1,2,4,8)
  uint8_t *pt;                 // pointer to user data array, stored little endian
  void (*callBackCCCD)(void);  // action if SNP CCCD Updated Indication
}NotifyCharacteristic_t;
extern const uint32_t NOTIFYMAXCHARACTERISTICS;
extern uint32_t NotifyCharacteristicCount;
extern NotifyCharacteristic_t NotifyCharacteristicList[];
extern const uint8_t NPI_Register[];
extern const uint8_t NPI_GetVersion[];
extern const uint8_t NPI_GetStatus[];
extern uint8_t NPI_AddService[];
extern uint8_t NPI_AddCharValue[];
extern uint8_t NPI_AddCharDescriptor[];
extern uint8_t NPI_AddCharDescriptor4[];
extern uint8_t NPI_GATTSetDeviceName[];
extern const uint8_t NPI_SetAdvertisement1[];
extern uint8_t NPI_SetAdvertisementData[];
extern const uint8_t NPI_StartAdvertisement[];

//**************Lab 6 routines*******************
// **********GetMsgSize**********
// helper function, returns length of NPI message frame
// Inputs: pointer to message
// Outputs: message length
uint32_t GetMsgSize(const uint8_t *msg) {
  uint32_t payloadSize;
  // 2 bytes little-endian payload length field
  payloadSize = (msg[2] << 8) + msg[1];
  // payload length + 6 bytes for SOF(1) + length(2) + Command(2) + FCS(1) fields
  return payloadSize + 6;
}
// **********SetFCS**************
// helper function, add check byte to message
// assumes every byte in the message has been set except the FCS
// used the length field, assumes less than 256 bytes
// FCS = 8-bit EOR(all bytes except SOF and the FCS itself)
// Inputs: pointer to message
//         stores the FCS into message itself
// Outputs: none
//---MyCode---
void SetFCS(uint8_t *msg){
  uint8_t fcs,
          msgSize,
          i; 

  fcs = 0;
  msgSize = GetMsgSize(msg);
  for (i = 1; i < msgSize-1; i++)
    fcs ^= msg[i];

  msg[msgSize-1] = fcs;
	//---MyCodeEnd---
}
// **********SetLittleEndian**************
// helper function, sets sets two bytes of message to little-endian value
// Inputs: 16-bit value 
//         pointer to message
// Outputs: none
void SetLittleEndian(const uint16_t value, uint8_t *msg) {
  msg[0] = (uint8_t) (value & 0xFF);
  msg[1] = (uint8_t) (value >> 8);
}

// **********StrLen**************
// helper function, calculates length of string
// Inputs: pointer to string
// Outputs: length
uint32_t StrLen(const char *str) {
  uint32_t length;
  const char *ch;

  length = 0;
  for (ch = str; *ch; ch++ )
    length++;

  return length;
}
//*************BuildGetStatusMsg**************
// Create a Get Status message, used in Lab 6
// Inputs pointer to empty buffer of at least 6 bytes
// Output none
// build the necessary NPI message that will Get Status
void BuildGetStatusMsg(uint8_t *msg){
  uint32_t msgSize, i;
  
  msgSize = GetMsgSize(NPI_GetStatus);
  for (i = 0; i < msgSize; i++)
    msg[i] = NPI_GetStatus[i];
}
//*************Lab6_GetStatus**************
// Get status of connection, used in Lab 6
// Input:  none
// Output: status 0xAABBCCDD
// AA is GAPRole Status
// BB is Advertising Status
// CC is ATT Status
// DD is ATT method in progress
uint32_t Lab6_GetStatus(void){volatile int r; uint8_t sendMsg[8];
  OutString("\n\rGet Status");
  BuildGetStatusMsg(sendMsg);
  r = AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  return (RecvBuf[4]<<24)+(RecvBuf[5]<<16)+(RecvBuf[6]<<8)+(RecvBuf[7]);
}

//*************BuildGetVersionMsg**************
// Create a Get Version message, used in Lab 6
// Inputs pointer to empty buffer of at least 6 bytes
// Output none
// build the necessary NPI message that will Get Status
void BuildGetVersionMsg(uint8_t *msg){
  uint32_t msgSize, i;
  
  msgSize = GetMsgSize(NPI_GetVersion);
  for (i = 0; i < msgSize; i++)
    msg[i] = NPI_GetVersion[i];
}
//*************Lab6_GetVersion**************
// Get version of the SNP application running on the CC2650, used in Lab 6
// Input:  none
// Output: version
uint32_t Lab6_GetVersion(void){volatile int r;uint8_t sendMsg[8];
  OutString("\n\rGet Version");
  BuildGetVersionMsg(sendMsg);
  r = AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE); 
  return (RecvBuf[5]<<8)+(RecvBuf[6]);
}

//*************BuildAddServiceMsg**************
// Create an Add service message, used in Lab 6
// Inputs uuid is 0xFFF0, 0xFFF1, ...
//        pointer to empty buffer of at least 9 bytes
// Output none
// build the necessary NPI message that will add a service
void BuildAddServiceMsg(uint16_t uuid, uint8_t *msg){
  uint8_t i;

  for (i = 0; i < 6; i++)   // poulates SOF, Length, Command, Command Parameter fields
    msg[i] = NPI_AddService[i];
  SetLittleEndian(uuid, &msg[6]);
  SetFCS(msg);
}
//*************Lab6_AddService**************
// Add a service, used in Lab 6
// Inputs uuid is 0xFFF0, 0xFFF1, ...
// Output APOK if successful,
//        APFAIL if SNP failure
int Lab6_AddService(uint16_t uuid){ int r; uint8_t sendMsg[12];
  OutString("\n\rAdd service");
  BuildAddServiceMsg(uuid,sendMsg);
  r = AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);  
  return r;
}
//*************AP_BuildRegisterServiceMsg**************
// Create a register service message, used in Lab 6
// Inputs pointer to empty buffer of at least 6 bytes
// Output none
// build the necessary NPI message that will register a service
void BuildRegisterServiceMsg(uint8_t *msg){
  uint32_t msgSize, i;
  
  msgSize = GetMsgSize(NPI_Register);
  for (i = 0; i < msgSize-1; i++)
    msg[i] = NPI_Register[i];
  SetFCS(msg);
}
//*************Lab6_RegisterService**************
// Register a service, used in Lab 6
// Inputs none
// Output APOK if successful,
//        APFAIL if SNP failure
int Lab6_RegisterService(void){ int r; uint8_t sendMsg[8];
  OutString("\n\rRegister service");
  BuildRegisterServiceMsg(sendMsg);
  r = AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  return r;
}

//*************BuildAddCharValueMsg**************
// Create a Add Characteristic Value Declaration message, used in Lab 6
// Inputs uuid is 0xFFF0, 0xFFF1, ...
//        permission is GATT Permission, 0=none,1=read,2=write, 3=Read+write 
//        properties is GATT Properties, 2=read,8=write,0x0A=read+write, 0x10=notify
//        pointer to empty buffer of at least 14 bytes
// Output none
// build the necessary NPI message that will add a characteristic value
void BuildAddCharValueMsg(uint16_t uuid,  
      uint8_t permission, uint8_t properties, uint8_t *msg){
  static const uint16_t AttrValueMaxLength = 512;
  static const uint8_t RFU = 0x00;
  uint8_t i;

  for (i = 0; i < 5; i++)   // poulates SOF, Length, Command fields
    msg[i] = NPI_AddCharValue[i];
  msg[5] = permission; 
  SetLittleEndian(properties, &msg[6]);
  msg[8] = RFU;
  SetLittleEndian(AttrValueMaxLength, &msg[9]);
  SetLittleEndian(uuid, &msg[11]);
  SetFCS(msg);
}

//*************BuildAddCharDescriptorMsg**************
// Create a Add Characteristic Descriptor Declaration message, used in Lab 6
// Inputs name is a null-terminated string, maximum length of name is 20 bytes
//        pointer to empty buffer of at least 32 bytes
// Output none
// build the necessary NPI message that will add a Descriptor Declaration
void BuildAddCharDescriptorMsg(char name[], uint8_t *msg){
  uint8_t stringLength,
          msgLength,
          i;
  char *ch;

  // poulate SOF, Command, Command Parameter fields
  for (i = 0; i < 7; i++)
    msg[i] = NPI_AddCharDescriptor[i]; 
  // calculate string length including null-terminator
  stringLength = (uint8_t) StrLen(name) + 1;
  // Set length field
  msgLength = stringLength + 6;
  SetLittleEndian(msgLength, &msg[1]);
  // Set max length and initial length of user description string
  SetLittleEndian(stringLength, &msg[7]);
  SetLittleEndian(stringLength, &msg[9]);
  // Set user description string data
  for (i = 0, ch = name; i < stringLength; i++, ch++)
    msg[11+i] = *ch;
  SetFCS(msg);
}

//*************Lab6_AddCharacteristic**************
// Add a read, write, or read/write characteristic, used in Lab 6
//        for notify properties, call AP_AddNotifyCharacteristic 
// Inputs uuid is 0xFFF0, 0xFFF1, ...
//        thesize is the number of bytes in the user data 1,2,4, or 8 
//        pt is a pointer to the user data, stored little endian
//        permission is GATT Permission, 0=none,1=read,2=write, 3=Read+write 
//        properties is GATT Properties, 2=read,8=write,0x0A=read+write
//        name is a null-terminated string, maximum length of name is 20 bytes
//        (*ReadFunc) called before it responses with data from internal structure
//        (*WriteFunc) called after it accepts data into internal structure
// Output APOK if successful,
//        APFAIL if name is empty, more than 8 characteristics, or if SNP failure
int Lab6_AddCharacteristic(uint16_t uuid, uint16_t thesize, void *pt, uint8_t permission,
  uint8_t properties, char name[], void(*ReadFunc)(void), void(*WriteFunc)(void)){
  int r; uint16_t handle; 
  uint8_t sendMsg[32];  
  if(thesize>8) return APFAIL;
  if(name[0]==0) return APFAIL;       // empty name
  if(CharacteristicCount>=MAXCHARACTERISTICS) return APFAIL; // error
  BuildAddCharValueMsg(uuid,permission,properties,sendMsg);
  OutString("\n\rAdd CharValue");
  r=AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  if(r == APFAIL) return APFAIL;
  handle = (RecvBuf[7]<<8)+RecvBuf[6]; // handle for this characteristic
  OutString("\n\rAdd CharDescriptor");
  BuildAddCharDescriptorMsg(name,sendMsg);
  r=AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  if(r == APFAIL) return APFAIL;
  CharacteristicList[CharacteristicCount].theHandle = handle;
  CharacteristicList[CharacteristicCount].size = thesize;
  CharacteristicList[CharacteristicCount].pt = (uint8_t *) pt;
  CharacteristicList[CharacteristicCount].callBackRead = ReadFunc;
  CharacteristicList[CharacteristicCount].callBackWrite = WriteFunc;
  CharacteristicCount++;
  return APOK; // OK
} 
  

//*************BuildAddNotifyCharDescriptorMsg**************
// Create a Add Notify Characteristic Descriptor Declaration message, used in Lab 6
// Inputs name is a null-terminated string, maximum length of name is 20 bytes
//        pointer to empty buffer of at least bytes
// Output none
// build the necessary NPI message that will add a Descriptor Declaration
void BuildAddNotifyCharDescriptorMsg(char name[], uint8_t *msg){
  uint8_t stringLength,
          msgLength,
          i;
  char *ch;

  // set SOF, SNP Add Characteristic Descriptor Declaration, User Description 
  // String+CCCD, CCCD parameters read+write, GATT Read Permissions
  for (i = 0; i < 8; i++)
    msg[i] = NPI_AddCharDescriptor4[i]; 
  // calculate string length including null-terminator
  stringLength = (uint8_t) StrLen(name) + 1;
  // Set length field
  msgLength = stringLength + 7;
  SetLittleEndian(msgLength, &msg[1]);
  // Set max length and initial length of user description string
  SetLittleEndian(stringLength, &msg[8]);
  SetLittleEndian(stringLength, &msg[10]);
  // Set user description string data
  for (i = 0, ch = name; i < stringLength; i++, ch++)
    msg[12+i] = *ch;
  SetFCS(msg);
}
  
//*************Lab6_AddNotifyCharacteristic**************
// Add a notify characteristic, used in Lab 6
//        for read, write, or read/write characteristic, call AP_AddCharacteristic 
// Inputs uuid is 0xFFF0, 0xFFF1, ...
//        thesize is the number of bytes in the user data 1,2,4, or 8 
//        pt is a pointer to the user data, stored little endian
//        name is a null-terminated string, maximum length of name is 20 bytes
//        (*CCCDfunc) called after it accepts , changing CCCDvalue
// Output APOK if successful,
//        APFAIL if name is empty, more than 4 notify characteristics, or if SNP failure
int Lab6_AddNotifyCharacteristic(uint16_t uuid, uint16_t thesize, void *pt,   
  char name[], void(*CCCDfunc)(void)){
  int r; uint16_t handle; 
  uint8_t sendMsg[36];  
  if(thesize>8) return APFAIL;
  if(NotifyCharacteristicCount>=NOTIFYMAXCHARACTERISTICS) return APFAIL; // error
  BuildAddCharValueMsg(uuid,0,0x10,sendMsg);
  OutString("\n\rAdd Notify CharValue");
  r=AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  if(r == APFAIL) return APFAIL;
  handle = (RecvBuf[7]<<8)+RecvBuf[6]; // handle for this characteristic
  OutString("\n\rAdd CharDescriptor");
  BuildAddNotifyCharDescriptorMsg(name,sendMsg);
  r=AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  if(r == APFAIL) return APFAIL;
  NotifyCharacteristicList[NotifyCharacteristicCount].uuid = uuid;
  NotifyCharacteristicList[NotifyCharacteristicCount].theHandle = handle;
  NotifyCharacteristicList[NotifyCharacteristicCount].CCCDhandle = (RecvBuf[8]<<8)+RecvBuf[7]; // handle for this CCCD
  NotifyCharacteristicList[NotifyCharacteristicCount].CCCDvalue = 0; // notify initially off
  NotifyCharacteristicList[NotifyCharacteristicCount].size = thesize;
  NotifyCharacteristicList[NotifyCharacteristicCount].pt = (uint8_t *) pt;
  NotifyCharacteristicList[NotifyCharacteristicCount].callBackCCCD = CCCDfunc;
  NotifyCharacteristicCount++;
  return APOK; // OK
}

//*************BuildSetDeviceNameMsg**************
// Create a Set GATT Parameter message, used in Lab 6
// Inputs name is a null-terminated string, maximum length of name is 24 bytes
//        pointer to empty buffer of at least 36 bytes
// Output none
// build the necessary NPI message to set Device name
void BuildSetDeviceNameMsg(char name[], uint8_t *msg){
// for a hint see NPI_GATTSetDeviceNameMsg in VerySimpleApplicationProcessor.c
// for a hint see NPI_GATTSetDeviceName in AP.c
  uint8_t stringLength,
          msgLength,
          i;
  char *ch;

  // set SOF, SNP Set GATT Parameter, Generic Access Service, Device Name fields
  for (i = 0; i < 8; i++)
    msg[i] = NPI_GATTSetDeviceName[i]; 
  stringLength = (uint8_t) StrLen(name);
  // Set length field
  msgLength = stringLength + 3;
  SetLittleEndian(msgLength, &msg[1]);
  // Set device name string data
  for (i = 0, ch = name; i < stringLength; i++, ch++)
    msg[8+i] = *ch;
  SetFCS(msg);
}
//*************BuildSetAdvertisementData1Msg**************
// Create a Set Advertisement Data message, used in Lab 6
// Inputs pointer to empty buffer of at least 16 bytes
// Output none
// build the necessary NPI message for Non-connectable Advertisement Data
void BuildSetAdvertisementData1Msg(uint8_t *msg){
  uint32_t msgSize, i;
  
  msgSize = GetMsgSize(NPI_SetAdvertisement1);
  // copy NPI_SetAdvertisement1 message template
  for (i = 0; i < msgSize; i++)
    msg[i] = NPI_SetAdvertisement1[i];
  SetFCS(msg);
}

//*************BuildSetAdvertisementDataMsg**************
// Create a Set Advertisement Data message, used in Lab 6
// Inputs name is a null-terminated string, maximum length of name is 24 bytes
//        pointer to empty buffer of at least 36 bytes
// Output none
// build the necessary NPI message for Scan Response Data
void BuildSetAdvertisementDataMsg(char name[], uint8_t *msg){
  static const uint8_t ConnectionParameters[] = {
  // connection interval range
    0x05,           // length of this data
    0x12,           // GAP_ADTYPE_SLAVE_CONN_INTERVAL_RANGE
    0x50,0x00,      // DEFAULT_DESIRED_MIN_CONN_INTERVAL
    0x20,0x03,      // DEFAULT_DESIRED_MAX_CONN_INTERVAL
  // Tx power level
    0x02,           // length of this data
    0x0A,           // GAP_ADTYPE_POWER_LEVEL
    0x00            // 0dBm
  };
  uint8_t dataLength,
          msgLength,
          i;
  char *ch;

  // set SOF, SNP Set Advertisement Data, Scan Response Data, type=LOCAL_NAME_COMPLETE
  for (i = 0; i < 8; i++)
    msg[i] = NPI_SetAdvertisementData[i]; 
  // calculate string length
  dataLength = (uint8_t) StrLen(name);
  // Set Advertisement Data field length
  msg[6] = dataLength + 1;
  // Set length field
  msgLength = dataLength + 12;
  SetLittleEndian(msgLength, &msg[1]);
  // Set Advertisement data string
  for (i = 0, ch = name; i < dataLength; i++, ch++)
    msg[8+i] = *ch;
  // Set AdvertisementData connection paramters
  for (i = 0, ch = name; i < 9; i++, ch++)
    msg[8+dataLength+i] = ConnectionParameters[i];
  SetFCS(msg);
}
//*************BuildStartAdvertisementMsg**************
// Create a Start Advertisement Data message, used in Lab 6
// Inputs advertising interval
//        pointer to empty buffer of at least 20 bytes
// Output none
// build the necessary NPI message to start advertisement
void BuildStartAdvertisementMsg(uint16_t interval, uint8_t *msg){
  uint32_t msgSize, i;
  
  msgSize = GetMsgSize(NPI_StartAdvertisement);
  // copy NPI_StartAdvertisement message template
  for (i = 0; i < msgSize; i++)
    msg[i] = NPI_StartAdvertisement[i];
  SetLittleEndian(interval, &msg[8]);
  SetFCS(msg);
}

//*************Lab6_StartAdvertisement**************
// Start advertisement, used in Lab 6
// Input:  none
// Output: APOK if successful,
//         APFAIL if notification not configured, or if SNP failure
int Lab6_StartAdvertisement(void){volatile int r; uint8_t sendMsg[40];
  OutString("\n\rSet Device name");
  BuildSetDeviceNameMsg("Shape the World",sendMsg);
  r =AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  OutString("\n\rSetAdvertisement1");
  BuildSetAdvertisementData1Msg(sendMsg);
  r =AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  OutString("\n\rSetAdvertisement Data");
  BuildSetAdvertisementDataMsg("Shape the World",sendMsg);
  r =AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  OutString("\n\rStartAdvertisement");
  BuildStartAdvertisementMsg(100,sendMsg);
  r =AP_SendMessageResponse(sendMsg,RecvBuf,RECVSIZE);
  return r;
}

