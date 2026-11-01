/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 - 2017 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _USB_DEVICE_DESCRIPTOR_H_
#define _USB_DEVICE_DESCRIPTOR_H_ 1

#include "usb_audio_config.h"

#define DESCRIPTORS_SIZE_MAX 512

typedef struct _usb_descriptor_header
{
    uint8_t bLength;
    uint8_t bDescriptorType;
} __attribute__((packed)) usb_descriptor_header_t;

typedef struct _usb_configuration_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint16_t wTotalLength;
    uint8_t bNumInterfaces;
    uint8_t bConfigurationValue;
    uint8_t iConfiguration;
    uint8_t bmAttributes;
    uint8_t bMaxPower;
    uint8_t descriptors[DESCRIPTORS_SIZE_MAX];
} __attribute__((packed)) usb_configuration_descriptor_t;

typedef struct _usb_interface_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bInterfaceNumber;
    uint8_t bAlternateSetting;
    uint8_t bNumEndpoints;
    uint8_t bInterfaceClass;
    uint8_t bInterfaceSubClass;
    uint8_t bInterfaceProtocol;
    uint8_t iInterface;
} __attribute__((packed)) usb_interface_descriptor_t;

typedef struct _usb_interface_association_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bFirstInterface;
    uint8_t bInterfaceCount;
    uint8_t bFunctionClass;
    uint8_t bFunctionSubClass;
    uint8_t bFunctionProtocol;
    uint8_t iFunction;
} __attribute__((packed)) usb_interface_association_descriptor_t;

typedef struct _usb_endpoint_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bEndpointAddress;
    uint8_t bmAttributes;
    uint8_t wMaxPacketSize[2];
    uint8_t bInterval;
} __attribute__((packed)) usb_endpoint_descriptor_t;

/* Class Specific Audio Control Interface Descriptor */
typedef struct _usb_cs_ac_interface_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bcdADC[2];
    uint8_t bCategory;
    uint8_t wTotalLength[2];
    uint8_t bmControls;
} __attribute__((packed)) usb_cs_ac_interface_descriptor_t;

typedef struct _usb_clock_source_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bClockID;
    uint8_t bmAttributes;
    uint8_t bmControls;
    uint8_t bAssocTerminal;
    uint8_t iClockSource;
} __attribute__((packed)) usb_clock_source_descriptor_t;

typedef struct _usb_input_terminal_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bTerminalID;
    uint8_t wTerminalType[2];
    uint8_t bAssocTerminal;
    uint8_t bCSourceID;
    uint8_t bNrChannels;
    uint8_t bmChannelConfig[4];
    uint8_t iChannelNames;
    uint8_t bmControls[2];
    uint8_t iTerminal;
} __attribute__((packed)) usb_input_terminal_descriptor_t;

typedef struct _usb_output_terminal_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bTerminalID;
    uint8_t wTerminalType[2];
    uint8_t bAssocTerminal;
    uint8_t bSourceID;
    uint8_t bCSourceID;
    uint8_t bmControls[2];
    uint8_t iTerminal;
} __attribute__((packed)) usb_output_terminal_descriptor_t;

typedef struct _usb_cs_as_interface_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bTerminalLink;
    uint8_t bmControls;
    uint8_t bFormatType;
    uint8_t bmFormats[4];
    uint8_t bNrChannels;
    uint8_t bmChannelConfig[4];
    uint8_t iChannelNames;
} __attribute__((packed)) usb_cs_as_interface_descriptor_t;

typedef struct _usb_as_format_1_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bFormatType;
    uint8_t bSubslotSize;
    uint8_t bBitResolution;
} __attribute__((packed)) usb_as_format_1_descriptor_t;

typedef struct _usb_cs_as_endpoint_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bmAttributes;
    uint8_t bmControls;
    uint8_t bLockDelayUnits;
    uint8_t wLockDelay[2];
} __attribute__((packed)) usb_cs_as_endpoint_descriptor_t;

typedef struct _usb_cdc_header_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bcdCDC[2];
} __attribute__((packed)) usb_cdc_header_descriptor_t;

typedef struct _usb_cdc_acm_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bmCapabilities;
} __attribute__((packed)) usb_cdc_acm_descriptor_t;

typedef struct _usb_cdc_cm_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bmCapabilities;
    uint8_t bDataInterface;
} __attribute__((packed)) usb_cdc_cm_descriptor_t;

typedef struct _usb_cdc_union_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bControlInterface;
    uint8_t bSubordinateInterface0;
} __attribute__((packed)) usb_cdc_union_descriptor_t;

typedef struct _usb_cs_midi_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bcdMSC[2];
    uint8_t wTotalLength[2];
} __attribute__((packed)) usb_cs_midi_descriptor_t;

typedef struct _usb_midi_in_jack_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bJackType;
    uint8_t bJackID;
    uint8_t iJack;
} __attribute__((packed)) usb_midi_in_jack_descriptor_t;

