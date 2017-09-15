
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2017 Adi Gostin, distributed under Apache License v2.0.

#ifndef MSTP_LIB_BASE_TYPES_H
#define MSTP_LIB_BASE_TYPES_H

#include "stp.h"
#include <assert.h>

// ============================================================================

#define CIST_INDEX	0

struct STP_BRIDGE;

typedef unsigned char SM_STATE;

int Cmp (const void* _p1, const void* _p2, int size);

// ============================================================================

// Two-byte value in network byte order (big-endian), not aligned in memory.
struct INV_UINT2
{
private:
	unsigned char vh;
	unsigned char vl;

public:
	unsigned short GetValue () const
	{
		return (vh << 8) | vl;
	}

	void operator= (unsigned short value)
	{
		vh = value >> 8;
		vl = (unsigned char) value;
	}

	bool operator== (unsigned short rhs) const
	{
		return GetValue () == rhs;
	}

	bool operator== (const INV_UINT2& rhs) const
	{
		return ((this->vh == rhs.vh) && (this->vl == rhs.vl));
	}

	operator unsigned short ()
	{
		return (vh << 8) | vl;
	}
};

// ============================================================================

// Four-byte value in network byte order (big-endian), not aligned in memory.
struct INV_UINT4
{
private:
	unsigned char vhh;
	unsigned char vhl;
	unsigned char vlh;
	unsigned char vll;

public:
	unsigned int GetValue () const
	{
		return (vhh << 24) | (vhl << 16) | (vlh << 8) | vll;
	}

	void operator= (unsigned int newValue)
	{
		vhh = (unsigned char) (newValue >> 24);
		vhl = (unsigned char) (newValue >> 16);
		vlh = (unsigned char) (newValue >> 8);
		vll = (unsigned char) newValue;
	}

	const INV_UINT4& operator += (unsigned int rhs)
	{
		*this = this->GetValue () + rhs;
		return *this;
	}

	bool operator== (const INV_UINT4& rhs) const
	{
		return (this->vhh == rhs.vhh)
			&& (this->vhl == rhs.vhl)
			&& (this->vlh == rhs.vlh)
			&& (this->vll == rhs.vll);
	}
};

// ============================================================================

// Eight-byte BridgeId structure as defined in the STP standard, not aligned in memory.
struct BRIDGE_ID
{
private:
	INV_UINT2          _priority;
	STP_BRIDGE_ADDRESS _address;

public:
	bool operator== (const BRIDGE_ID& rhs) const
	{
		return ((this->_priority == rhs._priority) && (this->_address == rhs._address));
	}

	bool operator != (const BRIDGE_ID& rhs) const
	{
		return !this->operator== (rhs);
	}

	void SetPriority (unsigned short settablePriorityComponent, unsigned short mstid)
	{
		assert ((settablePriorityComponent % 4096) == 0);

		_priority = settablePriorityComponent | mstid;
	}

	void SetAddress (const unsigned char address[6])
	{
		_address.bytes[0] = address[0];
		_address.bytes[1] = address[1];
		_address.bytes[2] = address[2];
		_address.bytes[3] = address[3];
		_address.bytes[4] = address[4];
		_address.bytes[5] = address[5];
	}

	void Set (unsigned short settablePriorityComponent, unsigned short mstid, const unsigned char address[6])
	{
		SetPriority (settablePriorityComponent, mstid);
		SetAddress (address);
	}

	unsigned short GetPriority () const
	{
		return _priority.GetValue ();
	}

	const STP_BRIDGE_ADDRESS& GetAddress() const
	{
		return _address;
	}

	operator const unsigned char* () const
	{
		return (const unsigned char*) this;
	}
};

// ============================================================================

struct PORT_ID
{
private:
	unsigned char _high;
	unsigned char _low;
	// Valid Port Numbers are in the range 1 through 4095. Value zero in _low means that the structure contains uninitialized data.

public:
	bool IsInitialized () const { return _low != 0; }

	void Set (unsigned char priority, unsigned short portNumber);
	void Reset ();
	unsigned char GetPriority () const;
	void SetPriority (unsigned char priority);
	unsigned short GetPortNumber () const;
	unsigned short GetPortIdentifier () const;
	bool IsBetterThan (const PORT_ID& rhs) const;
};

