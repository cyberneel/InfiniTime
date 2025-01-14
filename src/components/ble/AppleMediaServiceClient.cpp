#include "components/ble/AppleMediaServiceClient.h"
#include <algorithm>
#include "components/ble/NotificationManager.h"
#include "systemtask/SystemTask.h"
#include <string.h>

using namespace Pinetime::Controllers;

int OnDiscoveryEventCallback(uint16_t conn_handle, const struct ble_gatt_error* error, const struct ble_gatt_svc* service, void* arg) {
  auto client = static_cast<AppleMediaServiceClient*>(arg);
  return client->OnDiscoveryEvent(conn_handle, error, service);
}

int OnAMSCharacteristicDiscoveredCallback(uint16_t conn_handle,
                                          const struct ble_gatt_error* error,
                                          const struct ble_gatt_chr* chr,
                                          void* arg) {
  auto client = static_cast<AppleMediaServiceClient*>(arg);
  return client->OnCharacteristicsDiscoveryEvent(conn_handle, error, chr);
}

int OnAMSDescriptorDiscoveryEventCallback(uint16_t conn_handle,
                                          const struct ble_gatt_error* error,
                                          uint16_t chr_val_handle,
                                          const struct ble_gatt_dsc* dsc,
                                          void* arg) {
  auto client = static_cast<AppleMediaServiceClient*>(arg);
  return client->OnDescriptorDiscoveryEventCallback(conn_handle, error, chr_val_handle, dsc);
}

int NewAlertSubcribeCallback(uint16_t conn_handle, const struct ble_gatt_error* error, struct ble_gatt_attr* attr, void* arg) {
  auto client = static_cast<AppleMediaServiceClient*>(arg);
  return client->OnNewAlertSubcribe(conn_handle, error, attr);
}

AppleMediaServiceClient::AppleMediaServiceClient(Pinetime::System::SystemTask& systemTask,
                                                 Pinetime::Controllers::NotificationManager& notificationManager)
  : systemTask {systemTask}, notificationManager {notificationManager} {
}

bool AppleMediaServiceClient::OnDiscoveryEvent(uint16_t connectionHandle, const ble_gatt_error* error, const ble_gatt_svc* service) {
  if (service == nullptr && error->status == BLE_HS_EDONE) {
    if (isDiscovered) {
      NRF_LOG_INFO("AMS Discovery found, starting characteristics discovery");

      ble_gattc_disc_all_chrs(connectionHandle, amsStartHandle, amsEndHandle, OnAMSCharacteristicDiscoveredCallback, this);
    } else {
      NRF_LOG_INFO("AMS not found");
      onServiceDiscovered(connectionHandle);
    }
    return true;
  }

  if (service != nullptr && ble_uuid_cmp(&amsUuid.u, &service->uuid.u) == 0) {
    NRF_LOG_INFO("AMS discovered : 0x%x - 0x%x", service->start_handle, service->end_handle);
    amsStartHandle = service->start_handle;
    amsEndHandle = service->end_handle;
    isDiscovered = true;
  }
  return false;
}

int AppleMediaServiceClient::OnCharacteristicsDiscoveryEvent(uint16_t connectionHandle,
                                                             const ble_gatt_error* error,
                                                             const ble_gatt_chr* characteristic) {
  if (error->status != 0 && error->status != BLE_HS_EDONE) {
    NRF_LOG_INFO("AMS Characteristic discovery ERROR");
    onServiceDiscovered(connectionHandle);
    return 0;
  }

  if (characteristic == nullptr && error->status == BLE_HS_EDONE) {
    NRF_LOG_INFO("AMS Characteristic discovery complete");
    if (isRemoteCommandCharacteristicDiscovered) {
      ble_gattc_disc_all_dscs(connectionHandle, remoteCommandHandle, amsEndHandle, OnAMSDescriptorDiscoveryEventCallback, this);
    }
    if (isEntityUpdateCharacteristicDiscovered) {
      ble_gattc_disc_all_dscs(connectionHandle, entityUpdateHandle, amsEndHandle, OnAMSDescriptorDiscoveryEventCallback, this);
    }
    if (isRemoteCommandCharacteristicDiscovered == isEntityUpdateCharacteristicDiscovered &&
        isRemoteCommandCharacteristicDiscovered == isEntityAttributeCharacteristicDiscovered) {
      onServiceDiscovered(connectionHandle);
    }
  } else {
    if (characteristic != nullptr) {
      if (ble_uuid_cmp(&remoteCommandChar.u, &characteristic->uuid.u) == 0) {
        NRF_LOG_INFO("AMS Characteristic discovered: Remote Command");
        DebugNotification("AMS Characteristic discovered: Remote Command");
        remoteCommandHandle = characteristic->val_handle;
        isRemoteCommandCharacteristicDiscovered = true;
      } else if (ble_uuid_cmp(&entityUpdateChar.u, &characteristic->uuid.u) == 0) {
        NRF_LOG_INFO("AMS Characteristic discovered: Entity Update");
        DebugNotification("AMS Characteristic discovered: Entity Update");
        entityUpdateHandle = characteristic->val_handle;
        isEntityUpdateCharacteristicDiscovered = true;
      } else if (ble_uuid_cmp(&entityAttributeChar.u, &characteristic->uuid.u) == 0) {
        char msg[75];
        snprintf(msg, sizeof(msg), "AMS Characteristic discovered: Entity Attribute\n%d", characteristic->val_handle);
        NRF_LOG_INFO(msg);
        DebugNotification(msg);
        entityAttributeHandle = characteristic->val_handle;
        isEntityAttributeCharacteristicDiscovered = true;
      }
    }
  }
  return 0;
}

