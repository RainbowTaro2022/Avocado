#include "mouse.h"
#include "config.h"

namespace peripherals {
Mouse::Mouse() {
    // verbose = config["debug"]["log"]["mouse"];
}

uint8_t Mouse::handle(uint8_t byte) {
    switch (state) {
        case 0:
            if (byte == 0x01) {
                state++;
                return 0xff;
            }
            return 0xff;

        case 1:
            if (byte == 0x42) {
                state++;
                return 0x12;
            }
            state = 0;
            return 0xff;

        case 2: state++; return 0x5a;

        case 3: state++; return ~buttons._byte[0];

        case 4: state++; return ~buttons._byte[1];

        case 5: state++; return x;

        case 6: state = 0; return y;

        default: state = 0; return 0xff;
    }
}
};  // namespace peripherals