#pragma once
#define min // workaround: nimble's min/max macros conflict with libstdc++
#define max
#include <host/ble_gap.h>
#undef max
#undef min
#include <atomic>

namespace Pinetime {
  namespace Controllers {
    class InfiniSleepController;
    class NimbleController;

    namespace InfiniSleepControllerTypes {
      struct SessionData; // Forward declaration of the nested type
    }

    class SleepDataService {
    public:
      SleepDataService(NimbleController& nimble, Controllers::InfiniSleepController& infininiSleepController);
      void Init();
      int OnSleepDataRequested(uint16_t attributeHandle, ble_gatt_access_ctxt* context);
      void OnNewSleepDataValue(InfiniSleepControllerTypes::SessionData &sessionData);

      void SubscribeNotification(uint16_t attributeHandle);
      void UnsubscribeNotification(uint16_t attributeHandle);

      static constexpr uint16_t sleepDataServiceId {0x2A80};
      static constexpr ble_uuid16_t sleepDataServiceUuid {.u {.type = BLE_UUID_TYPE_16}, .value = sleepDataServiceId};

    private:
      NimbleController& nimble;
      Controllers::InfiniSleepController& infiniSleepController;
      static constexpr uint16_t sleepDataInfoId {0x2037};

      static constexpr ble_uuid16_t sleepDataInfoUuid {.u {.type = BLE_UUID_TYPE_16}, .value = sleepDataInfoId};

      struct ble_gatt_chr_def characteristicDefinition[2];
      struct ble_gatt_svc_def serviceDefinition[2];

      uint16_t sleepDataInfoHandle;
      std::atomic_bool sleepDataInfoNotificationEnable {false};
    };
  }
}
