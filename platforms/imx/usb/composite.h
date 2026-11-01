/*
 * Copyright (c) 2015, Freescale Semiconductor, Inc.
 * Copyright 2016 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef _USB_DEVICE_COMPOSITE_H_
#define _USB_DEVICE_COMPOSITE_H_ 1

#include "audio_unified.h"
#include "disk.h"
#include "usb_device_descriptor.h"
#include "usb_midi.h"
#include "virtual_com.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/* @TEST_ANCHOR */

#if defined(USB_DEVICE_CONFIG_EHCI) && (USB_DEVICE_CONFIG_EHCI > 0)
#ifndef CONTROLLER_ID
#define CONTROLLER_ID kUSB_ControllerEhci0
#endif
#endif
#if defined(USB_DEVICE_CONFIG_KHCI) && (USB_DEVICE_CONFIG_KHCI > 0)
#ifndef CONTROLLER_ID
#define CONTROLLER_ID kUSB_ControllerKhci0
#endif
#endif
#if defined(USB_DEVICE_CONFIG_LPCIP3511FS) && (USB_DEVICE_CONFIG_LPCIP3511FS > 0U)
#ifndef CONTROLLER_ID
#define CONTROLLER_ID kUSB_ControllerLpcIp3511Fs0
#endif
#endif
#if defined(USB_DEVICE_CONFIG_LPCIP3511HS) && (USB_DEVICE_CONFIG_LPCIP3511HS > 0U)
#ifndef CONTROLLER_ID
#define CONTROLLER_ID kUSB_ControllerLpcIp3511Hs0
#endif
#endif

typedef struct _usb_device_composite_struct
{
    usb_device_handle deviceHandle; /* USB device handle. */
    usb_cdc_vcom_struct_t cdcVcom;  /* CDC virtual com device structure. */
    usb_midi_struct_t midi;         /* midi device structure. */
    usb_msc_struct_t mscDisk;       /* MSC device structure */
    usb_audio_composite_struct_t audioUnified;
    uint8_t speed;  /* Speed of USB device. USB_SPEED_FULL/USB_SPEED_LOW/USB_SPEED_HIGH.  */
    uint8_t attach; /* A flag to indicate whether a usb device is attached. 1: attached, 0: not
                       attached */
    uint8_t currentConfiguration; /* Current configuration value. */
    uint8_t currentInterfaceAlternateSetting[USB_INTERFACE_COUNT]; /* Current alternate setting
                                                                      value for each interface. */
} usb_device_composite_struct_t;

typedef struct _usb_composite_state
{
    bool isMidiEnabled;
    bool isAudioInEnabled;
    bool isAudioOutEnabled;
    bool isStorageEnabled;
    bool isSerialEnabled;
    bool isResetRequested;
} usb_composite_state_t;

/*******************************************************************************
 * API
 ******************************************************************************/
/*!
 * @brief CDC class specific callback function.
 *
 * This function handles the CDC class specific requests.
 *
 * @param handle          The CDC ACM class handle.
 * @param event           The CDC ACM class event type.
 * @param param           The parameter of the class specific request.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
extern usb_status_t USB_DeviceCdcVcomCallback(class_handle_t handle, uint32_t event, void* param);
/*!
 * @brief Virtual COM device set configuration function.
 *
 * This function sets configuration for CDC class.
 *
 * @param handle The CDC ACM class handle.
 * @param configure The CDC ACM class configure index.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
extern usb_status_t USB_DeviceCdcVcomSetConfigure(class_handle_t handle, uint8_t configure);
/*!
 * @brief Virtual COM device initialization function.
 *
 * This function initializes the device with the composite device class information.
 *
 * @param deviceComposite The pointer to the composite device structure.
 *
 * @return A USB error code or kStatus_USB_Success.
 */
extern usb_status_t USB_DeviceCdcVcomInit(usb_device_composite_struct_t* deviceComposite);
/*!
 * @brief Application task function.
 *
 * This function runs the task for application.
 *
 * @return None.
 */
extern void USB_DeviceCdcVcomTask(void);

extern usb_status_t USB_DeviceMidiCallback(class_handle_t handle, uint32_t event, void* param);
extern usb_status_t USB_DeviceMidiImplInit(usb_device_composite_struct_t* deviceComposite);
extern usb_status_t USB_DeviceMidiSetConfigure(class_handle_t handle, uint8_t configure);

bool MidiUsbAvailable();
void MidiUsbTx(uint32_t midiPacket);
void MidiUsbRx(uint32_t midiPacket);

extern usb_status_t USB_DeviceMscCallback(class_handle_t handle, uint32_t event, void* param);
extern usb_status_t USB_DeviceMscDiskInit(usb_device_composite_struct_t* deviceComposite);
extern usb_status_t USB_DeviceMscDiskSetConfigure(class_handle_t handle, uint8_t configure);

extern usb_status_t USB_DeviceAudioCompositeCallback(class_handle_t handle,
                                                     uint32_t event,
                                                     void* param);

extern usb_status_t USB_DeviceAudioCompositeInit(usb_device_composite_struct_t* device_composite);
extern usb_status_t USB_DeviceAudioCompositeSetConfigure(class_handle_t handle, uint8_t configure);
extern usb_status_t USB_DeviceAudioRecorderSetInterface(class_handle_t handle,
                                                        uint8_t interface,
                                                        uint8_t alternateSetting);
extern usb_status_t USB_DeviceAudioPlayerSetInterface(class_handle_t handle,
                                                      uint8_t interface,
                                                      uint8_t alternateSetting);
extern void USB_DeviceIsrEnable(void);
extern void USB_DeviceIsrDisable(void);

#endif /* _USB_DEVICE_COMPOSITE_H_ */
