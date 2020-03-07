
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "bridge.h"
#include "wire.h"
#include "simulator.h"
#include "test_helpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

extern const project_factory_t project_factory;

TEST_CLASS(project_tests)
{
public:
	TEST_METHOD(TestMethod1)
	{
		uint32_t port_count = 4;
		uint32_t msti_count = 0;
		auto p = project_factory();
		p->insert_bridge(0, std::make_unique<bridge>(port_count, msti_count, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 }));
		p->insert_bridge(1, std::make_unique<bridge>(port_count, msti_count, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x70 }));
		p->insert_bridge(2, std::make_unique<bridge>(port_count, msti_count, mac_address{ 0x10, 0x20, 0x30, 0x40, 0x50, 0x80 }));

		auto w = std::make_unique<wire>();
		w->set_p0(p->bridges()[1]->ports()[2].get());
		w->set_p1(p->bridges()[2]->ports()[3].get());
		p->insert_wire(0, std::move(w));

		Assert::AreEqual (p->bridges()[1]->ports()[2].get(), std::get<connected_wire_end>(p->wires()[0]->p0()));
		Assert::AreEqual (p->bridges()[2]->ports()[3].get(), std::get<connected_wire_end>(p->wires()[0]->p1()));

		p->remove_bridge((size_t)0);

		Assert::AreEqual (p->bridges()[0]->ports()[2].get(), std::get<connected_wire_end>(p->wires()[0]->p0()));
		Assert::AreEqual (p->bridges()[1]->ports()[3].get(), std::get<connected_wire_end>(p->wires()[0]->p1()));
	}
};
