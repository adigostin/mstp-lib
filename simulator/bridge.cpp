
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2020 Adi Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "bridge.h"
#include "wire.h"
#include "simulator.h"
#include "bridge_tree.h"
#include "edge/utility_functions.h"

using namespace D2D1;

const edge::nvp stp_version_nvps[] =
{
	{ STP_GetVersionString(STP_VERSION_LEGACY_STP), STP_VERSION_LEGACY_STP },
	{ STP_GetVersionString(STP_VERSION_RSTP), STP_VERSION_RSTP },
	{ STP_GetVersionString(STP_VERSION_MSTP), STP_VERSION_MSTP },
	{ nullptr, 0 }
};

const char stp_version_type_name[] = "stp_version";

static constexpr UINT WM_PACKET_RECEIVED = WM_APP + 1;

static constexpr uint8_t BpduDestAddress[6] = { 1, 0x80, 0xC2, 0, 0, 0 };

const char mac_address_property_traits::type_name[] = "mac_address";
void mac_address_property_traits::to_string (mac_address from, edge::out_sstream_i* to, const edge::string_convert_context_i*)
{
	std::stringstream ss;
	ss << std::uppercase << std::setfill('0') << std::hex
		<< std::setw(2) << (int) from[0] << std::setw(2) << (int) from[1] << std::setw(2) << (int) from[2]
		<< std::setw(2) << (int) from[3] << std::setw(2) << (int) from[4] << std::setw(2) << (int) from[5];
	to->write(ss.str());
}

void mac_address_property_traits::from_string (std::string_view str, mac_address& to, const edge::string_convert_context_i*)
{
	static constexpr char format_error_message[] = "Invalid address format. The Bridge Address must have the format XX:XX:XX:XX:XX:XX or XXXXXXXXXXXX (6 hex bytes).";

	size_t offset_multiplier;
	if (str.size() == 12)
	{
		offset_multiplier = 2;
	}
	else if (str.size() == 17)
	{
		if ((str[2] != ':') || (str[5] != ':') || (str[8] != ':') || (str[11] != ':') || (str[14] != ':'))
			throw edge::string_convert_exception(format_error_message);

		offset_multiplier = 3;
	}
	else
		throw edge::string_convert_exception(format_error_message);

	for (size_t i = 0; i < 6; i++)
	{
		wchar_t ch0 = str[i * offset_multiplier];
		wchar_t ch1 = str[i * offset_multiplier + 1];

		if (!iswxdigit(ch0) || !iswxdigit(ch1))
			throw edge::string_convert_exception(format_error_message);

		auto hn = (ch0 <= '9') ? (ch0 - '0') : ((ch0 >= 'a') ? (ch0 - 'a' + 10) : (ch0 - 'A' + 10));
		auto ln = (ch1 <= '9') ? (ch1 - '0') : ((ch1 >= 'a') ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10));
		to[i] = (hn << 4) | ln;
	}
}

static constexpr wchar_t helper_window_class_name[] = L"{C2A14267-93FD-44FD-89A3-809FBB66A20B}";
HINSTANCE bridge::_hinstance;
UINT_PTR bridge::_link_pulse_timer_id;
UINT_PTR bridge::_one_second_timer_id;
std::unordered_set<bridge*> bridge::_created_bridges;

