#ifndef __ZIGBEE_H__
#define __ZIGBEE_H__

#include "env.h"

#define Z_EXTADDR_LEN (8ul)

#define SEC_KEY_LEN  (16ul)

// General Clusters
#define ZCL_CLUSTER_ID_GEN_BASIC                             0x0000
#define ZCL_CLUSTER_ID_GEN_POWER_CFG                         0x0001
#define ZCL_CLUSTER_ID_GEN_DEVICE_TEMP_CONFIG                0x0002
#define ZCL_CLUSTER_ID_GEN_IDENTIFY                          0x0003
#define ZCL_CLUSTER_ID_GEN_GROUPS                            0x0004
#define ZCL_CLUSTER_ID_GEN_SCENES                            0x0005
#define ZCL_CLUSTER_ID_GEN_ON_OFF                            0x0006
#define ZCL_CLUSTER_ID_GEN_ON_OFF_SWITCH_CONFIG              0x0007
#define ZCL_CLUSTER_ID_GEN_LEVEL_CONTROL                     0x0008
#define ZCL_CLUSTER_ID_GEN_ALARMS                            0x0009
#define ZCL_CLUSTER_ID_GEN_TIME                              0x000A
#define ZCL_CLUSTER_ID_GEN_LOCATION                          0x000B
#define ZCL_CLUSTER_ID_GEN_ANALOG_INPUT_BASIC                0x000C
#define ZCL_CLUSTER_ID_GEN_ANALOG_OUTPUT_BASIC               0x000D
#define ZCL_CLUSTER_ID_GEN_ANALOG_VALUE_BASIC                0x000E
#define ZCL_CLUSTER_ID_GEN_BINARY_INPUT_BASIC                0x000F
#define ZCL_CLUSTER_ID_GEN_BINARY_OUTPUT_BASIC               0x0010
#define ZCL_CLUSTER_ID_GEN_BINARY_VALUE_BASIC                0x0011
#define ZCL_CLUSTER_ID_GEN_MULTISTATE_INPUT_BASIC            0x0012
#define ZCL_CLUSTER_ID_GEN_MULTISTATE_OUTPUT_BASIC           0x0013
#define ZCL_CLUSTER_ID_GEN_MULTISTATE_VALUE_BASIC            0x0014
#define ZCL_CLUSTER_ID_GEN_COMMISSIONING                     0x0015
#define ZCL_CLUSTER_ID_GEN_PARTITION                         0x0016

#define ZCL_CLUSTER_ID_OTA                                   0x0019

#define ZCL_CLUSTER_ID_GEN_POWER_PROFILE                     0x001A
#define ZCL_CLUSTER_ID_GEN_APPLIANCE_CONTROL                 0x001B

#define ZCL_CLUSTER_ID_GEN_POLL_CONTROL                      0x0020

#define ZCL_CLUSTER_ID_GREEN_POWER                           0x0021

// Retail Clusters
#define ZCL_CLUSTER_ID_MOBILE_DEVICE_CONFIGURATION           0x0022
#define ZCL_CLUSTER_ID_NEIGHBOR_CLEANING                     0x0023
#define ZCL_CLUSTER_ID_NEAREST_GATEWAY                       0x0024
   
// Closures Clusters
#define ZCL_CLUSTER_ID_CLOSURES_SHADE_CONFIG                 0x0100
#define ZCL_CLUSTER_ID_CLOSURES_DOOR_LOCK                    0x0101
#define ZCL_CLUSTER_ID_CLOSURES_WINDOW_COVERING              0x0102

// HVAC Clusters
#define ZCL_CLUSTER_ID_HVAC_PUMP_CONFIG_CONTROL              0x0200
#define ZCL_CLUSTER_ID_HVAC_THERMOSTAT                       0x0201
#define ZCL_CLUSTER_ID_HVAC_FAN_CONTROL                      0x0202
#define ZCL_CLUSTER_ID_HVAC_DIHUMIDIFICATION_CONTROL         0x0203
#define ZCL_CLUSTER_ID_HVAC_USER_INTERFACE_CONFIG            0x0204