// ============================================================================
// 13.9 and 13.10 in 802.1Q-2011
struct PRIORITY_VECTOR
{
	BRIDGE_ID	RootId;					// a) - used for CIST, zero for MSTIs
	INV_UINT4	ExternalRootPathCost;	// b) - used for CIST, zero for MSTIs
	BRIDGE_ID	RegionalRootId;			// c)
	INV_UINT4	InternalRootPathCost;	// d)
	BRIDGE_ID	DesignatedBridgeId;		// e)
	PORT_ID		DesignatedPortId;		// f)

	bool operator== (const PRIORITY_VECTOR& rhs) const
	{
		return Cmp (this, &rhs, sizeof (*this)) == 0;
	}

	bool operator!= (const PRIORITY_VECTOR& rhs) const
	{
		return Cmp (this, &rhs, sizeof (*this)) != 0;
	}

	bool IsBetterThan (const PRIORITY_VECTOR& rhs) const
	{
		return (Cmp (this, &rhs, sizeof (*this)) < 0);
	}

	bool IsBetterThanOrSameAs (const PRIORITY_VECTOR& rhs) const
	{
		return (Cmp (this, &rhs, sizeof (*this)) <= 0);
	}

	bool IsWorseThan (const PRIORITY_VECTOR& rhs) const
	{
		return (Cmp (this, &rhs, sizeof (*this)) > 0);
	}

	bool IsWorseThanOrSameAs (const PRIORITY_VECTOR& rhs) const
	{
		return (Cmp (this, &rhs, sizeof (*this)) >= 0);
	}

	bool IsNotBetterThan (const PRIORITY_VECTOR& rhs) const
	{
		return this->IsWorseThanOrSameAs (rhs);
	}

	// 13.9, page 336 in 802.1Q-2011:
	// A received CIST message priority vector is superior to the port priority vector if, and only if, the message
	// priority vector is better than the port priority vector, or the Designated Bridge Identifier Bridge Address and
	// Designated Port Identifier Port Number components are the same; in which case, the message has been
	// transmitted from the same Designated Port as a previously received superior message
	//
	// 13.10, page 338 in 802.1Q-2011: wording is identical for MSTI priority vectors.
	bool IsSuperiorTo (const PRIORITY_VECTOR& rhs) const
	{
		if (this->IsBetterThan (rhs))
			return true;

		if ((this->DesignatedBridgeId.GetAddress () == rhs.DesignatedBridgeId.GetAddress ())
			&& (this->DesignatedPortId.GetPortNumber () == rhs.DesignatedPortId.GetPortNumber ()))
		{
			return true;
		}

		return false;
	}
};

// ============================================================================

struct TIMES
{
	unsigned short ForwardDelay;
	unsigned short HelloTime;
	unsigned short MaxAge;
	unsigned short MessageAge;
	unsigned char remainingHops;

	bool operator!= (const TIMES& rhs) const
	{
		return Cmp (this, &rhs, sizeof (*this)) != 0;
	}

	bool operator== (const TIMES& rhs) const
	{
		return Cmp (this, &rhs, sizeof (*this)) == 0;
	}
};

// ============================================================================

const char* GetPortRoleName (STP_PORT_ROLE role);

enum INFO_IS
{
	INFO_IS_UNKNOWN,
	INFO_IS_MINE,
	INFO_IS_AGED,
	INFO_IS_RECEIVED,
	INFO_IS_DISABLED,
};

enum RCVD_INFO
{
	RCVD_INFO_UNKNOWN,
	RCVD_INFO_SUPERIOR_DESIGNATED,
	RCVD_INFO_REPEATED_DESIGNATED,
	RCVD_INFO_INFERIOR_DESIGNATED,
	RCVD_INFO_INFERIOR_ROOT_ALTERNATE,
	RCVD_INFO_OTHER,

	RCVD_INFO_CONFIRMED_ROOT_MSG, // used by 802.1w-2001
};

// ============================================================================

#endif