bridge::bridge (size_t port_count, size_t msti_count, mac_address macAddress)
{
	for (size_t i = 0; i < 1 + msti_count; i++)
		_trees.push_back(std::make_unique<bridge_tree>(this, i));

	float offset = 0;
	for (size_t port_index = 0; port_index < port_count; port_index++)
	{
		offset += (port::PortToPortSpacing / 2 + port::InteriorWidth / 2);
		_ports.push_back(std::make_unique<port>(this, port_index, side::bottom, offset));
		offset += (port::InteriorWidth / 2 + port::PortToPortSpacing / 2);
	}

	_x = 0;
	_y = 0;
	_width = std::max (offset, MinWidth);
	_height = DefaultHeight;

	_stpBridge = STP_CreateBridge ((unsigned int)port_count, (unsigned int)msti_count, max_vlan_number, &StpCallbacks, macAddress.data(), 256);
	STP_EnableLogging (_stpBridge, true);
	STP_SetApplicationContext (_stpBridge, this);

	for (auto& port : _ports)
		port->invalidate().add_handler(&bridge::on_port_invalidated, this);

	// ----------------------------------------------------------------------------

	if (_created_bridges.empty())
	{
		static constexpr auto link_pulse_callback = [](HWND, UINT, UINT_PTR, DWORD)
		{
			for (auto bridge : _created_bridges)
			{
				if (bridge->project())
					bridge->OnLinkPulseTick();
			}
		};
		_link_pulse_timer_id = ::SetTimer (nullptr, 0, 16, link_pulse_callback); rassert(_link_pulse_timer_id);

		DWORD period = 950 + (std::random_device()() % 100);
		static constexpr auto one_second_callback = [](HWND, UINT, UINT_PTR, DWORD)
		{
			for (auto bridge : _created_bridges)
			{
				if (bridge->project() && !bridge->project()->simulation_paused())
					STP_OnOneSecondTick (bridge->_stpBridge, ::GetMessageTime());
			}
		};
		_one_second_timer_id = ::SetTimer (nullptr, 0, 1000, one_second_callback); rassert(_one_second_timer_id);

		static constexpr WNDPROC helper_window_proc = [](HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) -> LRESULT
		{
			if (msg == WM_PACKET_RECEIVED)
			{
				auto bridge = reinterpret_cast<::bridge*>(::GetWindowLongPtr (hwnd, GWLP_USERDATA));
				bridge->ProcessReceivedPackets();
				return 0;
			}
			return ::DefWindowProc (hwnd, msg, wparam, lparam);
		};
		BOOL bRes = ::GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR)helper_window_proc, &_hinstance); rassert(bRes);

		WNDCLASS wc = { sizeof(WNDCLASS) };
		wc.hInstance = _hinstance;
		wc.lpfnWndProc = helper_window_proc;
		wc.lpszClassName = helper_window_class_name;
		ATOM atom = ::RegisterClass(&wc);
	}
	
	rassert (_helper_window == nullptr);
	_helper_window = ::CreateWindow (helper_window_class_name, L"", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, _hinstance, 0); rassert (_helper_window != nullptr);
	::SetWindowLongPtr (_helper_window, GWLP_USERDATA, (LONG_PTR)this);

	_created_bridges.insert(this);
}

bridge::~bridge()
{
	_created_bridges.erase(this);

	::DestroyWindow (_helper_window);
	_helper_window = nullptr;

	if (_created_bridges.empty())
	{
		::UnregisterClass (helper_window_class_name, _hinstance);

		BOOL bres = ::KillTimer (nullptr, _one_second_timer_id); rassert(bres);
		_one_second_timer_id = 0;

		bres = ::KillTimer (nullptr, _link_pulse_timer_id); rassert(bres);
		_link_pulse_timer_id = 0;
	}

	// ----------------------------------------------------------------

	for (auto& port : _ports)
		port->invalidate().remove_handler(&bridge::on_port_invalidated, this);

	//while (!_trees.empty())
	//	this->bridge_tree_collection_i::remove_last();

	STP_DestroyBridge (_stpBridge);
}

edge::hierarchy_object_i* bridge::parent() const
{
	return _project;
}

void bridge::set_parent (project_i* parent)
{
	_project = parent;
}

project_i* bridge::project() const
{
	return _project;
}

void bridge::on_port_invalidated (void* arg, port* p)
{
	auto bridge = static_cast<class bridge*>(arg);
	invalidate_e::invoker(bridge->_em).invoke(bridge);
}

// Checks the wires and computes macOperational for each port on this bridge.
void bridge::OnLinkPulseTick()
{
	uint32_t now = (uint32_t) ::GetMessageTime();

	bool invalidate = false;
	for (size_t portIndex = 0; portIndex < _ports.size(); portIndex++)
	{
		auto port = _ports[portIndex].get();
		if (port->_missedLinkPulseCounter < port::MissedLinkPulseCounterMax)
		{
			port->_missedLinkPulseCounter++;
			if (port->_missedLinkPulseCounter == port::MissedLinkPulseCounterMax)
			{
				port->set_actual_speed(0);
				STP_OnPortDisabled (_stpBridge, (unsigned int) portIndex, now);
				invalidate = true;
			}
		}

		packet_transmit_e::invoker(_em).invoke(this, portIndex, link_pulse_t { now, port->supported_speed() });
	}

	if (invalidate)
		invalidate_e::invoker(_em).invoke(this);
}

void bridge::enqueue_received_packet (packet_t&& packet, size_t rxPortIndex)
{
	_rxQueue.push ({ rxPortIndex, std::move(packet) });
	::PostMessage (_helper_window, WM_PACKET_RECEIVED, 0, 0);
}

