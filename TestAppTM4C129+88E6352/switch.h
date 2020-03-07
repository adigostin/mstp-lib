
#ifndef MARVELL_MDIO_H_
#define MARVELL_MDIO_H_

#include <stdint.h>

enum switch_phy
{
	switch_phy_0      = 0,
	switch_phy_1      = 1,
	switch_phy_2      = 2,
	switch_phy_3      = 3,
	switch_phy_4      = 4,
	switch_phy_serdes = 15,
};

enum switch_dev_addr
{
	switch_dev_addr_port0   = 0x10,
	switch_dev_addr_port1   = 0x11,
	switch_dev_addr_port2   = 0x12,
	switch_dev_addr_port3   = 0x13,
	switch_dev_addr_port4   = 0x14,
	switch_dev_addr_port5   = 0x15,
	switch_dev_addr_port6   = 0x16,
	switch_dev_addr_global1 = 0x1B,
	switch_dev_addr_global2 = 0x1C,
	switch_dev_addr_global3 = 0x1D,
};

void     switch_power_up_phys      (uint32_t mdio_addr);
uint32_t switch_read_register      (uint32_t mdio_addr, switch_dev_addr dev_addr, uint32_t reg_addr);
void     switch_write_register     (uint32_t mdio_addr, switch_dev_addr dev_addr, uint32_t reg_addr, uint32_t value);
uint32_t switch_read_phy_register  (uint32_t mdio_addr, switch_phy phy, uint32_t reg_addr);
void     switch_write_phy_register (uint32_t mdio_addr, switch_phy phy, uint32_t reg_addr, uint32_t value);
#endif
