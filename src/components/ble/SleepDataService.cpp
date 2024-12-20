#include "components/infinisleep/InfiniSleepController.h"
#include "components/ble/SleepDataService.h"
#include "components/ble/NimbleController.h"
#include <nrf_log.h>
#include <ctime>

using namespace Pinetime::Controllers;

constexpr ble_uuid16_t SleepDataService::sleepDataServiceUuid;
constexpr ble_uuid16_t SleepDataService::sleepDataInfoUuid;

namespace {
  int SleepDataServiceCallback(uint16_t /*conn_handle*/, uint16_t attr_handle, struct ble_gatt_access_ctxt* ctxt, void* arg) {
    auto* sleepDataService = static_cast<SleepDataService*>(arg);
    return sleepDataService->OnSleepDataRequested(attr_handle, ctxt);
  }
}

// TODO Refactoring - remove dependency to SystemTask
SleepDataService::SleepDataService(NimbleController& nimble,
                                   InfiniSleepController& infiniSleepController,
                                   Controllers::DateTime& dateTimeController)
  : nimble {nimble},
    infiniSleepController {infiniSleepController},
    dateTimeController {dateTimeController},
    characteristicDefinition {{.uuid = &sleepDataInfoUuid.u,
                               .access_cb = SleepDataServiceCallback,
                               .arg = this,
                               .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_NOTIFY,
                               .val_handle = &sleepDataInfoHandle},
                              {0}},
    serviceDefinition {
      {/* Device Information Service */
       .type = BLE_GATT_SVC_TYPE_PRIMARY,
       .uuid = &sleepDataServiceUuid.u,
       .characteristics = characteristicDefinition},
      {0},
    } {
  // TODO refactor to prevent this loop dependency (service depends on controller and controller depends on service)
  infiniSleepController.SetService(this);
}

void SleepDataService::Init() {
  int res = 0;
  res = ble_gatts_count_cfg(serviceDefinition);
  ASSERT(res == 0);

  res = ble_gatts_add_svcs(serviceDefinition);
  ASSERT(res == 0);
}

int SleepDataService::OnSleepDataRequested(uint16_t attributeHandle, ble_gatt_access_ctxt* context) {
  if (attributeHandle == sleepDataInfoHandle) {
    NRF_LOG_INFO("Sleep Data : handle = %d", sleepDataInfoHandle);

    uint32_t timestamp = infiniSleepController.prevSessionData.startTimeStamp;
    uint16_t minutesAsleep = infiniSleepController.prevSessionData.totalSleepMinutes;

    // [0...3] = timestamp, [4...5] = total minutes
    uint8_t buffer[6];

    buffer[0] = (timestamp >> 24) & 0xFF;
    buffer[1] = (timestamp >> 16) & 0xFF;
    buffer[2] = (timestamp >> 8) & 0xFF;
    buffer[3] = timestamp & 0xFF;

    buffer[4] = (minutesAsleep >> 8) & 0xFF;
    buffer[5] = minutesAsleep & 0xFF;

    int res = os_mbuf_append(context->om, buffer, 6);
    return (res == 0) ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
  }
  return 0;
}

void SleepDataService::OnNewSleepDataValue(InfiniSleepControllerTypes::SessionData& sessionData) {
  if (!sleepDataInfoNotificationEnable)
    return;

  uint32_t timestamp = sessionData.startTimeStamp;
  uint16_t minutesAsleep = sessionData.totalSleepMinutes;

  // [0...3] = timestamp, [4...5] = total minutes
  uint8_t buffer[6];

  buffer[0] = (timestamp >> 24) & 0xFF;
  buffer[1] = (timestamp >> 16) & 0xFF;
  buffer[2] = (timestamp >> 8) & 0xFF;
  buffer[3] = timestamp & 0xFF;

  buffer[4] = (minutesAsleep >> 8) & 0xFF;
  buffer[5] = minutesAsleep & 0xFF;

  auto* om = ble_hs_mbuf_from_flat(buffer, 6);

  uint16_t connectionHandle = nimble.connHandle();

  if (connectionHandle == 0 || connectionHandle == BLE_HS_CONN_HANDLE_NONE) {
    return;
  }

  ble_gattc_notify_custom(connectionHandle, sleepDataInfoHandle, om);
}

void SleepDataService::SubscribeNotification(uint16_t attributeHandle) {
  if (attributeHandle == sleepDataInfoHandle)
    sleepDataInfoNotificationEnable = true;
}

void SleepDataService::UnsubscribeNotification(uint16_t attributeHandle) {
  if (attributeHandle == sleepDataInfoHandle)
    sleepDataInfoNotificationEnable = false;
}