int AppleMediaServiceClient::OnDescriptorDiscoveryEventCallback(uint16_t connectionHandle,
                                                                const ble_gatt_error* error,
                                                                uint16_t characteristicValueHandle,
                                                                const ble_gatt_dsc* descriptor) {
  if (error->status == 0) {
    if (characteristicValueHandle == remoteCommandHandle && ble_uuid_cmp(&remoteCommandChar.u, &descriptor->uuid.u)) {
      if (remoteCommandDescriptorHandle == 0) {
        NRF_LOG_INFO("AMS Descriptor discovered : %d", descriptor->handle);
        // DebugNotification("AMS Descriptor discovered");
        remoteCommandDescriptorHandle = descriptor->handle;
        isRemoteCommandDescriptorFound = true;
        uint8_t value[2] {1, 0};
        ble_gattc_write_flat(connectionHandle, remoteCommandDescriptorHandle, value, sizeof(value), NewAlertSubcribeCallback, this);
      }
    } else if (characteristicValueHandle == entityUpdateHandle && ble_uuid_cmp(&entityUpdateChar.u, &descriptor->uuid.u)) {
      if (entityUpdateDescriptorHandle == 0) {
        NRF_LOG_INFO("AMS Descriptor discovered : %d", descriptor->handle);
        // DebugNotification("AMS Descriptor discovered");
        entityUpdateDescriptorHandle = descriptor->handle;
        isEntityUpdateDescriptorFound = true;
        uint8_t value[2] {1, 0};
        ble_gattc_write_flat(connectionHandle, entityUpdateDescriptorHandle, value, sizeof(value), NewAlertSubcribeCallback, this);
        // Playback Info
        value[0] = 0;
        value[1] = 1;
        ble_gattc_write_flat(connectionHandle, entityUpdateHandle, value, sizeof(value), nullptr, this);
        // Track Info
        uint8_t value2[5];
        value2[0] = 2;
        value2[1] = 0;
        value2[2] = 1;
        value2[3] = 2;
        value2[4] = 3;
        ble_gattc_write_flat(connectionHandle, entityUpdateHandle, value2, sizeof(value2), nullptr, this);
      }
    }
  } else {
    if (error->status != BLE_HS_EDONE) {
      char errorStr[55];
      snprintf(errorStr, sizeof(errorStr), "ANCS Descriptor discovery ERROR: %d", error->status);
      NRF_LOG_INFO(errorStr);
      // DebugNotification(errorStr);
    }
    if (isRemoteCommandDescriptorFound == isEntityUpdateDescriptorFound)
      onServiceDiscovered(connectionHandle);
  }
  return 0;
}

int AppleMediaServiceClient::OnNewAlertSubcribe(uint16_t connectionHandle, const ble_gatt_error* error, ble_gatt_attr* /*attribute*/) {
  if (error->status == 0) {
    NRF_LOG_INFO("AMS New alert subscribe OK");
    // DebugNotification("AMS New alert subscribe OK");
  } else {
    NRF_LOG_INFO("AMS New alert subscribe ERROR");
    // DebugNotification("AMS New alert subscribe ERROR");
  }
  if (isRemoteCommandDescriptorFound == isEntityUpdateDescriptorFound)
    onServiceDiscovered(connectionHandle);

  return 0;
}

