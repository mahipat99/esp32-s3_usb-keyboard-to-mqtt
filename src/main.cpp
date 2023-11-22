#include <elapsedMillis.h>
#include <usb/usb_host.h>
#include "show_desc.hpp"
#include "usbhhelp.hpp"
#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE
#include "esp_log.h"
#include <WiFi.h>
extern "C"
{
#include "freertos/FreeRTOS.h"
#include "freertos/timers.h"
}
#include <AsyncMqttClient.h>

#define WIFI_SSID "*************************"
#define WIFI_PASSWORD "*************************"

#define MQTT_USERNAME "*************************"
#define MQTT_PASSWORD "*************************"

#define MQTT_HOST IPAddress(0, 0, 0, 0)
#define MQTT_PORT 1883

#define MAX_SIM_KEYS 6

#define EVENT_MESSAGE "{\"evt\":\"%s\",\"key\":%02d}"
#define EVENT_TOPIC "keypads/lr-numpad01/action"

bool isKeyboard = false;
bool isKeyboardReady = false;
uint8_t KeyboardInterval;
bool isKeyboardPolling = false;
elapsedMillis KeyboardTimer;

const size_t KEYBOARD_IN_BUFFER_SIZE = 8;
usb_transfer_t *KeyboardIn = NULL;

AsyncMqttClient mqttClient;
TimerHandle_t mqttReconnectTimer;
TimerHandle_t wifiReconnectTimer;

byte pressedKeys[MAX_SIM_KEYS];
byte lastPressedKeys[MAX_SIM_KEYS];

char sendBuffer[100];

void connectToWifi()
{
  ESP_LOGI("", "Connecting to Wi-Fi...");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void connectToMqtt()
{
  ESP_LOGI("", "Connecting to MQTT...");
  mqttClient.connect();
}

void WiFiEvent(WiFiEvent_t event)
{
  ESP_LOGI("", "[WiFi-event] event: %d\n", event);
  switch (event)
  {
  case SYSTEM_EVENT_STA_GOT_IP:
    ESP_LOGI("", "WiFi connected");
    ESP_LOGI("", "IP address: ");
    ESP_LOGI("", "%s", WiFi.localIP().toString());
    connectToMqtt();
    break;
  case SYSTEM_EVENT_STA_DISCONNECTED:
    ESP_LOGI("", "WiFi lost connection");
    xTimerStop(mqttReconnectTimer, 0); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
    xTimerStart(wifiReconnectTimer, 0);
    break;
  }
}

void onMqttConnect(bool sessionPresent)
{
  ESP_LOGI("", "Connected to MQTT.");
  ESP_LOGI("", "Session present: %d", sessionPresent);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason)
{
  ESP_LOGI("", "Disconnected from MQTT.");

  if (WiFi.isConnected())
  {
    xTimerStart(mqttReconnectTimer, 0);
  }
}

void keyboard_transfer_cb(usb_transfer_t *transfer)
{
  if (Device_Handle == transfer->device_handle)
  {
    isKeyboardPolling = false;
    if (transfer->status == 0)
    {
      if (transfer->actual_num_bytes == 8)
      {
        uint8_t *const p = transfer->data_buffer;
        ESP_LOGI("", "HID report: %02x %02x %02x %02x %02x %02x %02x %02x",
                 p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7]);
        bool found = false;

        for (size_t s = 0; s < MAX_SIM_KEYS; s++)
          pressedKeys[s] = 0;

        size_t fi = 0;
        for (size_t s = 0; s < MAX_SIM_KEYS; s++)
        {
          if (p[s + 2] == 0)
            continue;
          found = false;
          for (size_t c = 0; c < MAX_SIM_KEYS; c++)
            found |= (lastPressedKeys[c] == p[s + 2]);
          pressedKeys[fi++] = p[s + 2];
          if (!found)
          {
            ESP_LOGI("", "KEY_DOWN: %02x", p[s + 2]);
            sprintf(sendBuffer, EVENT_MESSAGE, "keydown", p[s + 2]);
            mqttClient.publish(EVENT_TOPIC, 1, false, sendBuffer, strlen(sendBuffer));
          }
        }

        for (size_t s = 0; s < MAX_SIM_KEYS; s++)
        {
          found = false;
          for (size_t c = 0; c < MAX_SIM_KEYS; c++)
            found |= (lastPressedKeys[s] == p[c + 2]);
          if (!found)
          {
            ESP_LOGI("", "KEY_UP: %02x", lastPressedKeys[s]);
            sprintf(sendBuffer, EVENT_MESSAGE, "keyup", lastPressedKeys[s]);
            mqttClient.publish(EVENT_TOPIC, 1, false, sendBuffer, strlen(sendBuffer));
          }
        }

        for (size_t s = 0; s < MAX_SIM_KEYS; s++)
          lastPressedKeys[s] = pressedKeys[s];
      }
      else
      {
        ESP_LOGI("", "Keyboard boot hid transfer too short or long");
      }
    }
    else
    {
      ESP_LOGI("", "transfer->status %d", transfer->status);
    }
  }
}