typedef struct _usb_midi_out_jack_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bJackType;
    uint8_t bJackID;
    uint8_t bNrInputPins;
    uint8_t baSourceID0;
    uint8_t BaSourcePin0;
    uint8_t iJack;
} __attribute__((packed)) usb_midi_out_jack_descriptor_t;

typedef struct _usb_midi_element_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bElementID;
    uint8_t bNrInputPins;
    uint8_t baSourceID0;
    uint8_t BaSourcePin0;
    uint8_t bNrOutputPins;
    uint8_t bInTerminalLink;
    uint8_t bOutTerminalLink;
    uint8_t bElCapsSize;
    uint8_t bmElementCaps0;
    uint8_t iElement;
} __attribute__((packed)) usb_midi_element_descriptor_t;

/** This is weird endpoint extension but that's how it is in midi1.0 specification */
typedef struct _usb_midi_endpoint_descriptor
{
    usb_endpoint_descriptor_t ep;
    uint8_t bRefresh;
    uint8_t bSynchAddress;
} __attribute__((packed)) usb_midi_endpoint_descriptor_t;

typedef struct _usb_midi_cs_ep_descriptor
{
    uint8_t bLength;
    uint8_t bDescriptorType;
    uint8_t bDescriptorSubType;
    uint8_t bNumEmbMIDIJack;
    uint8_t bAssocJackID0;
} __attribute__((packed)) usb_midi_cs_ep_descriptor_t;

typedef struct _usb_midi_interface_descriptor
{
    usb_interface_descriptor_t iface;
    usb_cs_midi_descriptor_t cs_iface;
    usb_midi_in_jack_descriptor_t jack0;
    usb_midi_in_jack_descriptor_t jack1;
    usb_midi_out_jack_descriptor_t jack2;
    usb_midi_out_jack_descriptor_t jack3;
    usb_midi_endpoint_descriptor_t ep0;
    usb_midi_cs_ep_descriptor_t cs_ep0;
    usb_midi_endpoint_descriptor_t ep1;
    usb_midi_cs_ep_descriptor_t cs_ep1;
} __attribute__((packed)) usb_midi_interface_descriptor_t;

typedef struct _usb_audio_in_interface_descriptor
{
    usb_interface_association_descriptor_t iad;
    usb_interface_descriptor_t ac_iface;
    usb_cs_ac_interface_descriptor_t cs_ac_iface;
    usb_clock_source_descriptor_t clock_source;
    usb_input_terminal_descriptor_t input_terminal;
    usb_output_terminal_descriptor_t output_terminal;
    usb_interface_descriptor_t as_iface_0;
    usb_interface_descriptor_t as_iface_1;
    usb_cs_as_interface_descriptor_t cs_as_iface;
    usb_as_format_1_descriptor_t format;
    usb_endpoint_descriptor_t as_ep;
    usb_cs_as_endpoint_descriptor_t cs_as_ep;
} __attribute__((packed)) usb_audio_in_interface_descriptor_t;

typedef struct _usb_audio_out_interface_descriptor
{
    usb_interface_association_descriptor_t iad;
    usb_interface_descriptor_t ac_iface;
    usb_cs_ac_interface_descriptor_t cs_ac_iface;
    usb_clock_source_descriptor_t clock_source;
    usb_input_terminal_descriptor_t input_terminal;
    usb_output_terminal_descriptor_t output_terminal;
    usb_interface_descriptor_t as_iface_0;
    usb_interface_descriptor_t as_iface_1;
    usb_cs_as_interface_descriptor_t cs_as_iface;
    usb_as_format_1_descriptor_t format;
    usb_endpoint_descriptor_t as_ep;
    usb_cs_as_endpoint_descriptor_t cs_as_ep;
    usb_endpoint_descriptor_t as_fdb_ep;
} __attribute__((packed)) usb_audio_out_interface_descriptor_t;

typedef struct _usb_audio_in_out_interface_descriptor
{
    usb_interface_association_descriptor_t iad;
    usb_interface_descriptor_t ac_iface;
    usb_cs_ac_interface_descriptor_t cs_ac_iface;
    usb_clock_source_descriptor_t clock_source;
    usb_input_terminal_descriptor_t input_terminal0;
    usb_output_terminal_descriptor_t output_terminal0;
    usb_input_terminal_descriptor_t input_terminal1;
    usb_output_terminal_descriptor_t output_terminal1;
    usb_interface_descriptor_t as_iface0_0;
    usb_interface_descriptor_t as_iface0_1;
    usb_cs_as_interface_descriptor_t cs_as_iface0;
    usb_as_format_1_descriptor_t format0;
    usb_endpoint_descriptor_t as_ep0;
    usb_cs_as_endpoint_descriptor_t cs_as_ep0;
    usb_interface_descriptor_t as_iface1_0;
    usb_interface_descriptor_t as_iface1_1;
    usb_cs_as_interface_descriptor_t cs_as_iface1;
    usb_as_format_1_descriptor_t format1;
    usb_endpoint_descriptor_t as_ep1;
    usb_cs_as_endpoint_descriptor_t cs_as_ep1;
    usb_endpoint_descriptor_t as_fdb_ep;
} __attribute__((packed)) usb_audio_in_out_interface_descriptor_t;