void bridge::ProcessReceivedPackets()
{
	bool invalidate = false;

	while (!_rxQueue.empty())
	{
		size_t rxPortIndex = _rxQueue.front().first;
		auto port = _ports[rxPortIndex].get();
		auto sd = std::move(_rxQueue.front().second);
		_rxQueue.pop();

		if (std::holds_alternative<link_pulse_t>(sd))
		{
			auto lpsd = std::get<link_pulse_t>(sd);
			bool oldMacOperational = port->_missedLinkPulseCounter < port::MissedLinkPulseCounterMax;
			port->_missedLinkPulseCounter = 0;
			if (oldMacOperational == false)
			{
				// Send a link pulse right away, to make sure the other port goes up before we send it any frame.
				packet_transmit_e::invoker(_em).invoke(this, rxPortIndex, link_pulse_t { lpsd.timestamp, port->supported_speed() });

				auto actual_speed = std::min (lpsd.sender_supported_speed, port->supported_speed());
				port->set_actual_speed(actual_speed);

				STP_OnPortEnabled (_stpBridge, (unsigned int) rxPortIndex, actual_speed, true, lpsd.timestamp);
				invalidate = true;
			}
		}
		else if (std::holds_alternative<frame_t>(sd))
		{
			auto fsd = std::get<frame_t>(sd);

			if (!port->mac_operational())
			{
				// The sender must be misbehaving (forgot to send link pulses). Currently the simulator is single-threaded so this shouldn't happen.
				rassert(false);
			}
			else
			{
				if ((fsd.data.size() >= 6) && (memcmp (&fsd.data[0], BpduDestAddress, 6) == 0))
				{
					// It's a BPDU.
					if (_bpdu_trapping_enabled)
					{
						STP_OnBpduReceived (_stpBridge, (unsigned int) rxPortIndex, &fsd.data[21], (unsigned int) (fsd.data.size() - 21), fsd.timestamp);
					}
					else
					{
						// broadcast it to the other ports.
						for (size_t txPortIndex = 0; txPortIndex < _ports.size(); txPortIndex++)
						{
							if (txPortIndex == rxPortIndex)
								continue;

							auto txPortAddress = GetPortAddress(txPortIndex);

							// If it already went through this port, we have a loop that would hang our UI.
							if (std::find (fsd.tx_path_taken.begin(), fsd.tx_path_taken.end(), txPortAddress) != fsd.tx_path_taken.end())
							{
								// We don't do anything here; we have code in wire.cpp that shows loops to the user - as thick red wires.
								//volatile int a = 0;
							}
							else
							{
								frame_t f;
								f.timestamp = fsd.timestamp;
								f.data = fsd.data;
								f.tx_path_taken = fsd.tx_path_taken;
								f.tx_path_taken.push_back (txPortAddress);

								packet_transmit_e::invoker(_em).invoke(this, txPortIndex, std::move(f));
							}
						}
					}
				}
				else
					rassert(false); // not implemented
			}
		}
		else
			rassert(false);
	}

	if (invalidate)
		invalidate_e::invoker(_em).invoke(this);
}

void bridge::set_location(float x, float y)
{
	if ((_x != x) || (_y != y))
	{
		_x = x;
		_y = y;
		invalidate_e::invoker(_em).invoke(this);
	}
}

void bridge::render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const
{
	auto treeIndex = STP_GetTreeIndexFromVlanNumber (_stpBridge, vlanNumber);

	const unsigned char* address = STP_GetBridgeAddress(_stpBridge)->bytes;

	std::stringstream text;
	float bridgeOutlineWidth = OutlineWidth;
	if (STP_IsBridgeStarted(_stpBridge))
	{
		auto stpVersion = STP_GetStpVersion(_stpBridge);
		auto treeIndex = STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
		bool isCistRoot = STP_IsCistRoot(_stpBridge);
		bool isRegionalRoot = (treeIndex > 0) && STP_IsRegionalRoot(_stpBridge, treeIndex);

		if ((treeIndex == 0) ? isCistRoot : isRegionalRoot)
			bridgeOutlineWidth *= 2;

		text << std::uppercase << std::setfill('0') << std::setw(4) << std::hex << STP_GetBridgePriority(_stpBridge, treeIndex) << '.'
			<< std::setw(2) << (int) address[0] << std::setw(2) << (int) address[1] << std::setw(2) << (int) address[2]
			<< std::setw(2) << (int) address[3] << std::setw(2) << (int) address[4] << std::setw(2) << (int) address[5]
			<< std::endl;
		text << "STP enabled (" << STP_GetVersionString(stpVersion) << ")" << std::endl;
		text << (isCistRoot ? "CIST Root Bridge\r\n" : "");
		if (stpVersion >= STP_VERSION_MSTP)
		{
			text << "VLAN " << std::dec << vlanNumber << ". Spanning tree: " << ((treeIndex == 0) ? "CIST(0)" : (std::string("MSTI") + std::to_string(treeIndex)).c_str()) << std::endl;
			text << (isRegionalRoot ? "Regional Root\r\n" : "");
		}
	}
	else
	{
		text << std::uppercase << std::setfill('0') << std::hex
			<< std::setw(2) << (int) address[0] << std::setw(2) << (int) address[1] << std::setw(2) << (int) address[2]
			<< std::setw(2) << (int) address[3] << std::setw(2) << (int) address[4] << std::setw(2) << (int) address[5]
			<< std::endl << "STP disabled\r\n(right-click to enable)";
	}

	// Draw bridge outline.
	D2D1_ROUNDED_RECT rr = RoundedRect (bounds(), RoundRadius, RoundRadius);
	edge::inflate (&rr, -bridgeOutlineWidth / 2);
	com_ptr<ID2D1SolidColorBrush> brush;
	dc->CreateSolidColorBrush (configIdColor, &brush);
	dc->FillRoundedRectangle (&rr, brush);//_powered ? dos._poweredFillBrush : dos._unpoweredBrush*
	dc->DrawRoundedRectangle (&rr, dos._brushWindowText, bridgeOutlineWidth);

	// Draw bridge text.
	auto tl = edge::text_layout (dos._dWriteFactory, dos._regularTextFormat, text.str().c_str());
	dc->DrawTextLayout ({ _x + OutlineWidth * 2 + 3, _y + OutlineWidth * 2 + 3}, tl.layout(), dos._brushWindowText);

	for (auto& port : _ports)
		port->render (dc, dos, vlanNumber);
}

