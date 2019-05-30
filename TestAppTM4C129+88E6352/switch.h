
#ifndef MARVELL_MDIO_H_
#define MARVELL_MDIO_H_

#include <stdint.h>

enum switch_dev_addr
{
	switch_dev_addr_phy0    = 0,
	switch_dev_addr_phy1    = 1,
	switch_dev_addr_phy2    = 2,
	switch_dev_addr_phy3    = 3,
	switch_dev_addr_phy4    = 4,
	switch_dev_addr_serdes  = 15,
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

void     switch_init (uint32_t mdio_address);
uint32_t switch_read_register      (uint32_t mdio_addr, switch_dev_addr dev_addr,     uint32_t reg_addr);
void     switch_write_register     (uint32_t mdio_addr, switch_dev_addr dev_addr,     uint32_t reg_addr, uint32_t value);
uint32_t switch_read_phy_register  (uint32_t mdio_addr, switch_dev_addr phy_dev_addr, uint32_t reg_addr);
void     switch_write_phy_register (uint32_t mdio_addr, switch_dev_addr phy_dev_addr, uint32_t reg_addr, uint32_t value);
#endif