// Lighting Clusters
#define ZCL_CLUSTER_ID_LIGHTING_COLOR_CONTROL                0x0300
#define ZCL_CLUSTER_ID_LIGHTING_BALLAST_CONFIG               0x0301

// Measurement and Sensing Clusters
#define ZCL_CLUSTER_ID_MS_ILLUMINANCE_MEASUREMENT            0x0400
#define ZCL_CLUSTER_ID_MS_ILLUMINANCE_LEVEL_SENSING_CONFIG   0x0401
#define ZCL_CLUSTER_ID_MS_TEMPERATURE_MEASUREMENT            0x0402
#define ZCL_CLUSTER_ID_MS_PRESSURE_MEASUREMENT               0x0403
#define ZCL_CLUSTER_ID_MS_FLOW_MEASUREMENT                   0x0404
#define ZCL_CLUSTER_ID_MS_RELATIVE_HUMIDITY                  0x0405
#define ZCL_CLUSTER_ID_MS_OCCUPANCY_SENSING                  0x0406

// Security and Safety (SS) Clusters
#define ZCL_CLUSTER_ID_SS_IAS_ZONE                           0x0500
#define ZCL_CLUSTER_ID_SS_IAS_ACE                            0x0501
#define ZCL_CLUSTER_ID_SS_IAS_WD                             0x0502

// Protocol Interfaces
#define ZCL_CLUSTER_ID_PI_GENERIC_TUNNEL                     0x0600
#define ZCL_CLUSTER_ID_PI_BACNET_PROTOCOL_TUNNEL             0x0601
#define ZCL_CLUSTER_ID_PI_ANALOG_INPUT_BACNET_REG            0x0602
#define ZCL_CLUSTER_ID_PI_ANALOG_INPUT_BACNET_EXT            0x0603
#define ZCL_CLUSTER_ID_PI_ANALOG_OUTPUT_BACNET_REG           0x0604
#define ZCL_CLUSTER_ID_PI_ANALOG_OUTPUT_BACNET_EXT           0x0605
#define ZCL_CLUSTER_ID_PI_ANALOG_VALUE_BACNET_REG            0x0606
#define ZCL_CLUSTER_ID_PI_ANALOG_VALUE_BACNET_EXT            0x0607
#define ZCL_CLUSTER_ID_PI_BINARY_INPUT_BACNET_REG            0x0608
#define ZCL_CLUSTER_ID_PI_BINARY_INPUT_BACNET_EXT            0x0609
#define ZCL_CLUSTER_ID_PI_BINARY_OUTPUT_BACNET_REG           0x060A
#define ZCL_CLUSTER_ID_PI_BINARY_OUTPUT_BACNET_EXT           0x060B
#define ZCL_CLUSTER_ID_PI_BINARY_VALUE_BACNET_REG            0x060C
#define ZCL_CLUSTER_ID_PI_BINARY_VALUE_BACNET_EXT            0x060D
#define ZCL_CLUSTER_ID_PI_MULTISTATE_INPUT_BACNET_REG        0x060E
#define ZCL_CLUSTER_ID_PI_MULTISTATE_INPUT_BACNET_EXT        0x060F
#define ZCL_CLUSTER_ID_PI_MULTISTATE_OUTPUT_BACNET_REG       0x0610
#define ZCL_CLUSTER_ID_PI_MULTISTATE_OUTPUT_BACNET_EXT       0x0611
#define ZCL_CLUSTER_ID_PI_MULTISTATE_VALUE_BACNET_REG        0x0612
#define ZCL_CLUSTER_ID_PI_MULTISTATE_VALUE_BACNET_EXT        0x0613
#define ZCL_CLUSTER_ID_PI_11073_PROTOCOL_TUNNEL              0x0614
#define ZCL_CLUSTER_ID_PI_ISO7818_PROTOCOL_TUNNEL            0x0615
#define ZCL_CLUSTER_ID_PI_RETAIL_TUNNEL                      0x0617