void bridge::render_selection (ID2D1DeviceContext* rt, const edge::zoomer* zoomer, const drawing_resources& dos) const
{
	auto oldaa = rt->GetAntialiasMode();
	rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

	auto tl = zoomer->pointw_to_pointd ({ _x - OutlineWidth / 2, _y - OutlineWidth / 2 });
	auto br = zoomer->pointw_to_pointd ({ _x + _width + OutlineWidth / 2, _y + _height + OutlineWidth / 2 });
	rt->DrawRectangle ({ tl.x - 10, tl.y - 10, br.x + 10, br.y + 10 }, dos._brushHighlight, 2, dos._strokeStyleSelectionRect);

	rt->SetAntialiasMode(oldaa);
}

uint8_t bridge::hit_test (const D2D1::Matrix3x2F& wtr, D2D1_POINT_2F dLocation, float tolerance)
{
	auto tl = wtr.TransformPoint({ _x, _y });
	auto br = wtr.TransformPoint({ _x + _width, _y + _height });

	if ((dLocation.x >= tl.x) && (dLocation.y >= tl.y) && (dLocation.x < br.x) && (dLocation.y < br.y))
		return HTCodeInner;

	return {};
}

std::array<uint8_t, 6> bridge::GetPortAddress (size_t portIndex) const
{
	std::array<uint8_t, 6> pa = bridge_address();
	pa[5]++;
	if (pa[5] == 0)
	{
		pa[4]++;
		if (pa[4] == 0)
		{
			pa[3]++;
			if (pa[3] == 0)
				rassert(false); // not implemented
		}
	}

	return pa;
}

mac_address bridge::bridge_address() const
{
	mac_address address;
	auto x = sizeof(address);
	memcpy (address.data(), STP_GetBridgeAddress(_stpBridge)->bytes, 6);
	return address;
}

void bridge::set_bridge_address (mac_address address)
{
	if (memcmp(STP_GetBridgeAddress(_stpBridge)->bytes, address.data(), 6) != 0)
	{
		edge::value_property_change_args args = { bridge_address_property };
		edge::property_changing_e::invoker(_em).invoke(this, args);
		STP_SetBridgeAddress(_stpBridge, address.data(), GetMessageTime());
		edge::property_changed_e::invoker(_em).invoke(this, args);
	}
}