typedef struct _usb_cdc_interface_descriptor
{
    usb_interface_association_descriptor_t iad;
    usb_interface_descriptor_t iface0;
    usb_cdc_header_descriptor_t func0;
    usb_cdc_cm_descriptor_t func1;
    usb_cdc_acm_descriptor_t func2;
    usb_cdc_union_descriptor_t func3;
    usb_endpoint_descriptor_t ep0;
    usb_interface_descriptor_t iface1;
    usb_endpoint_descriptor_t ep1_0;
    usb_endpoint_descriptor_t ep1_1;
} __attribute__((packed)) usb_cdc_interface_descriptor_t;

typedef struct _usb_msc_interface_descriptor
{
    usb_interface_descriptor_t iface;
    usb_endpoint_descriptor_t ep0;
    usb_endpoint_descriptor_t ep1;
} __attribute__((packed)) usb_msc_interface_descriptor_t;

/*******************************************************************************
 * Definitions
 ******************************************************************************/
#define USB_DEVICE_SPECIFIC_BCD_VERSION (0x0200)
#define USB_DEVICE_DEMO_BCD_VERSION     (0x0101U)

#ifdef TRACKER_MINI
#define USB_DEVICE_VID (0x16D0U)
#define USB_DEVICE_PID (0x1118U)
#else // TRACKER_PLUS
#define USB_DEVICE_VID (0x16D0U)
#define USB_DEVICE_PID (0x1119U)
#endif

/* Communication Class SubClass Codes */
#define USB_CDC_DIRECT_LINE_CONTROL_MODEL         (0x01)
#define USB_CDC_ABSTRACT_CONTROL_MODEL            (0x02)
#define USB_CDC_TELEPHONE_CONTROL_MODEL           (0x03)
#define USB_CDC_MULTI_CHANNEL_CONTROL_MODEL       (0x04)
#define USB_CDC_CAPI_CONTROL_MOPDEL               (0x05)
#define USB_CDC_ETHERNET_NETWORKING_CONTROL_MODEL (0x06)
#define USB_CDC_ATM_NETWORKING_CONTROL_MODEL      (0x07)
#define USB_CDC_WIRELESS_HANDSET_CONTROL_MODEL    (0x08)
#define USB_CDC_DEVICE_MANAGEMENT                 (0x09)
#define USB_CDC_MOBILE_DIRECT_LINE_MODEL          (0x0A)
#define USB_CDC_OBEX                              (0x0B)
#define USB_CDC_ETHERNET_EMULATION_MODEL          (0x0C)

/* Communication Class Protocol Codes */
#define USB_CDC_NO_CLASS_SPECIFIC_PROTOCOL  (0x00) /*also for Data Class Protocol Code */
#define USB_CDC_AT_250_PROTOCOL             (0x01)
#define USB_CDC_AT_PCCA_101_PROTOCOL        (0x02)
#define USB_CDC_AT_PCCA_101_ANNEX_O         (0x03)
#define USB_CDC_AT_GSM_7_07                 (0x04)
#define USB_CDC_AT_3GPP_27_007              (0x05)
#define USB_CDC_AT_TIA_CDMA                 (0x06)
#define USB_CDC_ETHERNET_EMULATION_PROTOCOL (0x07)
#define USB_CDC_EXTERNAL_PROTOCOL           (0xFE)
#define USB_CDC_VENDOR_SPECIFIC             (0xFF) /*also for Data Class Protocol Code */

/* Data Class Protocol Codes */
#define USB_CDC_PYHSICAL_INTERFACE_PROTOCOL (0x30)
#define USB_CDC_HDLC_PROTOCOL               (0x31)
#define USB_CDC_TRANSPARENT_PROTOCOL        (0x32)
#define USB_CDC_MANAGEMENT_PROTOCOL         (0x50)
#define USB_CDC_DATA_LINK_Q931_PROTOCOL     (0x51)
#define USB_CDC_DATA_LINK_Q921_PROTOCOL     (0x52)
#define USB_CDC_DATA_COMPRESSION_V42BIS     (0x90)
#define USB_CDC_EURO_ISDN_PROTOCOL          (0x91)
#define USB_CDC_RATE_ADAPTION_ISDN_V24      (0x92)
#define USB_CDC_CAPI_COMMANDS               (0x93)
#define USB_CDC_HOST_BASED_DRIVER           (0xFD)
#define USB_CDC_UNIT_FUNCTIONAL             (0xFE)