// Smart Energy (SE) Clusters
#define ZCL_CLUSTER_ID_SE_PRICE                              0x0700
#define ZCL_CLUSTER_ID_SE_DRLC                               0x0701
#define ZCL_CLUSTER_ID_SE_METERING                           0x0702
#define ZCL_CLUSTER_ID_SE_MESSAGING                          0x0703
#define ZCL_CLUSTER_ID_SE_TUNNELING                          0x0704
#define ZCL_CLUSTER_ID_SE_PREPAYMENT                         0x0705
#define ZCL_CLUSTER_ID_SE_ENERGY_MGMT                        0x0706
#define ZCL_CLUSTER_ID_SE_CALENDAR                           0x0707
#define ZCL_CLUSTER_ID_SE_DEVICE_MGMT                        0x0708
#define ZCL_CLUSTER_ID_SE_EVENTS                             0x0709
#define ZCL_CLUSTER_ID_SE_MDU_PAIRING                        0x070A
   
#define ZCL_CLUSTER_ID_SE_KEY_ESTABLISHMENT                  0x0800
  
#define ZCL_CLUSTER_ID_TELECOMMUNICATIONS_INFORMATION        0x0900
#define ZCL_CLUSTER_ID_TELECOMMUNICATIONS_VOICE_OVER_ZIGBEE  0x0904
#define ZCL_CLUSTER_ID_TELECOMMUNICATIONS_CHATTING           0x0905
   
#define ZCL_CLUSTER_ID_HA_APPLIANCE_IDENTIFICATION           0x0B00
#define ZCL_CLUSTER_ID_HA_METER_IDENTIFICATION               0x0B01
#define ZCL_CLUSTER_ID_HA_APPLIANCE_EVENTS_ALERTS            0x0B02
#define ZCL_CLUSTER_ID_HA_APPLIANCE_STATISTICS               0x0B03
#define ZCL_CLUSTER_ID_HA_ELECTRICAL_MEASUREMENT             0x0B04
#define ZCL_CLUSTER_ID_HA_DIAGNOSTIC                         0x0B05

// Light Link cluster
#define ZCL_CLUSTER_ID_TOUCHLINK                             0x1000

#define ATTRID_ON_OFF                                     0x0000

#define ATTRID_LEVEL_CURRENT_LEVEL                        0x0000

#define ZCL_CMD_READ                                    0x00
#define ZCL_CMD_READ_RSP                                0x01
#define ZCL_CMD_WRITE                                   0x02
#define ZCL_CMD_WRITE_UNDIVIDED                         0x03
#define ZCL_CMD_WRITE_RSP                               0x04
#define ZCL_CMD_WRITE_NO_RSP                            0x05
#define ZCL_CMD_CONFIG_REPORT                           0x06
#define ZCL_CMD_CONFIG_REPORT_RSP                       0x07
#define ZCL_CMD_READ_REPORT_CFG                         0x08
#define ZCL_CMD_READ_REPORT_CFG_RSP                     0x09
#define ZCL_CMD_REPORT                                  0x0a
#define ZCL_CMD_DEFAULT_RSP                             0x0b
#define ZCL_CMD_DISCOVER_ATTRS                          0x0c
#define ZCL_CMD_DISCOVER_ATTRS_RSP                      0x0d
#define ZCL_CMD_DISCOVER_CMDS_RECEIVED                  0x11
#define ZCL_CMD_DISCOVER_CMDS_RECEIVED_RSP              0x12
#define ZCL_CMD_DISCOVER_CMDS_GEN                       0x13
#define ZCL_CMD_DISCOVER_CMDS_GEN_RSP                   0x14
#define ZCL_CMD_DISCOVER_ATTRS_EXT                      0x15
#define ZCL_CMD_DISCOVER_ATTRS_EXT_RSP                  0x16

#define ZCL_FRAME_CLIENT_SERVER_DIR                     0x00
#define ZCL_FRAME_SERVER_CLIENT_DIR                     0x01