void bridge::move_port (class port* _port, D2D1_POINT_2F proposedLocation)
{
	float mouseX = proposedLocation.x - _x;
	float mouseY = proposedLocation.y - _y;

	float wh = _width / _height;

	// top side
	if ((mouseX > mouseY * wh) && (_width - mouseX) > mouseY * wh)
	{
		_port->_side = side::top;

		if (mouseX < port::InteriorWidth / 2)
			_port->_offset = port::InteriorWidth / 2;
		else if (mouseX > _width - port::InteriorWidth / 2)
			_port->_offset = _width - port::InteriorWidth / 2;
		else
			_port->_offset = mouseX;
	}

	// bottom side
	else if ((mouseX <= mouseY * wh) && (_width - mouseX) <= mouseY * wh)
	{
		_port->_side = side::bottom;

		if (mouseX < port::InteriorWidth / 2 + 1)
			_port->_offset = port::InteriorWidth / 2 + 1;
		else if (mouseX > _width - port::InteriorWidth / 2)
			_port->_offset = _width - port::InteriorWidth / 2;
		else
			_port->_offset = mouseX;
	}

	// left side
	if ((mouseX <= mouseY * wh) && (_width - mouseX) > mouseY * wh)
	{
		_port->_side = side::left;

		if (mouseY < port::InteriorWidth / 2)
			_port->_offset = port::InteriorWidth / 2;
		else if (mouseY > _height - port::InteriorWidth / 2)
			_port->_offset = _height - port::InteriorWidth / 2;
		else
			_port->_offset = mouseY;
	}

	// right side
	if ((mouseX > mouseY * wh) && (_width - mouseX) <= mouseY * wh)
	{
		_port->_side = side::right;

		if (mouseY < port::InteriorWidth / 2)
			_port->_offset = port::InteriorWidth / 2;
		else if (mouseY > _height - port::InteriorWidth / 2)
			_port->_offset = _height - port::InteriorWidth / 2;
		else
			_port->_offset = mouseY;
	}

	invalidate_e::invoker(_em).invoke(this);
}

void bridge::clear_log()
{
	_logLines.clear();
	_currentLogLine.text.clear();
	log_cleared_e::invoker(_em).invoke(this);
}

std::string bridge::mst_config_id_name() const
{
	auto configId = STP_GetMstConfigId(_stpBridge);
	size_t len = strnlen (configId->ConfigurationName, 32);
	return std::string(std::begin(configId->ConfigurationName), std::begin(configId->ConfigurationName) + len);
}

void bridge::set_mst_config_id_name (std::string value)
{
	if (value.size() > 32)
		throw std::invalid_argument("Invalid MST Config Name: more than 32 characters.");

	rassert(false);
	//this->on_property_changing(&mst_config_id_name_property);
	//STP_SetMstConfigName (_stpBridge, value.c_str(), GetMessageTime());
	//this->on_property_changed(&mst_config_id_name_property);
}

uint32_t bridge::GetMstConfigIdRevLevel() const
{
	auto id = STP_GetMstConfigId(_stpBridge);
	return ((unsigned short) id->RevisionLevelHigh << 8) | (unsigned short) id->RevisionLevelLow;
}

void bridge::SetMstConfigIdRevLevel (uint32_t revLevel)
{
	if (GetMstConfigIdRevLevel() != revLevel)
	{
		rassert(false);
		//this->on_property_changing(&mst_config_id_rev_level);
		//STP_SetMstConfigRevisionLevel (_stpBridge, revLevel, GetMessageTime());
		//this->on_property_changed(&mst_config_id_rev_level);
	}
}

std::string bridge::GetMstConfigIdDigest() const
{
	const unsigned char* digest = STP_GetMstConfigId(_stpBridge)->ConfigurationDigest;
	std::stringstream ss;
	ss << std::uppercase << std::setfill('0') << std::hex
		<< std::setw(2) << (int) digest[0]  << std::setw(2) << (int) digest[1]  << std::setw(2) << (int) digest[2]  << std::setw(2) << (int) digest[3]
		<< std::setw(2) << (int) digest[4]  << std::setw(2) << (int) digest[5]  << std::setw(2) << (int) digest[6]  << std::setw(2) << (int) digest[7]
		<< std::setw(2) << (int) digest[8]  << std::setw(2) << (int) digest[9]  << std::setw(2) << (int) digest[10] << std::setw(2) << (int) digest[11]
		<< std::setw(2) << (int) digest[12] << std::setw(2) << (int) digest[13] << std::setw(2) << (int) digest[14] << std::setw(2) << (int) digest[15];
	return ss.str();
}

void bridge::SetMstConfigTable (const STP_CONFIG_TABLE_ENTRY* entries, size_t entryCount)
{
	rassert(false);
	//this->on_property_changing (&mst_config_id_digest);
	//STP_SetMstConfigTable (_stpBridge, &entries[0], (unsigned int) entryCount, GetMessageTime());
	//this->on_property_changed (&mst_config_id_digest);
}

