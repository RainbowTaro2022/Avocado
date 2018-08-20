/**
 * Controller - handle all communication between peripherials such
 * as digital controller, analog controller, mouse and memory card.
 */
#pragma once
#include <array>
#include <string>
#include "device/device.h"
#include "peripherals/digital_controller.h"
#include "peripherals/analog_controller.h"
#include "peripherals/mouse.h"
#include "peripherals/memory_card.h"

struct System;

namespace device {
namespace controller {

enum class DeviceSelected { None, Controller, MemoryCard };

class Controller {
    System* sys;

    DeviceSelected deviceSelected;
    peripherals::Mouse controller;

    Reg16 mode;
    Reg16 control;
    Reg16 baud;
    bool irq = false;
    int irqTimer = 0;

    void handleByte(uint8_t byte);

    uint8_t rxData = 0;
    bool rxPending = false;
    bool ack = false;

    uint8_t getData() {
        uint8_t ret = rxData;
        rxData = 0xff;
        rxPending = false;

        return ret;
    }

   public:
    peripherals::MemoryCard card;
    
    Controller(System* sys);
    void step();
    uint8_t read(uint32_t address);
    void write(uint32_t address, uint8_t data);
    void setState(peripherals::DigitalController::ButtonState buttons) { /*this->controller.buttons = buttons;*/ }
};
}  // namespace controller
}  // namespace device