void check_interface_desc_boot_keyboard(const void *p)
{
  const usb_intf_desc_t *intf = (const usb_intf_desc_t *)p;

  if ((intf->bInterfaceClass == USB_CLASS_HID) &&
      (intf->bInterfaceSubClass == 1) &&
      (intf->bInterfaceProtocol == 1))
  {
    isKeyboard = true;
    ESP_LOGI("", "Claiming a boot keyboard!");
    esp_err_t err = usb_host_interface_claim(Client_Handle, Device_Handle,
                                             intf->bInterfaceNumber, intf->bAlternateSetting);
    if (err != ESP_OK)
      ESP_LOGI("", "usb_host_interface_claim failed: %x", err);
  }
}

void prepare_endpoint(const void *p)
{
  const usb_ep_desc_t *endpoint = (const usb_ep_desc_t *)p;
  esp_err_t err;

  // must be interrupt for HID
  if ((endpoint->bmAttributes & USB_BM_ATTRIBUTES_XFERTYPE_MASK) != USB_BM_ATTRIBUTES_XFER_INT)
  {
    ESP_LOGI("", "Not interrupt endpoint: 0x%02x", endpoint->bmAttributes);
    return;
  }
  if (endpoint->bEndpointAddress & USB_B_ENDPOINT_ADDRESS_EP_DIR_MASK)
  {
    err = usb_host_transfer_alloc(KEYBOARD_IN_BUFFER_SIZE, 0, &KeyboardIn);
    if (err != ESP_OK)
    {
      KeyboardIn = NULL;
      ESP_LOGI("", "usb_host_transfer_alloc In fail: %x", err);
      return;
    }
    KeyboardIn->device_handle = Device_Handle;
    KeyboardIn->bEndpointAddress = endpoint->bEndpointAddress;
    KeyboardIn->callback = keyboard_transfer_cb;
    KeyboardIn->context = NULL;
    isKeyboardReady = true;
    KeyboardInterval = endpoint->bInterval;
    ESP_LOGI("", "USB boot keyboard ready");
  }
  else
  {
    ESP_LOGI("", "Ignoring interrupt Out endpoint");
  }
}

void show_config_desc_full(const usb_config_desc_t *config_desc)
{
  // Full decode of config desc.
  const uint8_t *p = &config_desc->val[0];
  static uint8_t USB_Class = 0;
  uint8_t bLength;
  for (int i = 0; i < config_desc->wTotalLength; i += bLength, p += bLength)
  {
    bLength = *p;
    if ((i + bLength) <= config_desc->wTotalLength)
    {
      const uint8_t bDescriptorType = *(p + 1);
      switch (bDescriptorType)
      {
      case USB_B_DESCRIPTOR_TYPE_DEVICE:
        ESP_LOGI("", "USB Device Descriptor should not appear in config");
        break;
      case USB_B_DESCRIPTOR_TYPE_CONFIGURATION:
        show_config_desc(p);
        break;
      case USB_B_DESCRIPTOR_TYPE_STRING:
        ESP_LOGI("", "USB string desc TBD");
        break;
      case USB_B_DESCRIPTOR_TYPE_INTERFACE:
        USB_Class = show_interface_desc(p);
        check_interface_desc_boot_keyboard(p);
        break;
      case USB_B_DESCRIPTOR_TYPE_ENDPOINT:
        show_endpoint_desc(p);
        if (isKeyboard && KeyboardIn == NULL)
          prepare_endpoint(p);
        break;
      case USB_B_DESCRIPTOR_TYPE_DEVICE_QUALIFIER:
        // Should not be config config?
        ESP_LOGI("", "USB device qual desc TBD");
        break;
      case USB_B_DESCRIPTOR_TYPE_OTHER_SPEED_CONFIGURATION:
        // Should not be config config?
        ESP_LOGI("", "USB Other Speed TBD");
        break;
      case USB_B_DESCRIPTOR_TYPE_INTERFACE_POWER:
        // Should not be config config?
        ESP_LOGI("", "USB Interface Power TBD");
        break;
      case 0x21:
        if (USB_Class == USB_CLASS_HID)
        {
          show_hid_desc(p);
        }
        break;
      default:
        ESP_LOGI("", "Unknown USB Descriptor Type: 0x%x", bDescriptorType);
        break;
      }
    }
    else
    {
      ESP_LOGI("", "USB Descriptor invalid");
      return;
    }
  }
}

void setup()
{
  esp_log_level_set("*", ESP_LOG_VERBOSE);
  ESP_LOGI("", "BOOT");
  usbh_setup(show_config_desc_full);

  mqttReconnectTimer = xTimerCreate("mqttTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToMqtt));
  wifiReconnectTimer = xTimerCreate("wifiTimer", pdMS_TO_TICKS(2000), pdFALSE, (void *)0, reinterpret_cast<TimerCallbackFunction_t>(connectToWifi));

  WiFi.onEvent(WiFiEvent);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);

  mqttClient.setServer(MQTT_HOST, MQTT_PORT);
  mqttClient.setCredentials(MQTT_USERNAME, MQTT_PASSWORD);

  connectToWifi();
}

void loop()
{
  usbh_task();

  if (isKeyboardReady && !isKeyboardPolling && (KeyboardTimer > KeyboardInterval))
  {
    KeyboardIn->num_bytes = 8;
    esp_err_t err = usb_host_transfer_submit(KeyboardIn);
    if (err != ESP_OK)
    {
      ESP_LOGI("", "usb_host_transfer_submit In fail: %x", err);
    }
    isKeyboardPolling = true;
    KeyboardTimer = 0;
  }
}