void AppleMediaServiceClient::OnNotification(ble_gap_event* event) {
  if (event->notify_rx.attr_handle == remoteCommandHandle || event->notify_rx.attr_handle == remoteCommandDescriptorHandle) {
    NRF_LOG_INFO("AMS Notification received");

  } else if (event->notify_rx.attr_handle == entityUpdateHandle || event->notify_rx.attr_handle == entityUpdateDescriptorHandle) {
    NRF_LOG_INFO("AMS Notification received");

    uint8_t entityID = 0;
    uint8_t attributeID = 0;
    uint8_t entityUpdateFlags = 0;

    // Check what the entity is
    os_mbuf_copydata(event->notify_rx.om, 0, 1, &entityID);

    switch (entityID) {
      case 0:
        // Player entity
        os_mbuf_copydata(event->notify_rx.om, 1, 1, &attributeID);
        if (attributeID == 1) {
          os_mbuf_copydata(event->notify_rx.om, 2, 1, &entityUpdateFlags);
          uint32_t playebackInfo;
          os_mbuf_copydata(event->notify_rx.om, 3, event->notify_rx.om->om_len - 3, &playebackInfo);
          DecodePlaybackInfo(playebackInfo);
        }
        break;

      case 2:
        // Track entity
        os_mbuf_copydata(event->notify_rx.om, 1, 1, &attributeID);
        if (attributeID == 0) {
          // Artist utf-8
          uint32_t artist;
          os_mbuf_copydata(event->notify_rx.om, 2, event->notify_rx.om->om_len - 2, &artist);
          this->artist = std::string(reinterpret_cast<char*>(&artist));
        } else if (attributeID == 1) {
          // Album utf-8
          uint32_t album;
          os_mbuf_copydata(event->notify_rx.om, 2, event->notify_rx.om->om_len - 2, &album);
          this->album = std::string(reinterpret_cast<char*>(&album));
        } else if (attributeID == 2) {
          // Title utf-8
          uint32_t title;
          os_mbuf_copydata(event->notify_rx.om, 2, event->notify_rx.om->om_len - 2, &title);
          this->track = std::string(reinterpret_cast<char*>(&title));
        } else if (attributeID == 3) {
          // Duration in seconds string
          uint32_t duration;
          os_mbuf_copydata(event->notify_rx.om, 2, event->notify_rx.om->om_len - 2, &duration);
          this->trackLength = std::stof(reinterpret_cast<char*>(duration));
        }
        break;

      default:
        break;
    }
  }
}

void AppleMediaServiceClient::DecodePlaybackInfo(uint32_t playbackInfo) {
    // Convert uint32_t to string
    char str[12]; // Enough to hold the string representation of uint32_t
    snprintf(str, sizeof(str), "%lu", playbackInfo);

    // Extract the values

    // Playback state
    std::string string = str;
    playing =  string.substr(0, string.find(",")) == "1";

    // Playback rate
    string = string.substr(string.find(",") + 1);
    playbackSpeed = std::stof(string.substr(0, string.find(",")));

    // Elapsed time
    string = string.substr(string.find(",") + 1);
    progress = std::stof(string.substr(0, string.find(",")));
}

void AppleMediaServiceClient::Command(Commands command) {
  if (isRemoteCommandCharacteristicDiscovered) {
    uint8_t value[1] {static_cast<uint8_t>(command)};
    ble_gattc_write_flat(systemTask.nimble().connHandle(), remoteCommandHandle, value, sizeof(value), nullptr, this);
  }
}

void AppleMediaServiceClient::Reset() {
  amsStartHandle = {0};
  amsEndHandle = {0};
  remoteCommandHandle = {0};
  entityUpdateHandle = {0};
  entityAttributeHandle = {0};
  remoteCommandDescriptorHandle = {0};
  entityUpdateDescriptorHandle = {0};
  entityAttributeDescriptorHandle = {0};

  isDiscovered = {false};
  isRemoteCommandCharacteristicDiscovered = {false};
  isRemoteCommandDescriptorFound = {false};
  isEntityUpdateCharacteristicDiscovered = {false};
  isEntityUpdateDescriptorFound = {false};
  isEntityAttributeCharacteristicDiscovered = {false};
}

void AppleMediaServiceClient::Discover(uint16_t connectionHandle, std::function<void(uint16_t)> onServiceDiscovered) {
  NRF_LOG_INFO("[AMS] Starting discovery");
  // DebugNotification("[ANCS] Starting discovery");
  this->onServiceDiscovered = onServiceDiscovered;
  ble_gattc_disc_svc_by_uuid(connectionHandle, &amsUuid.u, OnDiscoveryEventCallback, this);
}

void AppleMediaServiceClient::DebugNotification(const char* msg) const {
  NRF_LOG_INFO("[AMS DEBUG] %s", msg);

  NotificationManager::Notification notif;
  std::strncpy(notif.message.data(), msg, notif.message.size() - 1);
  notif.message[notif.message.size() - 1] = '\0'; // Ensure null-termination
  notif.size = std::min(std::strlen(msg), notif.message.size());
  notif.category = Pinetime::Controllers::NotificationManager::Categories::SimpleAlert;
  notificationManager.Push(std::move(notif));

  systemTask.PushMessage(Pinetime::System::Messages::OnNewNotification);
}
