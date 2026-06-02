#include <solar.h>
#include "version.h"

Solar controller(GIT_VERSION);

void setup() {
  controller.setup();
}

void loop() {
  controller.loop();
}
d:\Cursor\OSPC\OSPController\OSPController\libraries\MPPTLib\solar.cpp