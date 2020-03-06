
#include "switch.h"
#include "smi.h"
#include <TM4C1294KCPDT.h>
#include <stdint.h>
#include <assert.h>
#include <stdio.h>

// Multi-Switch Register values
#define SMI_BUSY  (1u << 15)
#define SMI_22    (1u << 12)
#define SMI_READ  (0b10u << 10)
#define SMI_WRITE (0b01u << 10)
#define DEVADDR 5u
#define REGADDR 0u

static constexpr uint32_t smi_command_reg = 0;
static constexpr uint32_t smi_data_reg = 1;
static constexpr uint32_t global2_command_reg = 0x18;
static constexpr uint32_t global2_data_reg = 0x19;

uint32_t switch_read_register (uint32_t mdio_addr, switch_dev_addr dev_addr, uint32_t reg_addr)
{
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
	smi_write (mdio_addr, smi_data_reg, value);

	// command is for multichip addressing mode (See "SMI Command Register" in the 88E6352 spec).
	uint32_t command = (1 << 15) | (1 << 12) | (0b01 << 10) | (dev_addr << 5) | reg_addr;
	smi_write (mdio_addr, smi_command_reg, command);

	// Wait for the write command to be executed by the switch hardware.
	while (smi_read (mdio_addr, smi_command_reg) & (1 << 15));
}

uint32_t switch_read_phy_register (uint32_t mdio_addr, switch_phy phy, uint32_t reg_addr)
{
	uint32_t phy_read_command = SMI_BUSY | SMI_22 | SMI_READ  | (phy << DEVADDR) | (reg_addr << REGADDR);
	switch_write_register (mdio_addr, switch_dev_addr_global2, global2_command_reg, phy_read_command);

	while (switch_read_register (mdio_addr, switch_dev_addr_global2, global2_command_reg) & SMI_BUSY)
		;

	return switch_read_register (mdio_addr, switch_dev_addr_global2, global2_data_reg);
}

void switch_write_phy_register (uint32_t mdio_addr, switch_phy phy, uint32_t reg_addr, uint32_t value)
{
	switch_write_register (mdio_addr, switch_dev_addr_global2, global2_data_reg, value);

    uint32_t phy_write_command  = SMI_BUSY | SMI_22 | SMI_WRITE | (phy << DEVADDR) | (reg_addr << REGADDR);
	switch_write_register (mdio_addr, switch_dev_addr_global2, global2_command_reg, phy_write_command);

	while (switch_read_register (mdio_addr, switch_dev_addr_global2, global2_command_reg) & SMI_BUSY)
		;
}

static void power_up_phy (uint32_t mdio_addr, switch_phy phy)
{
	auto value = switch_read_phy_register (mdio_addr, phy, 0);
	value = value & (~(1u << 11) | (1 << 15));
	switch_write_phy_register (mdio_addr, phy, 0, value);

	value = switch_read_phy_register (mdio_addr, phy, 16);
	value &= ~(1 << 2);
	switch_write_phy_register (mdio_addr, phy, 16, value);
}

void switch_power_up_phys (uint32_t mdio_addr)
{
	power_up_phy(mdio_addr, switch_phy_0);
	power_up_phy(mdio_addr, switch_phy_1);
	power_up_phy(mdio_addr, switch_phy_2);
	power_up_phy(mdio_addr, switch_phy_3);
	power_up_phy(mdio_addr, switch_phy_4);

	switch_write_phy_register (mdio_addr, switch_phy_serdes, 22, 1); // select register page 1 (that's where all the SERDER registers are)
	power_up_phy (mdio_addr, switch_phy_serdes);
	switch_write_phy_register (mdio_addr, switch_phy_serdes, 22, 0); // select again register page 0
}
