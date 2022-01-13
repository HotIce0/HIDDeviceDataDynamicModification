#define	USB_PID_NULL	0x00			/* 保留PID, 未定义 */
#define	USB_PID_SOF		0x05
#define	USB_PID_SETUP	0x0D
#define	USB_PID_IN		0x09
#define	USB_PID_OUT		0x01
#define	USB_PID_ACK		0x02
#define	USB_PID_NAK		0x0A
#define	USB_PID_STALL	0x0E
#define	USB_PID_DATA0	0x03
#define	USB_PID_DATA1	0x0B
#define	USB_PID_PRE		0x0C

/**
 * Device and/or Interface Class codes */
enum USBClassCode {
	USB_CLASS_PER_INTERFACE = 0x00,

	/** Audio class */
	USB_CLASS_AUDIO = 0x01,

	/** Communications class */
	USB_CLASS_COMM = 0x02,

	/** Human Interface Device class */
	USB_CLASS_HID = 0x03,

	/** Physical */
	USB_CLASS_PHYSICAL = 0x05,

	/** Image class */
	USB_CLASS_IMAGE = 0x06,
	USB_CLASS_PTP = 0x06, /* legacy name from libusb-0.1 usb.h */

	/** Printer class */
	USB_CLASS_PRINTER = 0x07,

	/** Mass storage class */
	USB_CLASS_MASS_STORAGE = 0x08,

	/** Hub class */
	USB_CLASS_HUB = 0x09,

	/** Data class */
	USB_CLASS_DATA = 0x0a,

	/** Smart Card */
	USB_CLASS_SMART_CARD = 0x0b,

	/** Content Security */
	USB_CLASS_CONTENT_SECURITY = 0x0d,

	/** Video */
	USB_CLASS_VIDEO = 0x0e,

	/** Personal Healthcare */
	USB_CLASS_PERSONAL_HEALTHCARE = 0x0f,

	/** Diagnostic Device */
	USB_CLASS_DIAGNOSTIC_DEVICE = 0xdc,

	/** Wireless class */
	USB_CLASS_WIRELESS = 0xe0,

	/** Miscellaneous class */
	USB_CLASS_MISCELLANEOUS = 0xef,

	/** Application class */
	USB_CLASS_APPLICATION = 0xfe,

	/** Class is vendor-specific */
	USB_CLASS_VENDOR_SPEC = 0xff
};

/**
 * Descriptor types as defined by the USB specification.
 */
enum DescriptorType {
	/** Device descriptor. See libusb_device_descriptor. */
	USB_DT_DEVICE = 0x01,

	/** Configuration descriptor. See libusb_config_descriptor. */
	USB_DT_CONFIG = 0x02,

	/** String descriptor */
	USB_DT_STRING = 0x03,

	/** Interface descriptor. See libusb_interface_descriptor. */
	USB_DT_INTERFACE = 0x04,

	/** Endpoint descriptor. See libusb_endpoint_descriptor. */
	USB_DT_ENDPOINT = 0x05,

	/** BOS descriptor */
	USB_DT_BOS = 0x0f,

	/** Device Capability descriptor */
	USB_DT_DEVICE_CAPABILITY = 0x10,

	/** HID descriptor */
	USB_DT_HID = 0x21,

	/** HID report descriptor */
	USB_DT_REPORT = 0x22,

	/** Physical descriptor */
	USB_DT_PHYSICAL = 0x23,

	/** Hub descriptor */
	USB_DT_HUB = 0x29,

	/** SuperSpeed Hub descriptor */
	USB_DT_SUPERSPEED_HUB = 0x2a,

	/** SuperSpeed Endpoint Companion descriptor */
	USB_DT_SS_ENDPOINT_COMPANION = 0x30
};


enum USBEndpointTransferType {
	/** Control endpoint */
	USB_ENDPOINT_TRANSFER_TYPE_CONTROL = 0x0,

	/** Isochronous endpoint */
	USB_ENDPOINT_TRANSFER_TYPE_ISOCHRONOUS = 0x1,

	/** Bulk endpoint */
	USB_ENDPOINT_TRANSFER_TYPE_BULK = 0x2,

	/** Interrupt endpoint */
	USB_ENDPOINT_TRANSFER_TYPE_INTERRUPT = 0x3
};

/**
 * Standard requests, as defined in table 9-5 of the USB 3.0 specifications */
enum USBStandardRequest {
	/** Request status of the specific recipient */
	USB_REQUEST_GET_STATUS = 0x00,

	/** Clear or disable a specific feature */
	USB_REQUEST_CLEAR_FEATURE = 0x01,

	/* 0x02 is reserved */

	/** Set or enable a specific feature */
	USB_REQUEST_SET_FEATURE = 0x03,

	/* 0x04 is reserved */

	/** Set device address for all future accesses */
	USB_REQUEST_SET_ADDRESS = 0x05,

	/** Get the specified descriptor */
	USB_REQUEST_GET_DESCRIPTOR = 0x06,

	/** Used to update existing descriptors or add new descriptors */
	USB_REQUEST_SET_DESCRIPTOR = 0x07,

	/** Get the current device configuration value */
	USB_REQUEST_GET_CONFIGURATION = 0x08,

	/** Set device configuration */
	USB_REQUEST_SET_CONFIGURATION = 0x09,

	/** Return the selected alternate setting for the specified interface */
	USB_REQUEST_GET_INTERFACE = 0x0a,

	/** Select an alternate interface for the specified interface */
	USB_REQUEST_SET_INTERFACE = 0x0b,

