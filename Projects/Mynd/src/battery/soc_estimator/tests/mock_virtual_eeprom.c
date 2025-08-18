// mock virtual_eeprom.c

#include <stdint.h>
#include <string.h>
#include <stdio.h>

#define EEPROM_SIZE 1024

static uint16_t mockEEPROM[EEPROM_SIZE];

int vEEPROM_Init(void)
{
    memset(mockEEPROM, 0xFF, sizeof(mockEEPROM)); // Initialize EEPROM with 0xFF
    return 0;                                     // Success
}

int vEEPROM_AddressWrite(uint16_t addr, uint16_t value)
{
    if (addr >= EEPROM_SIZE)
    {
        return -1; // Address out of range
    }
    mockEEPROM[addr] = value;
    return 0; // Success
}

int vEEPROM_AddressWriteBuffer(uint16_t addr, const uint16_t *data, uint16_t size)
{
    if (addr + size > EEPROM_SIZE)
    {
        return -1; // Address out of range
    }
    memcpy(&mockEEPROM[addr], data, size * sizeof(uint16_t));
    return 0; // Success
}

int vEEPROM_AddressRead(uint16_t addr, uint16_t *value)
{
    if (addr >= EEPROM_SIZE)
    {
        return -1; // Address out of range
    }
    *value = mockEEPROM[addr];
    return 0; // Success
}

int vEEPROM_AddressReadBuffer(uint16_t addr, uint16_t *target, uint16_t size)
{
    if (addr + size > EEPROM_SIZE)
    {
        return -1; // Address out of range
    }
    memcpy(target, &mockEEPROM[addr], size * sizeof(uint16_t));
    return 0; // Success
}