/* Descriptor SubType in Communications Class Functional Descriptors */
#define USB_CDC_HEADER_FUNC_DESC              (0x00)
#define USB_CDC_CALL_MANAGEMENT_FUNC_DESC     (0x01)
#define USB_CDC_ABSTRACT_CONTROL_FUNC_DESC    (0x02)
#define USB_CDC_DIRECT_LINE_FUNC_DESC         (0x03)
#define USB_CDC_TELEPHONE_RINGER_FUNC_DESC    (0x04)
#define USB_CDC_TELEPHONE_REPORT_FUNC_DESC    (0x05)
#define USB_CDC_UNION_FUNC_DESC               (0x06)
#define USB_CDC_COUNTRY_SELECT_FUNC_DESC      (0x07)
#define USB_CDC_TELEPHONE_MODES_FUNC_DESC     (0x08)
#define USB_CDC_TERMINAL_FUNC_DESC            (0x09)
#define USB_CDC_NETWORK_CHANNEL_FUNC_DESC     (0x0A)
#define USB_CDC_PROTOCOL_UNIT_FUNC_DESC       (0x0B)
#define USB_CDC_EXTENSION_UNIT_FUNC_DESC      (0x0C)
#define USB_CDC_MULTI_CHANNEL_FUNC_DESC       (0x0D)
#define USB_CDC_CAPI_CONTROL_FUNC_DESC        (0x0E)
#define USB_CDC_ETHERNET_NETWORKING_FUNC_DESC (0x0F)
#define USB_CDC_ATM_NETWORKING_FUNC_DESC      (0x10)
#define USB_CDC_WIRELESS_CONTROL_FUNC_DESC    (0x11)
#define USB_CDC_MOBILE_DIRECT_LINE_FUNC_DESC  (0x12)
#define USB_CDC_MDLM_DETAIL_FUNC_DESC         (0x13)
#define USB_CDC_DEVICE_MANAGEMENT_FUNC_DESC   (0x14)
#define USB_CDC_OBEX_FUNC_DESC                (0x15)
#define USB_CDC_COMMAND_SET_FUNC_DESC         (0x16)
#define USB_CDC_COMMAND_SET_DETAIL_FUNC_DESC  (0x17)
#define USB_CDC_TELEPHONE_CONTROL_FUNC_DESC   (0x18)
#define USB_CDC_OBEX_SERVICE_ID_FUNC_DESC     (0x19)

/* usb descriptor length */
#define USB_DESCRIPTOR_LENGTH_CONFIGURATION_ALL (sizeof(g_UsbDeviceConfigurationDescriptor))
#define USB_MSC_DISK_REPORT_DESCRIPTOR_LENGTH   (63)
#define USB_CDC_VCOM_REPORT_DESCRIPTOR_LENGTH   (33)
#define USB_IAD_DESC_SIZE                       (8)
#define USB_DESCRIPTOR_LENGTH_CDC_HEADER_FUNC   (5)
#define USB_DESCRIPTOR_LENGTH_CDC_CALL_MANAG    (5)
#define USB_DESCRIPTOR_LENGTH_CDC_ABSTRACT      (4)
#define USB_DESCRIPTOR_LENGTH_CDC_UNION_FUNC    (5)

#define USB_DEVICE_CONFIGURATION_COUNT (1)
#define USB_DEVICE_STRING_COUNT        (7)
#define USB_DEVICE_LANGUAGE_COUNT      (1)
#define USB_INTERFACE_COUNT            (7)

#define USB_COMPOSITE_CONFIGURE_INDEX (1)

/* Configuration, interface and endpoint. */
#define USB_CDC_VCOM_CIC_CLASS    (0x02)
#define USB_CDC_VCOM_CIC_SUBCLASS (0x02)
#define USB_CDC_VCOM_CIC_PROTOCOL (0x00)
#define USB_CDC_VCOM_DIC_CLASS    (0x0A)
#define USB_CDC_VCOM_DIC_SUBCLASS (0x00)
#define USB_CDC_VCOM_DIC_PROTOCOL (0x00)

#define USB_CDC_VCOM_INTERFACE_COUNT               (2)
#define USB_CDC_VCOM_CIC_INTERFACE_INDEX           (5)
#define USB_CDC_VCOM_DIC_INTERFACE_INDEX           (6)
#define USB_CDC_VCOM_CIC_INTERFACE_ALTERNATE_COUNT (1)
#define USB_CDC_VCOM_DIC_INTERFACE_ALTERNATE_COUNT (1)
#define USB_CDC_VCOM_CIC_INTERFACE_ALTERNATE_0     (0)
#define USB_CDC_VCOM_DIC_INTERFACE_ALTERNATE_0     (0)
#define USB_CDC_VCOM_CIC_ENDPOINT_COUNT            (1)
#define USB_CDC_VCOM_CIC_INTERRUPT_IN_ENDPOINT     (5)
#define USB_CDC_VCOM_DIC_ENDPOINT_COUNT            (2)
#define USB_CDC_VCOM_DIC_BULK_IN_ENDPOINT          (6)
#define USB_CDC_VCOM_DIC_BULK_OUT_ENDPOINT         (6)