/*** Data Types ***/
#define ZCL_DATATYPE_NO_DATA                            0x00
#define ZCL_DATATYPE_DATA8                              0x08
#define ZCL_DATATYPE_DATA16                             0x09
#define ZCL_DATATYPE_DATA24                             0x0a
#define ZCL_DATATYPE_DATA32                             0x0b
#define ZCL_DATATYPE_DATA40                             0x0c
#define ZCL_DATATYPE_DATA48                             0x0d
#define ZCL_DATATYPE_DATA56                             0x0e
#define ZCL_DATATYPE_DATA64                             0x0f
#define ZCL_DATATYPE_BOOLEAN                            0x10
#define ZCL_DATATYPE_BITMAP8                            0x18
#define ZCL_DATATYPE_BITMAP16                           0x19
#define ZCL_DATATYPE_BITMAP24                           0x1a
#define ZCL_DATATYPE_BITMAP32                           0x1b
#define ZCL_DATATYPE_BITMAP40                           0x1c
#define ZCL_DATATYPE_BITMAP48                           0x1d
#define ZCL_DATATYPE_BITMAP56                           0x1e
#define ZCL_DATATYPE_BITMAP64                           0x1f
#define ZCL_DATATYPE_UINT8                              0x20
#define ZCL_DATATYPE_UINT16                             0x21
#define ZCL_DATATYPE_UINT24                             0x22
#define ZCL_DATATYPE_UINT32                             0x23
#define ZCL_DATATYPE_UINT40                             0x24
#define ZCL_DATATYPE_UINT48                             0x25
#define ZCL_DATATYPE_UINT56                             0x26
#define ZCL_DATATYPE_UINT64                             0x27
#define ZCL_DATATYPE_INT8                               0x28
#define ZCL_DATATYPE_INT16                              0x29
#define ZCL_DATATYPE_INT24                              0x2a
#define ZCL_DATATYPE_INT32                              0x2b
#define ZCL_DATATYPE_INT40                              0x2c
#define ZCL_DATATYPE_INT48                              0x2d
#define ZCL_DATATYPE_INT56                              0x2e
#define ZCL_DATATYPE_INT64                              0x2f
#define ZCL_DATATYPE_ENUM8                              0x30
#define ZCL_DATATYPE_ENUM16                             0x31
#define ZCL_DATATYPE_SEMI_PREC                          0x38
#define ZCL_DATATYPE_SINGLE_PREC                        0x39
#define ZCL_DATATYPE_DOUBLE_PREC                        0x3a
#define ZCL_DATATYPE_OCTET_STR                          0x41
#define ZCL_DATATYPE_CHAR_STR                           0x42
#define ZCL_DATATYPE_LONG_OCTET_STR                     0x43
#define ZCL_DATATYPE_LONG_CHAR_STR                      0x44
#define ZCL_DATATYPE_ARRAY                              0x48
#define ZCL_DATATYPE_STRUCT                             0x4c
#define ZCL_DATATYPE_SET                                0x50
#define ZCL_DATATYPE_BAG                                0x51
#define ZCL_DATATYPE_TOD                                0xe0
#define ZCL_DATATYPE_DATE                               0xe1
#define ZCL_DATATYPE_UTC                                0xe2
#define ZCL_DATATYPE_CLUSTER_ID                         0xe8
#define ZCL_DATATYPE_ATTR_ID                            0xe9
#define ZCL_DATATYPE_BAC_OID                            0xea
#define ZCL_DATATYPE_IEEE_ADDR                          0xf0
#define ZCL_DATATYPE_128_BIT_SEC_KEY                    0xf1
#define ZCL_DATATYPE_UNKNOWN                            0xff