void bridge::set_stp_enabled (bool value)
{
	if (_deserializing)
	{
		_enable_stp_after_deserialize = value;
		return;
	}

	if (value && !STP_IsBridgeStarted(_stpBridge))
	{
		edge::value_property_change_args args = { stp_enabled_property };
		edge::property_changing_e::invoker(_em).invoke(this, args);
		STP_StartBridge (_stpBridge, GetMessageTime());
		edge::property_changed_e::invoker(_em).invoke(this, args);
		invalidate_e::invoker(_em).invoke(this);
	}
	else if (!value && STP_IsBridgeStarted(_stpBridge))
	{
		rassert(false);
		//this->on_property_changing(&stp_enabled_property);
		//STP_StopBridge (_stpBridge, GetMessageTime());
		//this->on_property_changed(&stp_enabled_property);
		invalidate_e::invoker(_em).invoke(this);
	}
}

void bridge::set_stp_version (STP_VERSION stp_version)
{
	if (STP_GetStpVersion(_stpBridge) != stp_version)
	{
		rassert(false);
		//this->on_property_changing(&stp_version_property);
		//STP_SetStpVersion(_stpBridge, stp_version, GetMessageTime());
		//this->on_property_changed(&stp_version_property);
	}
}

void bridge::set_bridge_max_age (uint32_t value)
{
	if ((value < 6) || (value > 40))
		throw std::invalid_argument("MaxAge must be in the range 6..40.\r\nThe default value, also recommended by the standard, is 20.");

	if (bridge_max_age() != value)
	{
		rassert(false);
		//this->on_property_changing (&bridge_max_age_property);
		//STP_SetBridgeMaxAge (_stpBridge, value, ::GetMessageTime());
		//this->on_property_changed (&bridge_max_age_property);
	}
}

void bridge::set_bridge_forward_delay (uint32_t value)
{
	if (bridge_forward_delay() != value)
	{
		rassert(false);
		//this->on_property_changing (&bridge_forward_delay_property);
		//STP_SetBridgeForwardDelay (_stpBridge, value, ::GetMessageTime());
		//this->on_property_changed (&bridge_forward_delay_property);
	}
}

void bridge::set_tx_hold_count (uint32_t value)
{
	if (tx_hold_count() != value)
	{
		STP_SetTxHoldCount(_stpBridge, value, ::GetMessageTime());
		invalidate_e::invoker(_em).invoke(this);
	}
}

#pragma region properties
void bridge::set_x (float x)
{ 
	if (_x != x)
	{
		invalidate_e::invoker(_em).invoke(this);
		_x = x;
		invalidate_e::invoker(_em).invoke(this);
	}
}

void bridge::set_y (float y)
{
	if (_y != y)
	{
		invalidate_e::invoker(_em).invoke(this);
		_y = y;
		invalidate_e::invoker(_em).invoke(this);
	}
}

void bridge::set_width (float width)
{
	if (_width != width)
	{
		invalidate_e::invoker(_em).invoke(this);
		_width = width;
		invalidate_e::invoker(_em).invoke(this);
	}
}

void bridge::set_height (float height)
{
	if (_height != height)
	{
		invalidate_e::invoker(_em).invoke(this);
		_height = height;
		invalidate_e::invoker(_em).invoke(this);
	}
}

size_t bridge::mst_config_table_get_value_count() const
{
	unsigned int entry_count;
	STP_GetMstConfigTable(_stpBridge, &entry_count);
	return entry_count;
}

uint32_t bridge::mst_config_table_get_value(size_t i) const
{
	unsigned int entry_count;
	auto entries = STP_GetMstConfigTable(_stpBridge, &entry_count);
	return entries[i].treeIndex;
}

void bridge::mst_config_table_set_value(size_t i, uint32_t value)
{
	unsigned int entry_count;
	auto table = STP_GetMstConfigTable (_stpBridge, &entry_count);
	rassert (i < entry_count);
	if (table->treeIndex != value)
	{
		edge::value_collection_property_change_args args = { &mst_config_table_property, i, edge::collection_property_change_type::set };
		rassert(false);
		//this->on_property_changing(args);
		//STP_SetMstConfigTableEntry (_stpBridge, (unsigned int)i, value, ::GetMessageTime());
		//this->on_property_changed(args);
	}
}

bool bridge::mst_config_table_changed (size_t i) const
{
	unsigned int entry_count;
	const STP_CONFIG_TABLE_ENTRY* entries = STP_GetMstConfigTable (_stpBridge, &entry_count);
	rassert(i < entry_count);
	return entries[i].treeIndex != 0;
}

void bridge::on_deserializing (edge::xml_deserializer_i* de)
{
	_deserializing = true;
	_enable_stp_after_deserialize = stp_enabled_property.default_value().value();
}

void bridge::on_deserialized  (edge::xml_deserializer_i* de)
{
	if (_enable_stp_after_deserialize)
		STP_StartBridge (_stpBridge, ::GetMessageTime());
	_deserializing = false;
}

