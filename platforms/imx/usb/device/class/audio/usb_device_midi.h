/*
 * Copyright (c) 2015 - 2016, Freescale Semiconductor, Inc.
 * Copyright 2016 NXP
 * All rights reserved.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#ifndef __USB_DEVICE_MIDI_H__
#define __USB_DEVICE_MIDI_H__

/*!
 * @addtogroup usb_device_midi_drv
 * @{
 */
#define USB_MIDI_CLASS (0x01)
#define USB_MIDI_SUBCLASS (0x03)

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*!
 * @name USB midi class codes
 * @{
 */
/*! @brief Definition of MIDI class event. */
typedef enum _usb_device_midi_event
{
    kUSB_DeviceMidiEventSendResponse =
      0x01, /*!< This event indicates the bulk send transfer is complete or cancelled etc. */
    kUSB_DeviceMidiEventRecvResponse, /*!< This event indicates the bulk receive transfer is
                                         complete or cancelled etc.. */
} usb_device_midi_event_t;

typedef struct _usb_device_midi_pipe
{
    osa_mutex_handle_t mutex; /*!< The mutex of the pipe. */
    uint32_t mutexBuffer[(OSA_MUTEX_HANDLE_SIZE + 3) / 4];
    uint8_t* pipeDataBuffer; /*!< pipe data buffer backup when stall */
    uint32_t pipeDataLen;    /*!< pipe data length backup when stall  */
    uint8_t pipeStall;       /*!< pipe is stall  */
    uint8_t ep;              /*!< The endpoint number of the pipe. */
    uint8_t isBusy;          /*!< 1: The pipe is transferring packet, 0: The pipe is idle. */
} usb_device_midi_pipe_t;

typedef struct _usb_device_midi_struct
{
    usb_device_handle handle;                       /*!< The handle of the USB device. */
    usb_device_class_config_struct_t* configStruct; /*!< The class configure structure. */
    usb_device_interface_struct_t* interfaceHandle; /*!< The CDC communication interface handle. */
    usb_device_midi_pipe_t bulkIn;  /*!< The bulk in pipe for sending packet to host. */
    usb_device_midi_pipe_t bulkOut; /*!< The bulk out pipe for receiving packet from host. */
    uint8_t configuration;          /*!< The current configuration value. */
    uint8_t interfaceNumber;        /*!< The current interface number. */
    uint8_t alternate;              /*!< The alternate setting value of the interface. */
} usb_device_midi_struct_t;

/*******************************************************************************
 * API
 ******************************************************************************/

/*!
 * @name USB Audio Class Driver
 * @{
 */
/*!
 * @brief Initializes the USB audio class.
 *
 * This function obtains a USB device handle according to the controller ID, initializes the audio
 * class with the class configuration parameters, and creates the mutex for each pipe.
 *
 * @param controllerId The ID of the controller. The value can be chosen from the
 * kUSB_ControllerKhci0, kUSB_ControllerKhci1, kUSB_ControllerEhci0, or kUSB_ControllerEhci1.
 * @param config The user configuration structure of type usb_device_class_config_struct_t. The user
 *  populates the members of this structure and passes the pointer of this structure
 *  into this function.
 * @param handle  An out parameter. The class handle of the audio class.
 * @return A USB error code or kStatus_USB_Success.
 * @retval kStatus_USB_Success The audio class is initialized successfully.
 * @retval kStatus_USB_Busy No audio device handle available for allocation.
 * @retval kStatus_USB_InvalidHandle The audio device handle allocation failure.
 * @retval kStatus_USB_InvalidParameter The USB device handle allocation failure.
 */
extern usb_status_t USB_DeviceMidiInit(uint8_t controllerId,
                                       usb_device_class_config_struct_t* config,
                                       class_handle_t* handle);

/*!
 * @brief Deinitializes the USB audio class.
 *
 * This function destroys the mutex for each pipe, deinitializes each endpoint of the audio class,
 * and frees the audio class handle.
 *
 * @param handle The class handle of the audio class.
 * @return A USB error code or kStatus_USB_Success.
 * @retval kStatus_USB_Success The audio class is deinitialized successfully.
 * @retval kStatus_USB_Error The endpoint deinitialization failure.
 * @retval kStatus_USB_InvalidHandle The audio device handle or the audio class handle is invalid.
 * @retval kStatus_USB_InvalidParameter The endpoint number of the audio class handle is invalid.
 */
extern usb_status_t USB_DeviceMidiDeinit(class_handle_t handle);

/*!
 * @brief Handles the USB audio class event.
 *
 * This function responds to various events including the common device events and the
 * class-specific events. For class-specific events, it calls the class callback defined in the
 * application to deal with the class-specific event.
 *
 * @param handle The class handle of the audio class.
 * @param event The event type.
 * @param param The class handle of the audio class.
 * @return A USB error code or kStatus_USB_Success.
 * @retval kStatus_USB_Success The audio class is deinitialized successfully.
 * @retval kStatus_USB_Error The configure structure of the audio class handle is invalid.
 * @retval kStatus_USB_InvalidHandle The audio device handle or the audio class handle is invalid.
 * @retval kStatus_USB_InvalidParameter The endpoint number of the audio class handle is invalid.
 * @retval Others The error code returned by class callback in application.
 */
extern usb_status_t USB_DeviceMidiEvent(void* handle, uint32_t event, void* param);

/*!
 * @brief Primes the endpoint to send a packet to the host.
 *
 * This function checks whether the endpoint is sending packet, then it primes the endpoint
 * with the buffer address and the buffer length if the pipe is not busy. Otherwise, it ignores this
 * transfer by returning an error code.
 *
 * @param handle The class handle of the audio class.
 * @param ep The endpoint number of the transfer.
 * @param buffer The pointer to the buffer to be transferred.
 * @param length The length of the buffer to be transferred.
 * @return A USB error code or kStatus_USB_Success.
 * @retval kStatus_USB_Success Prime to send packet successfully.
 * @retval kStatus_USB_Busy The endpoint is busy in transferring.
 * @retval kStatus_USB_InvalidHandle The audio device handle or the audio class handle is invalid.
 * @retval kStatus_USB_ControllerNotFound The controller interface is invalid.
 *
 * @note The function can only be called in the same context.
 */
extern usb_status_t USB_DeviceMidiSend(class_handle_t handle,
                                       uint8_t ep,
                                       uint8_t* buffer,
                                       uint32_t length);

/*!
 * @brief Primes the endpoint to receive a packet from the host.
 *
 * This function checks whether the endpoint is receiving packet, then it primes the endpoint
 * with the buffer address and the buffer length if the pipe is not busy. Otherwise, it ignores this
 * transfer by returning an error code.
 *
 * @param handle The class handle of the audio class.
 * @param ep The endpoint number of the transfer.
 * @param buffer The pointer to the buffer to be transferred.
 * @param length The length of the buffer to be transferred.
 * @return A USB error code or kStatus_USB_Success.
 * @retval kStatus_USB_Success Prime to receive packet successfully.
 * @retval kStatus_USB_Busy The endpoint is busy in transferring.
 * @retval kStatus_USB_InvalidHandle The audio device handle or the audio class handle is invalid.
 * @retval kStatus_USB_ControllerNotFound The controller interface is invalid.
 *
 * @note The function can only be called in the same context.
 */
extern usb_status_t USB_DeviceMidiRecv(class_handle_t handle,
                                       uint8_t ep,
                                       uint8_t* buffer,
                                       uint32_t length);

/*! @}*/

/*! @}*/

#endif /* __USB_DEVICE_MIDI_H__ */