/*** Error Status Codes ***/
#define ZCL_STATUS_SUCCESS                              0x00
#define ZCL_STATUS_FAILURE                              0x01
// 0x02-0x7D are reserved.
#define ZCL_STATUS_NOT_AUTHORIZED                       0x7E
#define ZCL_STATUS_MALFORMED_COMMAND                    0x80
#define ZCL_STATUS_UNSUP_CLUSTER_COMMAND                0x81
#define ZCL_STATUS_UNSUP_GENERAL_COMMAND                0x82
#define ZCL_STATUS_UNSUP_MANU_CLUSTER_COMMAND           0x83
#define ZCL_STATUS_UNSUP_MANU_GENERAL_COMMAND           0x84
#define ZCL_STATUS_INVALID_FIELD                        0x85
#define ZCL_STATUS_UNSUPPORTED_ATTRIBUTE                0x86
#define ZCL_STATUS_INVALID_VALUE                        0x87
#define ZCL_STATUS_READ_ONLY                            0x88
#define ZCL_STATUS_INSUFFICIENT_SPACE                   0x89
#define ZCL_STATUS_DUPLICATE_EXISTS                     0x8a
#define ZCL_STATUS_NOT_FOUND                            0x8b
#define ZCL_STATUS_UNREPORTABLE_ATTRIBUTE               0x8c
#define ZCL_STATUS_INVALID_DATA_TYPE                    0x8d
#define ZCL_STATUS_INVALID_SELECTOR                     0x8e
#define ZCL_STATUS_WRITE_ONLY                           0x8f
#define ZCL_STATUS_INCONSISTENT_STARTUP_STATE           0x90
#define ZCL_STATUS_DEFINED_OUT_OF_BAND                  0x91
#define ZCL_STATUS_INCONSISTENT                         0x92
#define ZCL_STATUS_ACTION_DENIED                        0x93
#define ZCL_STATUS_TIMEOUT                              0x94
#define ZCL_STATUS_ABORT                                0x95
#define ZCL_STATUS_INVALID_IMAGE                        0x96
#define ZCL_STATUS_WAIT_FOR_DATA                        0x97
#define ZCL_STATUS_NO_IMAGE_AVAILABLE                   0x98
#define ZCL_STATUS_REQUIRE_MORE_IMAGE                   0x99

// 0xbd-bf are reserved.
#define ZCL_STATUS_HARDWARE_FAILURE                     0xc0
#define ZCL_STATUS_SOFTWARE_FAILURE                     0xc1
#define ZCL_STATUS_CALIBRATION_ERROR                    0xc2
// 0xc3-0xff are reserved.
#define ZCL_STATUS_CMD_HAS_RSP                          0xFF // Non-standard status (used for Default Rsp)

// Zigbee Home Automation Profile Identification
#define ZCL_HA_PROFILE_ID                                       0x0104

// Generic Device IDs
#define ZCL_HA_DEVICEID_ON_OFF_SWITCH                           0x0000
#define ZCL_HA_DEVICEID_LEVEL_CONTROL_SWITCH                    0x0001
#define ZCL_HA_DEVICEID_ON_OFF_OUTPUT                           0x0002
#define ZCL_HA_DEVICEID_LEVEL_CONTROLLABLE_OUTPUT               0x0003
#define ZCL_HA_DEVICEID_SCENE_SELECTOR                          0x0004
#define ZCL_HA_DEVICEID_CONFIGURATION_TOOL                      0x0005
#define ZCL_HA_DEVICEID_REMOTE_CONTROL                          0x0006
#define ZCL_HA_DEVICEID_COMBINED_INTERFACE                      0x0007
#define ZCL_HA_DEVICEID_RANGE_EXTENDER                          0x0008
#define ZCL_HA_DEVICEID_MAINS_POWER_OUTLET                      0x0009
#define ZCL_HA_DEVICEID_DOOR_LOCK                               0x000A
#define ZCL_HA_DEVICEID_DOOR_LOCK_CONTROLLER                    0x000B
#define ZCL_HA_DEVICEID_SIMPLE_SENSOR                           0x000C
#define ZCL_HA_DEVICEID_CONSUMPTION_AWARENESS_DEVICE            0x000D
#define ZCL_HA_DEVICEID_HOME_GATEWAY                            0x0050
#define ZCL_HA_DEVICEID_SMART_PLUG                              0x0051
#define ZCL_HA_DEVICEID_WHITE_GOODS                             0x0052
#define ZCL_HA_DEVICEID_METER_INTERFACE                         0x0053

// This is a reserved value which could be used for test purposes
#define ZCL_HA_DEVICEID_TEST_DEVICE                             0x00FF