	/** Set then report an endpoint's synchronization frame */
	USB_REQUEST_SYNCH_FRAME = 0x0c,

	/** Sets both the U1 and U2 Exit Latency */
	USB_REQUEST_SET_SEL = 0x30,

	/** Delay from the time a host transmits a packet to the time it is
	  * received by the device. */
	USB_SET_ISOCH_DELAY = 0x31
};

/**
 * Endpoint direction. Values for bit 7 of the
 */
enum USBEndpointDirection {
	/** Out: host-to-device */
	USB_ENDPOINT_OUT = 0x00,

	/** In: device-to-host */
	USB_ENDPOINT_IN = 0x80
};

/**
 * bmRequestType: D6~5
 */
enum USBRequestType {
	/** Standard */
	USB_REQUEST_TYPE_STANDARD = (0x00 << 5),
	/** Class */
	USB_REQUEST_TYPE_CLASS = (0x01 << 5),
	/** Vendor */
	USB_REQUEST_TYPE_VENDOR = (0x02 << 5),
	/** Reserved */
	USB_REQUEST_TYPE_RESERVED = (0x03 << 5)
};

/**
 * @brief bmRequestType: D4~0
 */
enum USBRequestRecipient {
	/** Device */
	USB_RECIPIENT_DEVICE = 0x00,
	/** Interface */
	USB_RECIPIENT_INTERFACE = 0x01,
	/** Endpoint */
	USB_RECIPIENT_ENDPOINT = 0x02,
	/** Other */
	USB_RECIPIENT_OTHER = 0x03
};

#define DEVICE_DESC_LEN 0x12

#define CONTROL_SETUP_SIZE sizeof(USBControlSetup)

#pragma pack (push)
#pragma pack (1)

typedef struct USBControlSetup {
    uint8_t bmRequestType;
    uint8_t bRequest;
    uint16_t wValue;
    uint16_t wIndex;
    uint16_t wLength;
} USBControlSetup;


typedef struct USBDeviceDescriptor {
	uint8_t  bLength;
	uint8_t  bDescriptorType;
	uint16_t bcdUSB;
	uint8_t  bDeviceClass;
	uint8_t  bDeviceSubClass;
	uint8_t  bDeviceProtocol;
	uint8_t  bMaxPacketSize0;
	uint16_t idVendor;
	uint16_t idProduct;
	uint16_t bcdDevice;
	uint8_t  iManufacturer;
	uint8_t  iProduct;
	uint8_t  iSerialNumber;
	uint8_t  bNumConfigurations;
} USBDeviceDescriptor;

typedef struct USBDescriptor {
    uint8_t bLength;
    uint8_t bDesriptorType;
} USBDescriptor;

typedef struct USBEndpointDescriptor {
	/** Size of this descriptor (in bytes) */
	uint8_t  bLength;

	uint8_t  bDescriptorType;

	/** 
    * Bits: 0~3 the endpoint number
    * Bits: 4~6 reserved, reset to zero
    * Bits: 7   Direction (Ignored for control endpoints)
    *           Values:(0 OUT, 1 IN)
    */
	uint8_t  bEndpointAddress;

	/** 
    * Bits: 0~1 TransferType
    * Bits: 2~3 Sync Type (Just iso transfer valied)
    * Bits: 4~5 Usage Type (Just iso transfer valied)
    */
	uint8_t  bmAttributes;

	/** Maximum packet size this endpoint is capable of sending/receiving. */
	uint16_t wMaxPacketSize;

	/** Interval for polling endpoint for data transfers. */
	uint8_t  bInterval;
} USBEndpointDescriptor;


typedef struct USBInterfaceDescriptor {
	/** Size of this descriptor (in bytes) */
	uint8_t  bLength;

    /* USB_DT_INTERFACE */
	uint8_t  bDescriptorType;

	/** Number of this interface */
	uint8_t  bInterfaceNumber;

	/** Value used to select this alternate setting for this interface */
	uint8_t  bAlternateSetting;

	/** Number of endpoints used by this interface (excluding the control
	 * endpoint). */
	uint8_t  bNumEndpoints;

	/** USB-IF class code for this interface. */
	uint8_t  bInterfaceClass;

	/** USB-IF subclass code for this interface, qualified by the
	 * bInterfaceClass value */
	uint8_t  bInterfaceSubClass;

	/** USB-IF protocol code for this interface, qualified by the
	 * bInterfaceClass and bInterfaceSubClass values */
	uint8_t  bInterfaceProtocol;

	/** Index of string descriptor describing this interface */
	uint8_t  iInterface;
} USBInterfaceDescriptor;

typedef struct USBConfigDescriptor {
	/** Size of this descriptor (in bytes) */
	uint8_t  bLength;

	/** Descriptor type. */
	uint8_t  bDescriptorType;

	/** Total length of data returned for this configuration */
	uint16_t wTotalLength;

	/** Number of interfaces supported by this configuration */
	uint8_t  bNumInterfaces;

	/** Identifier value for this configuration */
	uint8_t  bConfigurationValue;

	/** Index of string descriptor describing this configuration */
	uint8_t  iConfiguration;

	/** Configuration characteristics */
	uint8_t  bmAttributes;

	/** Maximum power consumption of the USB device from this bus in this
	 * configuration when the device is fully operation. Expressed in units
	 * of 2 mA when the device is operating in high-speed mode and in units
	 * of 8 mA when the device is operating in super-speed mode. */
	uint8_t  MaxPower;
} USBConfigDescriptor;


#pragma pack (pop)