/* Packet size. */
#define HS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE (16)
#define FS_CDC_VCOM_INTERRUPT_IN_PACKET_SIZE (16)
#define HS_CDC_VCOM_INTERRUPT_IN_INTERVAL    (0x07) /* 2^(7-1) = 8ms */
#define FS_CDC_VCOM_INTERRUPT_IN_INTERVAL    (0x08)

#define HS_CDC_VCOM_BULK_IN_PACKET_SIZE  (512)
#define FS_CDC_VCOM_BULK_IN_PACKET_SIZE  (64)
#define HS_CDC_VCOM_BULK_OUT_PACKET_SIZE (512)
#define FS_CDC_VCOM_BULK_OUT_PACKET_SIZE (64)

/* String descriptor length. */
#define USB_DESCRIPTOR_LENGTH_STRING0 (sizeof(g_UsbDeviceString0))
#define USB_DESCRIPTOR_LENGTH_STRING1 (sizeof(g_UsbDeviceString1))
#define USB_DESCRIPTOR_LENGTH_STRING2 (sizeof(g_UsbDeviceString2))
#define USB_DESCRIPTOR_LENGTH_STRING3 (sizeof(g_UsbDeviceString3))

#define USB_DESCRIPTOR_TYPE_CDC_CS_INTERFACE (0x24)
#define USB_DESCRIPTOR_TYPE_CDC_CS_ENDPOINT  (0x25)

#define USB_DESCRIPTOR_TYPE_CS_INTERFACE   (0x24)
#define USB_DESCRIPTOR_TYPE_CS_ENDPOINT    (0x25)
#define USB_DESCRIPTOR_LENGTH_MS_INTERFACE (0x07U)
#define USB_DESCRIPTOR_SUBTYPE_MS_HEADER   (0x01)
#define USB_MIDI_RELEASE                   (0x0100U)
#define USB_MIDI_CABLES                    (0x01)

#define USB_DESCRIPTOR_LENGTH_MIDI_IN_JACK  (0x06U)
#define USB_DESCRIPTOR_SUBTYPE_MIDI_IN_JACK (0x02U)

#define USB_DESCRIPTOR_JACK_TYPE_EMBEDDED (0x01)
#define USB_DESCRIPTOR_JACK_TYPE_EXTERNAL (0x02)

#define USB_DESCRIPTOR_LENGTH_MIDI_OUT_JACK  (0x09U)
#define USB_DESCRIPTOR_SUBTYPE_MIDI_OUT_JACK (0x03U)

#define USB_DESCRIPTOR_LENGTH_MIDI_ENDPOINT (0x09U)

#define USB_MIDI_CLASS    (0x01)
#define USB_MIDI_SUBCLASS (0x03)
#define USB_MIDI_PROTOCOL (0x0)

#define USB_MIDI_INTERFACE_COUNT           (1)
#define USB_MIDI_INTERFACE_INDEX           (0)
#define USB_MIDI_INTERFACE_ALTERNATE_COUNT (1)
#define USB_MIDI_INTERFACE_ALTERNATE_0     (0)
#define USB_MIDI_ENDPOINT_COUNT            (2)
#define USB_MIDI_BULK_IN_ENDPOINT          (1)
#define USB_MIDI_BULK_OUT_ENDPOINT         (1)

#define HS_MIDI_BULK_IN_PACKET_SIZE  (512)
#define FS_MIDI_BULK_IN_PACKET_SIZE  (64)
#define HS_MIDI_BULK_OUT_PACKET_SIZE (512)
#define FS_MIDI_BULK_OUT_PACKET_SIZE (64)

#define USB_MIDI_DESCRIPTOR_LENGTH                                                                 \
    (USB_DESCRIPTOR_LENGTH_INTERFACE + USB_DESCRIPTOR_LENGTH_MS_INTERFACE +                        \
     2 * USB_DESCRIPTOR_LENGTH_MIDI_IN_JACK + 2 * USB_DESCRIPTOR_LENGTH_MIDI_OUT_JACK +            \
     2 * USB_DESCRIPTOR_LENGTH_MIDI_ENDPOINT + 2 * (4 + USB_MIDI_CABLES))

#define USB_MSC_DISK_CLASS (0x08)
/* scsi command set */
#define USB_MSC_DISK_SUBCLASS (0x06)
/* bulk only transport protocol */
#define USB_MSC_DISK_PROTOCOL (0x50)

#define USB_MSC_DISK_INTERFACE_COUNT           (1)
#define USB_MSC_DISK_INTERFACE_INDEX           (4)
#define USB_MSC_DISK_INTERFACE_ALTERNATE_COUNT (1)
#define USB_MSC_DISK_INTERFACE_ALTERNATE_0     (0)
#define USB_MSC_DISK_ENDPOINT_COUNT            (2)
#define USB_MSC_DISK_BULK_IN_ENDPOINT          (4)
#define USB_MSC_DISK_BULK_OUT_ENDPOINT         (4)