// Lighting Device IDs
#define ZCL_HA_DEVICEID_ON_OFF_LIGHT                            0x0100
#define ZCL_HA_DEVICEID_DIMMABLE_LIGHT                          0x0101
#define ZCL_HA_DEVICEID_COLORED_DIMMABLE_LIGHT                  0x0102
#define ZCL_HA_DEVICEID_ON_OFF_LIGHT_SWITCH                     0x0103
#define ZCL_HA_DEVICEID_DIMMER_SWITCH                           0x0104
#define ZCL_HA_DEVICEID_COLOR_DIMMER_SWITCH                     0x0105
#define ZCL_HA_DEVICEID_LIGHT_SENSOR                            0x0106
#define ZCL_HA_DEVICEID_OCCUPANCY_SENSOR                        0x0107

// Closures Device IDs
#define ZCL_HA_DEVICEID_SHADE                                   0x0200
#define ZCL_HA_DEVICEID_SHADE_CONTROLLER                        0x0201
#define ZCL_HA_DEVICEID_WINDOW_COVERING_DEVICE                  0x0202
#define ZCL_HA_DEVICEID_WINDOW_COVERING_CONTROLLER              0x0203

// HVAC Device IDs
#define ZCL_HA_DEVICEID_HEATING_COOLING_UNIT                    0x0300
#define ZCL_HA_DEVICEID_THERMOSTAT                              0x0301
#define ZCL_HA_DEVICEID_TEMPERATURE_SENSOR                      0x0302
#define ZCL_HA_DEVICEID_PUMP                                    0x0303
#define ZCL_HA_DEVICEID_PUMP_CONTROLLER                         0x0304
#define ZCL_HA_DEVICEID_PRESSURE_SENSOR                         0x0305
#define ZCL_HA_DEVICEID_FLOW_SENSOR                             0x0306
#define ZCL_HA_DEVICEID_MINI_SPLIT_AC                           0x0307

// Intruder Alarm Systems (IAS) Device IDs
#define ZCL_HA_DEVICEID_IAS_CONTROL_INDICATING_EQUIPMENT        0x0400
#define ZCL_HA_DEVICEID_IAS_ANCILLARY_CONTROL_EQUIPMENT         0x0401
#define ZCL_HA_DEVICEID_IAS_ZONE                                0x0402
#define ZCL_HA_DEVICEID_IAS_WARNING_DEVICE                      0x0403

/*******************************/
/*** On/Off Cluster Commands ***/
/*******************************/
#define COMMAND_OFF                                       0x00
#define COMMAND_ON                                        0x01
#define COMMAND_TOGGLE                                    0x02
#define COMMAND_OFF_WITH_EFFECT                           0x40
#define COMMAND_ON_WITH_RECALL_GLOBAL_SCENE               0x41
#define COMMAND_ON_WITH_TIMED_OFF                         0x42

/**************************************/
/*** Level Control Cluster Commands ***/
/**************************************/
#define COMMAND_LEVEL_MOVE_TO_LEVEL                       0x00
#define COMMAND_LEVEL_MOVE                                0x01
#define COMMAND_LEVEL_STEP                                0x02
#define COMMAND_LEVEL_STOP                                0x03
#define COMMAND_LEVEL_MOVE_TO_LEVEL_WITH_ON_OFF           0x04
#define COMMAND_LEVEL_MOVE_WITH_ON_OFF                    0x05
#define COMMAND_LEVEL_STEP_WITH_ON_OFF                    0x06
#define COMMAND_LEVEL_STOP_WITH_ON_OFF                    0x07

/*** Level Control Move (Mode) Command values ***/
#define LEVEL_MOVE_UP                                     0x00
#define LEVEL_MOVE_DOWN                                   0x01

/*** Level Control Step (Mode) Command values ***/
#define LEVEL_STEP_UP                                     0x00
#define LEVEL_STEP_DOWN                                   0x01

