#ifndef CONFIG_USBCONFIG_H_
#define CONFIG_USBCONFIG_H_

#define VENDOR_ID 0x16D0
#define PRODUCT_ID 0x0E9F

#define PRODUCT_NAME_LEN 7
#define PRODUCT_NAME                                                                               \
    {                                                                                              \
        'T', 'r', 'a', 'c', 'k', 'e', 'r'                                                          \
    }

#define MANUFACTURER_NAME_LEN 7
#define MANUFACTURER_NAME                                                                          \
    {                                                                                              \
        'P', 'o', 'l', 'y', 'e', 'n', 'd'                                                          \
    }

#define EP0_SIZE 64
#define NUM_ENDPOINTS 7
#define NUM_USB_BUFFERS 30
#define NUM_INTERFACE 4

// Serial
#define CDC_IAD_DESCRIPTOR 1
#define CDC_STATUS_INTERFACE 0
#define CDC_DATA_INTERFACE 1
#define CDC_ACM_ENDPOINT 1
#define CDC_RX_ENDPOINT 2
#define CDC_TX_ENDPOINT 3
#define CDC_ACM_SIZE 16
#define CDC_RX_SIZE 64
#define CDC_TX_SIZE 64
#define ENDPOINT1_CONFIG ENDPOINT_TRANSMIT_ONLY
#define ENDPOINT2_CONFIG ENDPOINT_RECEIVE_ONLY
#define ENDPOINT3_CONFIG ENDPOINT_TRANSMIT_ONLY

// MIDI
#define MIDI_INTERFACE 2
#define MIDI_NUM_CABLES 1
#define MIDI_TX_ENDPOINT 4
#define MIDI_TX_SIZE 64
#define MIDI_RX_ENDPOINT 5
#define MIDI_RX_SIZE 64
#define ENDPOINT4_CONFIG ENDPOINT_TRANSMIT_ONLY
#define ENDPOINT5_CONFIG ENDPOINT_RECEIVE_ONLY

// RawHID
#define RAWHID_INTERFACE 3
#define RAWHID_TX_ENDPOINT 6
#define RAWHID_TX_SIZE 64
#define RAWHID_TX_INTERVAL 1
#define RAWHID_RX_ENDPOINT 7
#define RAWHID_RX_SIZE 64
#define RAWHID_RX_INTERVAL 1
#define RAWHID_USAGE_PAGE 0xFFAB // recommended: 0xFF00 to 0xFFFF
#define RAWHID_USAGE 0x0200      // recommended: 0x0100 to 0xFFFF
#define ENDPOINT6_CONFIG ENDPOINT_TRANSMIT_ONLY
#define ENDPOINT7_CONFIG ENDPOINT_RECEIVE_ONLY

#endif /* CONFIG_USBCONFIG_H_ */
