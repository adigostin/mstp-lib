
#include "switch.h"
#include "smi.h"
#include <TM4C1294KCPDT.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

// Multi-Switch Register values
#define SMI_BUSY (1<<15)
#define SMI_22 (1<<12)
#define SMI_READ (0b10<<10)
#define SMI_WRITE (0b01<<10)
#define DEVADDR 5
#define REGADDR 0

static constexpr uint32_t smi_command_reg = 0;
static constexpr uint32_t smi_data_reg = 1;
static constexpr uint32_t global2_command_reg = 0x18;
static constexpr uint32_t global2_data_reg = 0x19;
static constexpr uint32_t port_control_reg = 4;

uint32_t switch_read_register (uint32_t mdio_addr, switch_dev_addr dev_addr, uint32_t reg_addr)
{
	// PHY registers cannot be accessed directly; use switch_read_phy_register instead. (See Chapter 11 in the 88E6352 FS.)
	assert (dev_addr >= 0x10); 

	// command is for multichip addressing mode (See "SMI Command Register" in the 88E6352 spec).
	uint32_t command = (1 << 15) | (1 << 12) | (0b10 << 10) | (dev_addr << 5) | reg_addr;
	smi_write (mdio_addr, smi_command_reg, command);

	// Wait for the read command to be executed by the switch hardware.
	while (smi_read (mdio_addr, smi_command_reg) & (1 << 15));

	// Read the result of the read operation.
	uint32_t value = smi_read (mdio_addr, smi_data_reg);
	return value;
}

void switch_write_register (uint32_t mdio_addr, switch_dev_addr dev_addr, uint32_t reg_addr, uint32_t value)
{
	// PHY registers cannot be accessed directly; use switch_read_phy_register instead. (See Chapter 11 in the 88E6352 FS.)
	assert (dev_addr >= 0x10); 

	smi_write (mdio_addr, smi_data_reg, value);

	// command is for multichip addressing mode (See "SMI Command Register" in the 88E6352 spec).
	uint32_t command = (1 << 15) | (1 << 12) | (0b01 << 10) | (dev_addr << 5) | reg_addr;
	smi_write (mdio_addr, smi_command_reg, command);

	// Wait for the write command to be executed by the switch hardware.
	while (smi_read (mdio_addr, smi_command_reg) & (1 << 15));
}

uint32_t switch_read_phy_register (uint32_t mdio_addr, switch_dev_addr phy_dev_addr, uint32_t reg_addr)
{
	assert ((phy_dev_addr < 5) || (phy_dev_addr == 15));

	uint32_t phyReadCommand = SMI_BUSY | SMI_22 | SMI_READ  | (phy_dev_addr << DEVADDR) | (reg_addr << REGADDR);
	switch_write_register (mdio_addr, switch_dev_addr_global2, global2_command_reg, phyReadCommand);

	while (switch_read_register (mdio_addr, switch_dev_addr_global2, global2_command_reg) & SMI_BUSY)
		;

	return switch_read_register (mdio_addr, switch_dev_addr_global2, global2_data_reg);
}

void switch_write_phy_register (uint32_t mdio_addr, switch_dev_addr phy_dev_addr, uint32_t reg_addr, uint32_t value)
{
	assert ((phy_dev_addr < 5) || (phy_dev_addr == 15));

	switch_write_register (mdio_addr, switch_dev_addr_global2, global2_data_reg, value);

    uint32_t phyWriteCommand  = SMI_BUSY | SMI_22 | SMI_WRITE | (phy_dev_addr << DEVADDR) | (reg_addr << REGADDR);
	switch_write_register (mdio_addr, switch_dev_addr_global2, global2_command_reg, phyWriteCommand);

	while (switch_read_register (mdio_addr, switch_dev_addr_global2, global2_command_reg) & SMI_BUSY)
		;
}

static void power_up_phy (uint32_t mdio_addr, switch_dev_addr phy_dev_addr)
{
	auto value = switch_read_phy_register (mdio_addr, phy_dev_addr, 0);
	value = value & ~(1u << 11) | (1 << 15);
	switch_write_phy_register (mdio_addr, phy_dev_addr, 0, value);

	value = switch_read_phy_register (mdio_addr, phy_dev_addr, 16);
	value &= ~(1 << 2);
	switch_write_phy_register (mdio_addr, phy_dev_addr, 16, value);
}

void switch_init (uint32_t mdio_addr)
{
	uint32_t id = switch_read_register (mdio_addr, switch_dev_addr_port0, 3);
	assert ((id & 0xFFF0) == 0x3520);

    for (uint32_t phy_dev_addr = 0; phy_dev_addr < 5; phy_dev_addr++)
		power_up_phy (mdio_addr, (switch_dev_addr) phy_dev_addr);

	switch_write_phy_register (mdio_addr, switch_dev_addr_serdes, 22, 1);
	power_up_phy (mdio_addr, switch_dev_addr_serdes);
	switch_write_phy_register (mdio_addr, switch_dev_addr_serdes, 22, 0);

	for (uint32_t port = 0x10; port < 0x16; port++)
	{
        uint32_t original = switch_read_register (mdio_addr, (switch_dev_addr)port, port_control_reg);
		switch_write_register (mdio_addr, (switch_dev_addr)port, port_control_reg, ((original & ~(0b11)) | (0b11)));
	}
}