/******************************/
/*** Group Cluster Commands ***/
/******************************/
#define COMMAND_GROUP_ADD                                 0x00
#define COMMAND_GROUP_VIEW                                0x01
#define COMMAND_GROUP_GET_MEMBERSHIP                      0x02
#define COMMAND_GROUP_REMOVE                              0x03
#define COMMAND_GROUP_REMOVE_ALL                          0x04
#define COMMAND_GROUP_ADD_IF_IDENTIFYING                  0x05

#define COMMAND_GROUP_ADD_RSP                             0x00
#define COMMAND_GROUP_VIEW_RSP                            0x01
#define COMMAND_GROUP_GET_MEMBERSHIP_RSP                  0x02
#define COMMAND_GROUP_REMOVE_RSP                          0x03

/*******************************/
/*** Scenes Cluster Commands ***/
/*******************************/
#define COMMAND_SCENE_ADD                                 0x00
#define COMMAND_SCENE_VIEW                                0x01
#define COMMAND_SCENE_REMOVE                              0x02
#define COMMAND_SCENE_REMOVE_ALL                          0x03
#define COMMAND_SCENE_STORE                               0x04
#define COMMAND_SCENE_RECALL                              0x05
#define COMMAND_SCENE_GET_MEMBERSHIP                      0x06
#define COMMAND_SCENE_ENHANCED_ADD                        0x40
#define COMMAND_SCENE_ENHANCED_VIEW                       0x41
#define COMMAND_SCENE_COPY                                0x42

#define COMMAND_SCENE_ADD_RSP                             0x00
#define COMMAND_SCENE_VIEW_RSP                            0x01
#define COMMAND_SCENE_REMOVE_RSP                          0x02
#define COMMAND_SCENE_REMOVE_ALL_RSP                      0x03
#define COMMAND_SCENE_STORE_RSP                           0x04
#define COMMAND_SCENE_GET_MEMBERSHIP_RSP                  0x06
#define COMMAND_SCENE_ENHANCED_ADD_RSP                    0x40
#define COMMAND_SCENE_ENHANCED_VIEW_RSP                   0x41
#define COMMAND_SCENE_COPY_RSP                            0x42

// command parameter
#define SCENE_COPY_MODE_ALL_BIT                           0x01

#define afAddrGroup 1
#define afAddr16Bit 2
#define afAddr64Bit 3

#define OPCODE_ZIGBEE_RESPONSE_FAILED 0
#define OPCODE_ZIGBEE_RESPONSE_SUCCESSED 1
#define OPCODE_ZIGBEE_RESPONSE_TIMEOUT 2
#define OPCODE_ZIGBEE_HANDSHAKE 3
#define OPCODE_ZIGBEE_ZDO_SEND 4
#define OPCODE_ZIGBEE_ZCL_SEND 5
#define OPCODE_ZIGBEE_DISCOVER_START 6
#define OPCODE_ZIGBEE_DISCOVER_STOP 7
#define OPCODE_ZIGBEE_DATA_REQUEST 8
#define OPCODE_ZIGBEE_ATTRIBUTE_REPORT 9
#define OPCODE_ZIGBEE_GPIO_DIRECTION_SET 10
#define OPCODE_ZIGBEE_GPIO_SET 11
#define OPCODE_ZIGBEE_GPIO_GET 12
#define OPCODE_ZIGBEE_NV_INIT 13
#define OPCODE_ZIGBEE_NV_WRITE 14
#define OPCODE_ZIGBEE_NV_READ 15
#define OPCODE_ZIGBEE_KEY_CLICK 16
#define OPCODE_ZIGBEE_KEY_PRESS 17
#define OPCODE_ZIGBEE_KEY_POWERUP 18
#define OPCODE_ZIGBEE_LED_SET 19
#define OPCODE_ZIGBEE_EVENT_RESET 20
#define OPCODE_ZIGBEE_EVENT_NETWORK_START 21
#define OPCODE_ZIGBEE_EVENT_DISCOVER_START 22
#define OPCODE_ZIGBEE_EVENT_DISCOVER_STOP 23
#define OPCODE_ZIGBEE_EVENT_ZDO 24
#define OPCODE_ZIGBEE_EVENT_BATTERY_WARNING 25
#define OPCODE_ZIGBEE_EVENT_COMMISSIONING_STATUS 26
#define OPCODE_ZIGBEE_EVENT_IDENTIFY_TIME_CHANGE 27
#define OPCODE_ZIGBEE_EVENT_DEBUG 28
#define OPCODE_ZIGBEE_EVENT_CONFIRM 29
#define OPCODE_ZIGBEE_EVENT_GROUP_RSP 30
#define OPCODE_ZIGBEE_EVENT_SCENE_RSP 31

