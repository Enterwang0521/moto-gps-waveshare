#include "device_ble_capabilities.hpp"
#if defined(MOTO_BOARD_SMARTRING_PLUS)
static_assert(moto::esp32::device_capabilities() == 0x2FFU,
              "SmartRing must advertise TF presets without the disabled shortcuts page");
#else
static_assert(moto::esp32::device_capabilities() == 0x1FFU,
              "Legacy firmware retains its existing capability contract");
#endif
int main() { return 0; }