static const pg::property_group bridge_times_group = { 5, "Timer Params (Table 13-5)" };
static const pg::property_group mst_group = { 10, "MST Config Id" };

const mac_address_p bridge::bridge_address_property {
	"Address", nullptr, nullptr, true,
	&bridge_address,
	&set_bridge_address,
};

const bool_p bridge::stp_enabled_property {
	"STPEnabled", nullptr, nullptr, true,
	&stp_enabled,
	&set_stp_enabled,
	false, // default_value
};

const stp_version_p bridge::stp_version_property { "StpVersion", &stp_version, &set_stp_version, STP_VERSION_RSTP };

const size_p bridge::port_count_property {
	"PortCount", nullptr, nullptr, true,
	&port_count,
	nullptr,
};

const size_p bridge::msti_count_property {
	"MstiCount", nullptr, nullptr, true,
	&msti_count,
	nullptr,
};

const string_p bridge::mst_config_id_name_property {
	"MstConfigName", &mst_group, nullptr, true,
	&mst_config_id_name,
	&set_mst_config_id_name,
};

const edge::typed_value_collection_property<edge::uint32_property_traits> bridge::mst_config_table_property = {
	"MstConfigTable",
	&mst_config_table_get_value_count,
	&mst_config_table_get_value,
	&mst_config_table_set_value,
	&mst_config_table_changed,
};

const uint32_p bridge::mst_config_id_rev_level {
	"MstConfigRevLevel", &mst_group, nullptr, true,
	&GetMstConfigIdRevLevel,
	&SetMstConfigIdRevLevel,
	0,
};

const config_id_digest_p bridge::mst_config_id_digest
	= { "MstConfigDigest", &mst_group, nullptr, &GetMstConfigIdDigest, nullptr, };

#pragma region Timer and related parameters from Table 13-5
const uint32_p bridge::migrate_time_property {
	"MigrateTime", &bridge_times_group, nullptr, true,
	&bridge_migrate_time,
	nullptr,
	3, // default_value
};

const uint32_p bridge::bridge_hello_time_property {
	"BridgeHelloTime", &bridge_times_group, nullptr, true,
	&bridge_hello_time,
	nullptr,
	2, // default_value
};

const uint32_p bridge::bridge_max_age_property {
	"BridgeMaxAge", &bridge_times_group, nullptr, true,
	&bridge_max_age,
	&set_bridge_max_age,
	20, // default_value
};

const uint32_p bridge::bridge_forward_delay_property {
	"BridgeForwardDelay", &bridge_times_group, nullptr, true,
	&bridge_forward_delay,
	&set_bridge_forward_delay,
	15, // default_value
};

const uint32_p bridge::tx_hold_count_property {
	"TxHoldCount", &bridge_times_group, nullptr, true,
	&tx_hold_count,
	&set_tx_hold_count,
	6 // default_value
};

const uint32_p bridge::max_hops_property {
	"MaxHops",
	&bridge_times_group,
	"Setting this is not yet implemented in the library",
	true,
	&max_hops,
	nullptr,
	20 // default_value
};

const edge::static_value_property<edge::float_property_traits> bridge::x_property
	= { "X", &x, &set_x };

const edge::static_value_property<edge::float_property_traits> bridge::y_property
	= { "Y", &y, &set_y };

const edge::static_value_property<edge::float_property_traits> bridge::width_property
	= { "Width", &width, &set_width };

const edge::static_value_property<edge::float_property_traits> bridge::height_property
	= { "Height", &height, &set_height };

const edge::typed_object_collection_property1<bridge_tree> bridge::trees_prop = {
	"BridgeTrees",
	[](const edge::object* obj) { return checked_static_cast<const bridge*>(obj)->_trees.size(); },
	[](const edge::object* obj, size_t i) { return checked_static_cast<const bridge*>(obj)->_trees[i].get(); },
	nullptr, // inserter
	nullptr, // remover
};

const edge::typed_object_collection_property1<port> bridge::ports_prop = { 
	"Ports", 
	nullptr, // size_getter
	nullptr, // getter
	nullptr, // inserter
	nullptr, // remover
};

#pragma endregion

const edge::property* const bridge::_properties[] = {
	&bridge_address_property,
	&stp_enabled_property,
	&stp_version_property,
	&port_count_property,
	&msti_count_property,
	&mst_config_id_name_property,
	&mst_config_id_rev_level,
	&mst_config_table_property,
	&mst_config_id_digest,
	&migrate_time_property,
	&bridge_hello_time_property,
	&bridge_max_age_property,
	&bridge_forward_delay_property,
	&tx_hold_count_property,
	&max_hops_property,
	&x_property, &y_property, &width_property, &height_property,
	&trees_prop,
	&ports_prop,
};