#define EVENT_ATTRIBUTE_REPORT 253
#define EVENT_DEVICE_ANNCE 254

typedef struct 
{
    uint8_t mode;
    union {
        uint16_t sht;
        uint16_t group;
        uint8_t *ieee;
    }addr;
    uint16_t cluster;

    uint8_t status;
    uint16_t node_addr;

    union
    {
        struct
        {
            uint8_t *ieee;
            uint8_t capabilities;
        }device_annouce;
        struct
        {
            uint8_t ep;
            uint16_t profile;
            uint16_t type;
            uint8_t version;
            uint8_t count_in;
            uint16_t *cluster_in;
            uint8_t count_out;
            uint16_t *cluster_out;
        }simple_desc;
    }payload;
}ZigbeeZDOEvent;

typedef struct
{
    uint8_t mode;
    union {
        uint16_t sht;
        uint16_t group;
        uint8_t *ieee;
    }addr;
    uint16_t cluster;

    uint8_t ep;

    union
    {
        struct
        {
            uint8_t count;
            uint8_t *attrs;
        }read;
        struct
        {
            uint8_t count;
            uint8_t *attrs;
        }report;
    }attribute;
}ZigbeeZCLEvent;

typedef void (*ZigbeeResponse)(void *ctx, uint8_t status, uint8_t retry, uint8_t *data, uint8_t size);

uint8_t zclGetDataTypeLength( uint8_t dataType );
uint16_t zclGetAttrDataLength( uint8_t dataType, uint8_t *pData );

void zigbee_init(void (*listener)(uint8_t event, void *data, uint8_t size));
void zigbee_update(void);

int zigbee_idle();

uint8_t zigbee_session(void);
int zigbee_request(uint8_t opcode, uint8_t session, uint8_t *data, uint8_t size, uint8_t retry, uint16_t timeout, ZigbeeResponse response, void *ctx);

int zigbee_discover_start(void);
int zigbee_discover_stop(void);

void zigbee_active_ep(uint16_t addr, void (*rsp)(int ret, uint16_t addr, uint8_t *eps, uint8_t count));
void zigbee_simple_desc(uint16_t addr, uint8_t ep, void (*rsp)(int ret, 
    uint16_t addr, uint8_t ep, 
    uint16_t profile, uint16_t type, 
    uint8_t version, 
    uint8_t *ins, uint8_t in_count, 
    uint8_t *outs, uint8_t out_count));
void zigbee_attribute_read(uint16_t addr, uint8_t ep, uint16_t cluster, uint16_t attribute, void (*rsp)(int ret, 
    uint16_t addr, uint8_t ep,
    uint16_t cluster, uint16_t attribute,
    uint8_t *attrs, uint8_t count));

void *zigbee_zcl_command(
    uint16_t addr, uint8_t ep, 
    uint16_t cluster, uint8_t command, 
    uint8_t frame_type,
    const void *payload, uint8_t size, 
    void (*rsp)(int ret, void *ctx, uint16_t addr, uint8_t ep, uint16_t cluster, uint8_t command, uint8_t *data, uint8_t size), uint8_t ctx_size);
void *zigbee_zcl_command_mcast(
    uint16_t addr, uint8_t ep, 
    uint16_t cluster, uint8_t command, 
    uint8_t frame_type,
    const void *payload, uint8_t size, 
    void (*rsp)(int ret, void *ctx, uint16_t addr, uint8_t ep, uint16_t cluster, uint8_t command, uint8_t *data, uint8_t size), uint8_t ctx_size);

#endif