#define HS_MSC_DISK_BULK_IN_PACKET_SIZE  (512)
#define FS_MSC_DISK_BULK_IN_PACKET_SIZE  (64)
#define HS_MSC_DISK_BULK_OUT_PACKET_SIZE (512)
#define FS_MSC_DISK_BULK_OUT_PACKET_SIZE (64)

#define USB_ENDPOINT_AUDIO_DESCRIPTOR_LENGTH               (7)
#define USB_AUDIO_CLASS_SPECIFIC_ENDPOINT_LENGTH           (8)
#define USB_AUDIO_CLOCK_SOURCE_LENGTH                      (8)
#define USB_DESCRIPTOR_LENGTH_AC_INTERRUPT_ENDPOINT        (9)
#define USB_AUDIO_CONTROL_INTERFACE_HEADER_LENGTH          (9)
#define USB_AUDIO_INPUT_TERMINAL_ONLY_DESC_SIZE            (17)
#define USB_AUDIO_OUTPUT_TERMINAL_ONLY_DESC_SIZE           (12)
#define USB_AUDIO_FEATURE_UNIT_ONLY_DESC_SIZE(ch, n)       (0x05 + (ch * n) + 1)
#define USB_AUDIO_STREAMING_IFACE_DESC_SIZE                (16)
#define USB_AUDIO_STREAMING_ENDP_DESC_SIZE                 (8)
#define USB_AUDIO_STANDARD_AS_ISO_FEEDBACK_ENDPOINT_LENGTH (7)
#define USB_AUDIO_STREAMING_TYPE_I_DESC_SIZE               (6)

#define USB_AUDIO_RECORDER_ENDPOINT_COUNT  (1)
#define USB_AUDIO_PLAYER_ENDPOINT_COUNT    (1)
#define USB_AUDIO_RECORDER_INTERFACE_COUNT (2)
#define USB_AUDIO_PLAYER_INTERFACE_COUNT   (2)
#define USB_AUDIO_INTERFACE_COUNT          (3)

#define USB_AUDIO_CONTROL_INTERFACE_INDEX         (1)
#define USB_AUDIO_RECORDER_STREAM_INTERFACE_INDEX (2)
#define USB_AUDIO_PLAYER_STREAM_INTERFACE_INDEX   (3)

#define USB_AUDIO_CONTROL_INTERFACE_ALTERNATE_COUNT         (1)
#define USB_AUDIO_RECORDER_STREAM_INTERFACE_ALTERNATE_COUNT (2)
#define USB_AUDIO_PLAYER_STREAM_INTERFACE_ALTERNATE_COUNT   (2)
#define USB_AUDIO_CONTROL_INTERFACE_ALTERNATE_0             (0)
#define USB_AUDIO_RECORDER_STREAM_INTERFACE_ALTERNATE_0     (0)
#define USB_AUDIO_RECORDER_STREAM_INTERFACE_ALTERNATE_1     (1)
#define USB_AUDIO_PLAYER_STREAM_INTERFACE_ALTERNATE_0       (0)
#define USB_AUDIO_PLAYER_STREAM_INTERFACE_ALTERNATE_1       (1)

#define USB_AUDIO_CONTROL_ENDPOINT_COUNT         (0)
#define USB_AUDIO_RECORDER_STREAM_ENDPOINT_COUNT (1)
#define USB_AUDIO_PLAYER_STREAM_ENDPOINT_COUNT   (2)

#define USB_AUDIO_RECORDER_STREAM_ENDPOINT (2)
#define USB_AUDIO_PLAYER_STREAM_ENDPOINT   (3)
/* the same number as IN endpoint which it synchronizes */
#define USB_AUDIO_PLAYER_FEEDBACK_ENDPOINT (3)

#define USB_AUDIO_COMPOSITE_INTERFACE_COUNT                                                        \
    (USB_AUDIO_RECORDER_CONTROL_INTERFACE_COUNT + USB_AUDIO_RECORDER_STREAM_INTERFACE_COUNT +      \
     USB_AUDIO_PLAYER_STREAM_INTERFACE_COUNT)
#define USB_AUDIO_RECORDER_CONTROL_INTERFACE_COUNT (1)
#define USB_AUDIO_RECORDER_STREAM_INTERFACE_COUNT  (1)
#define USB_AUDIO_PLAYER_STREAM_INTERFACE_COUNT    (1)

#define AUDIO_IN_SAMPLING_RATE_KHZ  (44100U)
#define AUDIO_OUT_SAMPLING_RATE_KHZ (44100U)

/*reference macro for audio sample macro */
#define AUDIO_IN_SAMPLING_RATE (AUDIO_IN_SAMPLING_RATE_KHZ * 1000)
/*if audio in and audio out sample rate are different, please use AUDIO_OUT_SAMPLING_RATE and
AUDIO_IN_SAMPLING_RATE to initialize out and in sample rate respectively*/
#define AUDIO_SAMPLING_RATE (AUDIO_IN_SAMPLING_RATE)
#define TSAMFREQ2BYTES(f)   (f & 0xFFU), ((f >> 8U) & 0xFFU), ((f >> 16U) & 0xFFU)