const xtype<bridge, edge::size_t_property_traits, edge::size_t_property_traits, mac_address_property_traits> bridge::_type = {
	"Bridge",
	nullptr,
	_properties,
	&std::make_unique,
	&port_count_property,
	&msti_count_property,
	&bridge_address_property,
};
#pragma endregion

#pragma region STP Callbacks
const STP_CALLBACKS bridge::StpCallbacks =
{
	&StpCallback_EnableBpduTrapping,
	&StpCallback_EnableLearning,
	&StpCallback_EnableForwarding,
	&StpCallback_TransmitGetBuffer,
	&StpCallback_TransmitReleaseBuffer,
	&StpCallback_FlushFdb,
	&StpCallback_DebugStrOut,
	&StpCallback_OnTopologyChange,
	&StpCallback_OnPortRoleChanged,
	&StpCallback_AllocAndZeroMemory,
	&StpCallback_FreeMemory,
};

void* bridge::StpCallback_AllocAndZeroMemory(unsigned int size)
{
	void* p = malloc(size);
	memset (p, 0, size);
	return p;
}

void bridge::StpCallback_FreeMemory(void* p)
{
	free(p);
}

void* bridge::StpCallback_TransmitGetBuffer (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int bpduSize, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	auto txPort = b->_ports[portIndex].get();

	b->_txPacketData.resize (bpduSize + 21);
	memcpy (&b->_txPacketData[0], BpduDestAddress, 6);
	memcpy (&b->_txPacketData[6], &b->GetPortAddress(portIndex)[0], 6);
	b->_txTransmittingPort = txPort;
	b->_txTimestamp = timestamp;
	return &b->_txPacketData[21];
}

void bridge::StpCallback_TransmitReleaseBuffer (const STP_BRIDGE* bridge, void* bufferReturnedByGetBuffer)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));

	frame_t info;
	info.data = std::move(b->_txPacketData);
	info.timestamp = b->_txTimestamp;
	packet_transmit_e::invoker(b->_em).invoke(b, b->_txTransmittingPort->port_index(), std::move(info));
}

void bridge::StpCallback_EnableBpduTrapping (const STP_BRIDGE* bridge, bool enable, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	b->_bpdu_trapping_enabled = enable;
}

void bridge::StpCallback_EnableLearning (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	invalidate_e::invoker(b->_em).invoke(b);
}

void bridge::StpCallback_EnableForwarding (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	invalidate_e::invoker(b->_em).invoke(b);
}

void bridge::StpCallback_FlushFdb (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	b->_ports[portIndex]->_trees[treeIndex]->flush_fdb(timestamp);
}

void bridge::StpCallback_DebugStrOut (const STP_BRIDGE* bridge, int portIndex, int treeIndex, const char* nullTerminatedString, unsigned int stringLength, unsigned int flush)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));

	if (stringLength > 0)
	{
		if (b->_currentLogLine.text.empty())
		{
			b->_currentLogLine.text.assign (nullTerminatedString, (size_t) stringLength);
			b->_currentLogLine.portIndex = portIndex;
			b->_currentLogLine.treeIndex = treeIndex;
		}
		else
		{
			if ((b->_currentLogLine.portIndex != portIndex) || (b->_currentLogLine.treeIndex != treeIndex))
			{
				b->_logLines.push_back(std::make_unique<BridgeLogLine>(std::move(b->_currentLogLine)));
				log_line_generated_e::invoker(b->_em).invoke(b, b->_logLines.back().get());
			}

			b->_currentLogLine.text.append (nullTerminatedString, (size_t) stringLength);
		}

		if (!b->_currentLogLine.text.empty() && (b->_currentLogLine.text.back() == L'\n'))
		{
			b->_logLines.push_back(std::make_unique<BridgeLogLine>(std::move(b->_currentLogLine)));
			log_line_generated_e::invoker(b->_em).invoke(b, b->_logLines.back().get());
		}
	}

	if (flush && !b->_currentLogLine.text.empty())
	{
		b->_logLines.push_back(std::make_unique<BridgeLogLine>(std::move(b->_currentLogLine)));
		log_line_generated_e::invoker(b->_em).invoke(b, b->_logLines.back().get());
	}
}

void bridge::StpCallback_OnTopologyChange (const STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	b->_trees[treeIndex]->on_topology_change(timestamp);
}

void bridge::StpCallback_OnPortRoleChanged (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	invalidate_e::invoker(b->_em).invoke(b);
}
#pragma endregion
