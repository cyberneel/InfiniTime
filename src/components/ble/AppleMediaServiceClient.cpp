#include "components/ble/AppleMediaServiceClient.h"
#include <algorithm>
#include "components/ble/NotificationManager.h"
#include "systemtask/SystemTask.h"

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

int OnControlPointWriteCallback(uint16_t conn_handle, const struct ble_gatt_error* error, struct ble_gatt_attr* attr, void* arg) {
  auto client = static_cast<AppleMediaServiceClient*>(arg);
  return client->OnControlPointWrite(conn_handle, error, attr);
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
        NRF_LOG_INFO("ANCS Descriptor discovered : %d", descriptor->handle);
        // DebugNotification("AMS Descriptor discovered");
        entityUpdateDescriptorHandle = descriptor->handle;
        isEntityUpdateDescriptorFound = true;
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
  NRF_LOG_INFO("[ANCS] Starting discovery");
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