/* Audio data format */
#define AUDIO_FORMAT_PCM   0x1
#define AUDIO_FORMAT_FLOAT (0x03)

#define AUDIO_IN_FORMAT_CHANNELS (28U)
#define AUDIO_IN_FORMAT_BITS     (16)
#define AUDIO_IN_FORMAT_SIZE     (0x02)
#define AUDIO_IN_FORMAT          AUDIO_FORMAT_PCM

#define AUDIO_OUT_FORMAT_CHANNELS (2U)
#define AUDIO_OUT_FORMAT_BITS     (16)
#define AUDIO_OUT_FORMAT_SIZE     (0x02)
#define AUDIO_OUT_FORMAT          AUDIO_FORMAT_PCM

#define AUDIO_FORMAT_BITS     (AUDIO_IN_FORMAT_BITS)
#define AUDIO_FORMAT_CHANNELS (AUDIO_IN_FORMAT_CHANNELS)

#define HS_ISO_IN_ENDP_INTERVAL           (0x01)
#define HS_ISO_OUT_ENDP_INTERVAL          (0x01)
#define FS_ISO_OUT_ENDP_INTERVAL          (0x01) /* one frame, 1ms*/
#define FS_ISO_IN_ENDP_INTERVAL           (0x01)
#define FS_ISO_FEEDBACK_ENDP_INTERVAL     (0x01)
#define HS_AUDIO_INTERRUPT_IN_PACKET_SIZE (8)
#define FS_AUDIO_INTERRUPT_IN_PACKET_SIZE (8)
#define HS_AUDIO_INTERRUPT_IN_INTERVAL    (0x07U) /* 2^(7-1) = 8ms */
#define FS_AUDIO_INTERRUPT_IN_INTERVAL    (0x08U)
#define HS_ISO_IN_ENDP_PACKET_SIZE        1024 /*(1024 | 0x02 << 11)*/ /* 3072 */
#define HS_ISO_OUT_ENDP_PACKET_SIZE       1024
/*
#define HS_ISO_IN_ENDP_PACKET_SIZE                                                                 \
    ((AUDIO_IN_SAMPLING_RATE_KHZ / 1000) * AUDIO_IN_FORMAT_CHANNELS * AUDIO_IN_FORMAT_SIZE)
*/
#if defined(USB_DEVICE_AUDIO_USE_SYNC_MODE) && (USB_DEVICE_AUDIO_USE_SYNC_MODE > 0U)
#else
#if (USB_DEVICE_CONFIG_AUDIO_CLASS_2_0)
#define HS_ISO_FEEDBACK_ENDP_PACKET_SIZE (4)
#if defined(USB_DEVICE_WORKAROUND_AUDIO_20_WINDOWS) && (USB_DEVICE_WORKAROUND_AUDIO_20_WINDOWS > 0)
#define FS_ISO_FEEDBACK_ENDP_PACKET_SIZE (4)
#else
#define FS_ISO_FEEDBACK_ENDP_PACKET_SIZE (3)
#endif
#else
#define HS_ISO_FEEDBACK_ENDP_PACKET_SIZE (3)
#define FS_ISO_FEEDBACK_ENDP_PACKET_SIZE (3)
#endif
#endif

#define USB_DESCRIPTOR_LENGTH_AUDIO                                                                \
    (USB_IAD_DESC_SIZE + USB_DESCRIPTOR_LENGTH_INTERFACE +                                         \
     USB_AUDIO_CONTROL_INTERFACE_HEADER_LENGTH + USB_AUDIO_CLOCK_SOURCE_LENGTH +                   \
     USB_AUDIO_INPUT_TERMINAL_ONLY_DESC_SIZE + USB_AUDIO_OUTPUT_TERMINAL_ONLY_DESC_SIZE +          \
     USB_AUDIO_INPUT_TERMINAL_ONLY_DESC_SIZE + USB_AUDIO_OUTPUT_TERMINAL_ONLY_DESC_SIZE +          \
     USB_DESCRIPTOR_LENGTH_INTERFACE + USB_DESCRIPTOR_LENGTH_INTERFACE +                           \
     USB_AUDIO_STREAMING_IFACE_DESC_SIZE + USB_AUDIO_STREAMING_TYPE_I_DESC_SIZE +                  \
     USB_ENDPOINT_AUDIO_DESCRIPTOR_LENGTH + USB_AUDIO_STREAMING_ENDP_DESC_SIZE +                   \
     USB_DESCRIPTOR_LENGTH_INTERFACE + USB_DESCRIPTOR_LENGTH_INTERFACE +                           \
     USB_AUDIO_STREAMING_IFACE_DESC_SIZE + USB_AUDIO_STREAMING_TYPE_I_DESC_SIZE +                  \
     USB_ENDPOINT_AUDIO_DESCRIPTOR_LENGTH + USB_AUDIO_STREAMING_ENDP_DESC_SIZE +                   \
     USB_AUDIO_STANDARD_AS_ISO_FEEDBACK_ENDPOINT_LENGTH)

/* Class code. */
#define USB_DEVICE_CLASS    (0xEF)
#define USB_DEVICE_SUBCLASS (0x02)
#define USB_DEVICE_PROTOCOL (0x01)

#define USB_AUDIO_CLASS           (0x01)
#define USB_SUBCLASS_AUDIOCONTROL (0x01)
#define USB_SUBCLASS_AUDIOSTREAM  (0x02)
#if (USB_DEVICE_CONFIG_AUDIO_CLASS_2_0)
#define USB_AUDIO_PROTOCOL (0x20)
#else
#define USB_AUDIO_PROTOCOL (0x00)
#endif

#define USB_AUDIO_STREAM_ENDPOINT_DESCRIPTOR    (0x25)
#define USB_AUDIO_EP_GENERAL_DESCRIPTOR_SUBTYPE (0x01)

#if (USB_DEVICE_CONFIG_AUDIO_CLASS_2_0)
#define USB_AUDIO_CONTROL_CLOCK_SOURCE_ENTITY_ID (0x10)
#endif

#define USB_AUDIO_RECORDER_CONTROL_INPUT_TERMINAL_ID  (0x01)
#define USB_AUDIO_RECORDER_CONTROL_OUTPUT_TERMINAL_ID (0x02)

#define USB_AUDIO_PLAYER_CONTROL_INPUT_TERMINAL_ID  (0x03)
#define USB_AUDIO_PLAYER_CONTROL_OUTPUT_TERMINAL_ID (0x04)

#define USB_DEVICE_MAX_POWER (0x32)

extern usb_device_interfaces_struct_t g_mscDiskInterfaces[USB_MSC_DISK_INTERFACE_COUNT];
extern usb_device_interfaces_struct_t
  g_UsbDeviceAudioRecorderInterfaces[USB_AUDIO_RECORDER_INTERFACE_COUNT];
extern usb_device_interfaces_struct_t
  g_UsbDeviceAudioPlayerInterfaces[USB_AUDIO_PLAYER_INTERFACE_COUNT];
extern usb_device_interfaces_struct_t g_cdcVcomInterfaces[USB_CDC_VCOM_INTERFACE_COUNT];
extern usb_device_interfaces_struct_t g_MidiInterfaces[USB_MIDI_INTERFACE_COUNT];
/*******************************************************************************
 * API
 ******************************************************************************/
/*!
 * @brief USB device set speed function.
 *
 * This function sets the speed of the USB device.
 *
 * Due to the difference of HS and FS descriptors, the device descriptors and configurations need to
 * be updated to match current speed. As the default, the device descriptors and configurations are
 * configured by using FS parameters for both EHCI and KHCI. When the EHCI is enabled, the
 * application needs to call this function to update device by using current speed. The updated
 * information includes endpoint max packet size, endpoint interval, etc.
 *
 * @param handle The USB device handle.
 * @param speed Speed type. USB_SPEED_HIGH/USB_SPEED_FULL/USB_SPEED_LOW.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
extern usb_status_t USB_DeviceSetSpeed(usb_device_handle handle, uint8_t speed);
#if (defined(USB_DEVICE_CONFIG_CV_TEST) && (USB_DEVICE_CONFIG_CV_TEST > 0U))
/* Get device qualifier descriptor request */
usb_status_t USB_DeviceGetDeviceQualifierDescriptor(
  usb_device_handle handle,
  usb_device_get_device_qualifier_descriptor_struct_t* deviceQualifierDescriptor);
#endif
/*!
 * @brief USB device get device descriptor function.
 *
 * This function gets the device descriptor of the USB device.
 *
 * @param handle The USB device handle.
 * @param deviceDescriptor The pointer to the device descriptor structure.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t USB_DeviceGetDeviceDescriptor(
  usb_device_handle handle,
  usb_device_get_device_descriptor_struct_t* deviceDescriptor);

/*!
 * @brief USB device get configuration descriptor function.
 *
 * This function gets the configuration descriptor of the USB device.
 *
 * @param handle The USB device handle.
 * @param configurationDescriptor The pointer to the configuration descriptor structure.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t USB_DeviceGetConfigurationDescriptor(
  usb_device_handle handle,
  usb_device_get_configuration_descriptor_struct_t* configurationDescriptor);

/*!
 * @brief USB device get string descriptor function.
 *
 * This function gets the string descriptor of the USB device.
 *
 * @param handle The USB device handle.
 * @param stringDescriptor Pointer to the string descriptor structure.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
usb_status_t USB_DeviceGetStringDescriptor(
  usb_device_handle handle,
  usb_device_get_string_descriptor_struct_t* stringDescriptor);

#endif /* _USB_DEVICE_DESCRIPTOR_H_ */
