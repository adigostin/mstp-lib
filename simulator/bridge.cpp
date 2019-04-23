#include "pch.h"
#include "bridge.h"
#include "wire.h"
#include "simulator.h"
#include "win32/d2d_window.h"

using namespace std;
using namespace D2D1;
using namespace edge;

static constexpr UINT WM_ONE_SECOND_TIMER  = WM_APP + 1;
static constexpr UINT WM_LINK_PULSE_TIMER  = WM_APP + 2;
static constexpr UINT WM_PACKET_RECEIVED   = WM_APP + 3;

static constexpr uint8_t BpduDestAddress[6] = { 1, 0x80, 0xC2, 0, 0, 0 };

std::string mac_address_to_string (mac_address address)
{
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << (int) address[0] << setw(2) << (int) address[1] << setw(2) << (int) address[2]
		<< setw(2) << (int) address[3] << setw(2) << (int) address[4] << setw(2) << (int) address[5];
	return ss.str();
}

template<typename char_type> bool mac_address_from_string (std::basic_string_view<char_type> str, mac_address& to)
{
//	static constexpr char FormatErrorMessage[] = u8"Invalid address format. The Bridge Address must have the format XX:XX:XX:XX:XX:XX or XXXXXXXXXXXX (6 hex bytes).";

	int offsetMultiplier;
	if (str.size() == 12)
	{
		offsetMultiplier = 2;
	}
	else if (str.size() == 17)
	{
		if ((str[2] != ':') || (str[5] != ':') || (str[8] != ':') || (str[11] != ':') || (str[14] != ':'))
			return false;

		offsetMultiplier = 3;
	}
	else
		return false;

	for (size_t i = 0; i < 6; i++)
	{
		wchar_t ch0 = str[i * offsetMultiplier];
		wchar_t ch1 = str[i * offsetMultiplier + 1];

		if (!iswxdigit(ch0) || !iswxdigit(ch1))
			return false;

		auto hn = (ch0 <= '9') ? (ch0 - '0') : ((ch0 >= 'a') ? (ch0 - 'a' + 10) : (ch0 - 'A' + 10));
		auto ln = (ch1 <= '9') ? (ch1 - '0') : ((ch1 >= 'a') ? (ch1 - 'a' + 10) : (ch1 - 'A' + 10));
		to[i] = (hn << 4) | ln;
	}

	return true;
}

template bool mac_address_from_string (std::basic_string_view<char> from, mac_address& to);
template bool mac_address_from_string (std::basic_string_view<wchar_t> from, mac_address& to);

#pragma region bridge::HelperWindow
bridge::HelperWindow::HelperWindow (bridge* bridge)
	: _bridge(bridge)
{
	HINSTANCE hInstance;
	BOOL bRes = ::GetModuleHandleExW (GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, (LPCWSTR) &SubclassProc, &hInstance); assert(bRes);

	_hwnd = ::CreateWindowExW (0, L"STATIC", L"", 0, 0, 0, 0, 0, HWND_MESSAGE, 0, hInstance, 0); assert (_hwnd != nullptr);

	bRes = ::SetWindowSubclass (_hwnd, SubclassProc, 0, (DWORD_PTR) this); assert (bRes);

	auto link_pulse_callback = [](void* lpParameter, BOOLEAN TimerOrWaitFired)
	{
		// We're on a worker thread. Let's post this message and continue processing on the GUI thread.
		auto helperWindow = static_cast<HelperWindow*>(lpParameter);
		::PostMessage (helperWindow->_hwnd, WM_LINK_PULSE_TIMER, 0, 0);
	};
	bRes = ::CreateTimerQueueTimer (&_linkPulseTimerHandle, nullptr, link_pulse_callback, this, 16, 16, 0); assert(bRes);

	DWORD period = 950 + (std::random_device()() % 100);
	auto one_second_callback = [](void* lpParameter, BOOLEAN TimerOrWaitFired)
	{
		// We're on a worker thread. Let's post this message and continue processing on the GUI thread.
		auto helperWindow = static_cast<HelperWindow*>(lpParameter);
		::PostMessage (helperWindow->_hwnd, WM_ONE_SECOND_TIMER, 0, 0);
	};
	bRes = ::CreateTimerQueueTimer (&_oneSecondTimerHandle, nullptr, one_second_callback, this, period, period, 0); assert(bRes);
}

bridge::HelperWindow::~HelperWindow()
{
	::DeleteTimerQueueTimer (nullptr, _oneSecondTimerHandle, INVALID_HANDLE_VALUE);
	::DeleteTimerQueueTimer (nullptr, _linkPulseTimerHandle, INVALID_HANDLE_VALUE);
	::RemoveWindowSubclass (_hwnd, SubclassProc, 0);
	::DestroyWindow (_hwnd);
}

//static
LRESULT CALLBACK bridge::HelperWindow::SubclassProc (HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam, UINT_PTR uIdSubclass, DWORD_PTR dwRefData)
{
	auto hw = (HelperWindow*) dwRefData;

	if (uMsg == WM_LINK_PULSE_TIMER)
	{
		// We use a timer on a single thread for pulses because we want to avoid links going down due to delays on some threads but not on others.
		hw->_bridge->OnLinkPulseTick();
		return 0;
	}
	else if (uMsg == WM_ONE_SECOND_TIMER)
	{
		if (!hw->_bridge->project()->simulation_paused())
			STP_OnOneSecondTick (hw->_bridge->_stpBridge, GetMessageTime());
		return 0;
	}
	else if (uMsg == WM_PACKET_RECEIVED)
	{
		if (!hw->_bridge->project()->simulation_paused())
			hw->_bridge->ProcessReceivedPackets();
		return 0;
	}

	return DefSubclassProc(hWnd, uMsg, wParam, lParam);
}
#pragma endregion

bridge::bridge (uint32_t portCount, uint32_t mstiCount, mac_address macAddress)
{
	for (unsigned int i = 0; i < 1 + mstiCount; i++)
		_trees.push_back (make_unique<bridge_tree>(this, i));

	float offset = 0;
	for (unsigned int portIndex = 0; portIndex < portCount; portIndex++)
	{
		offset += (port::PortToPortSpacing / 2 + port::InteriorWidth / 2);
		auto port = unique_ptr<class port>(new class port(this, portIndex, side::bottom, offset));
		_ports.push_back (move(port));
		offset += (port::InteriorWidth / 2 + port::PortToPortSpacing / 2);
	}

	_x = 0;
	_y = 0;
	_width = max (offset, MinWidth);
	_height = DefaultHeight;

	_stpBridge = STP_CreateBridge (portCount, mstiCount, max_vlan_number, &StpCallbacks, macAddress.data(), 256);
	STP_EnableLogging (_stpBridge, true);
	STP_SetApplicationContext (_stpBridge, this);

	for (auto& port : _ports)
		port->GetInvalidateEvent().add_handler(&OnPortInvalidate, this);
}

bridge::~bridge()
{
	for (auto& port : _ports)
		port->GetInvalidateEvent().remove_handler(&OnPortInvalidate, this);
	STP_DestroyBridge (_stpBridge);
}

void bridge::on_added_to_project(project_i* project)
{
	base::on_added_to_project(project);
	assert (_helper_window == nullptr);
	_helper_window = std::make_unique<HelperWindow>(this);
}

void bridge::on_removing_from_project(project_i* project)
{
	_helper_window = nullptr;
	base::on_removing_from_project(project);
}

//static
void bridge::OnPortInvalidate (void* callbackArg, renderable_object* object)
{
	auto bridge = static_cast<class bridge*>(callbackArg);
	bridge->event_invoker<invalidate_e>()(bridge);
}

// Checks the wires and computes macOperational for each port on this bridge.
void bridge::OnLinkPulseTick()
{
	if (project()->simulation_paused())
		return;

	bool invalidate = false;
	for (size_t portIndex = 0; portIndex < _ports.size(); portIndex++)
	{
		auto port = _ports[portIndex].get();
		if (port->_missedLinkPulseCounter < port::MissedLinkPulseCounterMax)
		{
			port->_missedLinkPulseCounter++;
			if (port->_missedLinkPulseCounter == port::MissedLinkPulseCounterMax)
			{
				STP_OnPortDisabled (_stpBridge, (unsigned int) portIndex, ::GetMessageTime());
				invalidate = true;
			}
		}

		this->event_invoker<LinkPulseEvent>()(this, portIndex, ::GetMessageTime());
	}

	if (invalidate)
		this->event_invoker<invalidate_e>()(this);
}

void bridge::ProcessLinkPulse (size_t rxPortIndex, unsigned int timestamp)
{
	auto port = _ports.at(rxPortIndex).get();
	bool oldMacOperational = port->_missedLinkPulseCounter < port::MissedLinkPulseCounterMax;
	port->_missedLinkPulseCounter = 0;
	if (oldMacOperational == false)
	{
		STP_OnPortEnabled (_stpBridge, (unsigned int) rxPortIndex, 100, true, timestamp);
		this->event_invoker<invalidate_e>()(this);
	}
}

void bridge::EnqueuePacket (PacketInfo&& packet, size_t rxPortIndex)
{
	_rxQueue.push ({ rxPortIndex, move(packet) });
	::PostMessage (_helper_window->_hwnd, WM_PACKET_RECEIVED, (WPARAM)(void*)this, 0);
}

void bridge::ProcessReceivedPackets()
{
	bool invalidate = false;

	while (!_rxQueue.empty())
	{
		size_t rxPortIndex = _rxQueue.front().first;
		auto port = _ports.at(rxPortIndex).get();
		auto rp = move(_rxQueue.front().second);
		_rxQueue.pop();

		bool oldMacOperational = port->_missedLinkPulseCounter < port::MissedLinkPulseCounterMax;
		port->_missedLinkPulseCounter = 0;
		if (oldMacOperational == false)
		{
			STP_OnPortEnabled (_stpBridge, (unsigned int) rxPortIndex, 100, true, rp.timestamp);
			invalidate = true;
		}

		if ((rp.data.size() >= 6) && (memcmp (&rp.data[0], BpduDestAddress, 6) == 0))
		{
			// It's a BPDU.
			if (_bpdu_trapping_enabled)
			{
				STP_OnBpduReceived (_stpBridge, (unsigned int) rxPortIndex, &rp.data[21], (unsigned int) (rp.data.size() - 21), rp.timestamp);
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
					if (find (rp.txPortPath.begin(), rp.txPortPath.end(), txPortAddress) != rp.txPortPath.end())
					{
						// We don't do anything here; we have code in wire.cpp that shows loops to the user - as thick red wires.
					}
					else
					{
						this->event_invoker<PacketTransmitEvent>()(this, txPortIndex, PacketInfo(rp));
					}
				}
			}
		}
		else
			assert(false); // not implemented
	}

	if (invalidate)
		this->event_invoker<invalidate_e>()(this);
}

void bridge::SetLocation(float x, float y)
{
	if ((_x != x) || (_y != y))
	{
		_x = x;
		_y = y;
		this->event_invoker<invalidate_e>()(this);
	}
}

void bridge::Render (ID2D1RenderTarget* dc, const drawing_resources& dos, unsigned int vlanNumber, const D2D1_COLOR_F& configIdColor) const
{
	auto treeIndex = STP_GetTreeIndexFromVlanNumber (_stpBridge, vlanNumber);

	stringstream text;
	float bridgeOutlineWidth = OutlineWidth;
	if (STP_IsBridgeStarted(_stpBridge))
	{
		auto stpVersion = STP_GetStpVersion(_stpBridge);
		auto treeIndex = STP_GetTreeIndexFromVlanNumber(_stpBridge, vlanNumber);
		bool isCistRoot = STP_IsCistRoot(_stpBridge);
		bool isRegionalRoot = (treeIndex > 0) && STP_IsRegionalRoot(_stpBridge, treeIndex);

		if ((treeIndex == 0) ? isCistRoot : isRegionalRoot)
			bridgeOutlineWidth *= 2;

		text << uppercase << setfill('0') << setw(4) << hex << STP_GetBridgePriority(_stpBridge, treeIndex) << '.' << mac_address_to_string(bridge_address()) << endl;
		text << "STP enabled (" << STP_GetVersionString(stpVersion) << ")" << endl;
		text << (isCistRoot ? "CIST Root Bridge\r\n" : "");
		if (stpVersion >= STP_VERSION_MSTP)
		{
			text << "VLAN " << dec << vlanNumber << ". Spanning tree: " << ((treeIndex == 0) ? "CIST(0)" : (string("MSTI") + to_string(treeIndex)).c_str()) << endl;
			text << (isRegionalRoot ? "Regional Root\r\n" : "");
		}
	}
	else
	{
		text << uppercase << setfill('0') << hex << mac_address_to_string(bridge_address()) << endl << "STP disabled\r\n(right-click to enable)";
	}

	// Draw bridge outline.
	D2D1_ROUNDED_RECT rr = RoundedRect (GetBounds(), RoundRadius, RoundRadius);
	InflateRoundedRect (&rr, -bridgeOutlineWidth / 2);
	com_ptr<ID2D1SolidColorBrush> brush;
	dc->CreateSolidColorBrush (configIdColor, &brush);
	dc->FillRoundedRectangle (&rr, brush/*_powered ? dos._poweredFillBrush : dos._unpoweredBrush*/);
	dc->DrawRoundedRectangle (&rr, dos._brushWindowText, bridgeOutlineWidth);

	// Draw bridge text.
	auto tl = text_layout::create (dos._dWriteFactory, dos._regularTextFormat, text.str().c_str());
	dc->DrawTextLayout ({ _x + OutlineWidth * 2 + 3, _y + OutlineWidth * 2 + 3}, tl.layout, dos._brushWindowText);

	for (auto& port : _ports)
		port->Render (dc, dos, vlanNumber);
}

void bridge::render_selection (const edge::zoomable_i* zoomable, ID2D1RenderTarget* rt, const drawing_resources& dos) const
{
	auto oldaa = rt->GetAntialiasMode();
	rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_ALIASED);

	auto tl = zoomable->pointw_to_pointd ({ _x - OutlineWidth / 2, _y - OutlineWidth / 2 });
	auto br = zoomable->pointw_to_pointd ({ _x + _width + OutlineWidth / 2, _y + _height + OutlineWidth / 2 });
	rt->DrawRectangle ({ tl.x - 10, tl.y - 10, br.x + 10, br.y + 10 }, dos._brushHighlight, 2, dos._strokeStyleSelectionRect);

	rt->SetAntialiasMode(oldaa);
}

renderable_object::HTResult bridge::hit_test (const edge::zoomable_i* zoomable, D2D1_POINT_2F dLocation, float tolerance)
{
	for (auto& p : _ports)
	{
		auto ht = p->hit_test (zoomable, dLocation, tolerance);
		if (ht.object != nullptr)
			return ht;
	}

	auto tl = zoomable->pointw_to_pointd ({ _x, _y });
	auto br = zoomable->pointw_to_pointd ({ _x + _width, _y + _height });

	if ((dLocation.x >= tl.x) && (dLocation.y >= tl.y) && (dLocation.x < br.x) && (dLocation.y < br.y))
		return { this, HTCodeInner };

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
				assert(false); // not implemented
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
		this->on_property_changing(&bridge_address_property);
		STP_SetBridgeAddress(_stpBridge, address.data(), GetMessageTime());
		this->on_property_changed(&bridge_address_property);
	}
}

void bridge::SetCoordsForInteriorPort (class port* _port, D2D1_POINT_2F proposedLocation)
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

	this->event_invoker<invalidate_e>()(this);
}

void bridge::clear_log()
{
	_logLines.clear();
	_currentLogLine.text.clear();
	this->event_invoker<log_cleared_e>()(this);
}

std::string bridge::mst_config_id_name() const
{
	auto configId = STP_GetMstConfigId(_stpBridge);
	size_t len = strnlen (configId->ConfigurationName, 32);
	return std::string(std::begin(configId->ConfigurationName), std::begin(configId->ConfigurationName) + len);
}

void bridge::set_mst_config_id_name (std::string_view value)
{
	if (value.length() > 32)
		throw invalid_argument("Invalid MST Config Name: more than 32 characters.");

	char null_terminated[33];
	memcpy (null_terminated, value.data(), value.size());
	null_terminated[value.size()] = 0;

	this->on_property_changing(&mst_config_id_name_property);
	STP_SetMstConfigName (_stpBridge, null_terminated, GetMessageTime());
	this->on_property_changed(&mst_config_id_name_property);
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
		this->on_property_changing(&MstConfigIdRevLevel);
		STP_SetMstConfigRevisionLevel (_stpBridge, revLevel, GetMessageTime());
		this->on_property_changed(&MstConfigIdRevLevel);
	}
}

std::string bridge::GetMstConfigIdDigest() const
{
	const unsigned char* digest = STP_GetMstConfigId(_stpBridge)->ConfigurationDigest;
	stringstream ss;
	ss << uppercase << setfill('0') << hex
		<< setw(2) << (int) digest[0]  << setw(2) << (int) digest[1]  << setw(2) << (int) digest[2]  << setw(2) << (int) digest[3]
		<< setw(2) << (int) digest[4]  << setw(2) << (int) digest[5]  << setw(2) << (int) digest[6]  << setw(2) << (int) digest[7]
		<< setw(2) << (int) digest[8]  << setw(2) << (int) digest[9]  << setw(2) << (int) digest[10] << setw(2) << (int) digest[11]
		<< setw(2) << (int) digest[12] << setw(2) << (int) digest[13] << setw(2) << (int) digest[14] << setw(2) << (int) digest[15];
	return ss.str();
}

void bridge::SetMstConfigTable (const STP_CONFIG_TABLE_ENTRY* entries, size_t entryCount)
{
	this->on_property_changing (&MstConfigIdDigest);
	STP_SetMstConfigTable (_stpBridge, &entries[0], (unsigned int) entryCount, GetMessageTime());
	this->on_property_changed (&MstConfigIdDigest);
}

void bridge::set_stp_enabled (bool value)
{
	if (value && !STP_IsBridgeStarted(_stpBridge))
	{
		this->on_property_changing(&stp_enabled_property);
		STP_StartBridge (_stpBridge, GetMessageTime());
		this->on_property_changed(&stp_enabled_property);
	}
	else if (!value && STP_IsBridgeStarted(_stpBridge))
	{
		this->on_property_changing(&stp_enabled_property);
		STP_StopBridge (_stpBridge, GetMessageTime());
		this->on_property_changed(&stp_enabled_property);
	}
}

void bridge::set_stp_version (STP_VERSION stp_version)
{
	if (STP_GetStpVersion(_stpBridge) != stp_version)
	{
		this->on_property_changing(&stp_version_property);
		STP_SetStpVersion(_stpBridge, stp_version, GetMessageTime());
		this->on_property_changed(&stp_version_property);
	}
}

void bridge::set_bridge_max_age (uint32_t value)
{
	if (bridge_max_age() != value)
	{
		this->on_property_changing (&bridge_max_age_property);
		STP_SetBridgeMaxAge (_stpBridge, value, ::GetMessageTime());
		this->on_property_changed (&bridge_max_age_property);
	}
}

void bridge::set_bridge_forward_delay (uint32_t value)
{
	if (bridge_forward_delay() != value)
	{
		this->on_property_changing (&bridge_forward_delay_property);
		STP_SetBridgeForwardDelay (_stpBridge, value, ::GetMessageTime());
		this->on_property_changed (&bridge_forward_delay_property);
	}
}

void bridge::set_tx_hold_count (uint32_t value)
{
	if (tx_hold_count() != value)
	{
		this->on_property_changing(&tx_hold_count_property);
		STP_SetTxHoldCount(_stpBridge, value, ::GetMessageTime());
		this->on_property_changed(&tx_hold_count_property);
	}
}

#pragma region properties

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
	assert (i < entry_count);
	if (table->treeIndex != value)
	{
		property_change_args args = { &mst_config_table_property, i, collection_property_change_type::set };
		this->on_property_changing(args);
		STP_SetMstConfigTableEntry (_stpBridge, (unsigned int)i, value, ::GetMessageTime());
		this->on_property_changed(args);
	}
}

bool bridge::mst_config_table_changed() const
{
	unsigned int entry_count;
	const STP_CONFIG_TABLE_ENTRY* entries = STP_GetMstConfigTable (_stpBridge, &entry_count);

	static constexpr STP_CONFIG_TABLE_ENTRY zero = { };
	for (auto e = entries; e < &entries[entry_count]; e++)
	{
		if (memcmp(e, &zero, sizeof(zero)))
			return true; // changed from default
	}

	return false; // not changed
}

static const edge::property_group bridge_times_group = { 5, "Timer Params (Table 13-5)" };
static const edge::property_group mst_group = { 10, "MST Config Id" };

const mac_address_p bridge::bridge_address_property {
	"Address", nullptr, nullptr, ui_visible::yes,
	static_cast<mac_address_p::member_getter_t>(&bridge_address),
	static_cast<mac_address_p::member_setter_t>(&set_bridge_address),
	std::nullopt,
};

const bool_p bridge::stp_enabled_property {
	"STPEnabled", nullptr, nullptr, ui_visible::yes,
	static_cast<bool_p::member_getter_t>(&stp_enabled),
	static_cast<bool_p::member_setter_t>(&set_stp_enabled),
	false,
};

const stp_version_p bridge::stp_version_property {
	"StpVersion", nullptr, nullptr, ui_visible::yes,
	static_cast<stp_version_p::member_getter_t>(&stp_version),
	static_cast<stp_version_p::member_setter_t>(&set_stp_version),
	STP_VERSION_RSTP,
};

const uint32_p bridge::port_count_property {
	"PortCount", nullptr, nullptr, ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&port_count),
	nullptr,
	std::nullopt,
};

const edge::uint32_p bridge::msti_count_property {
	"MstiCount", nullptr, nullptr, ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&msti_count),
	nullptr,
	std::nullopt,
};

const temp_string_p bridge::mst_config_id_name_property {
	"MstConfigName", &mst_group, nullptr, ui_visible::yes,
	static_cast<temp_string_p::member_getter_t>(&mst_config_id_name),
	static_cast<temp_string_p::member_setter_t>(&set_mst_config_id_name),
	std::nullopt,
};

const typed_value_collection_property<bridge, uint32_property_traits> bridge::mst_config_table_property {
	"MstConfigTable", nullptr, nullptr, ui_visible::no,
	&mst_config_table_get_value_count,
	&mst_config_table_get_value,
	&mst_config_table_set_value,
	nullptr, // insert
	nullptr, // remove
	&mst_config_table_changed,
};

const edge::uint32_p bridge::MstConfigIdRevLevel {
	"Revision Level", &mst_group, nullptr, ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&bridge::GetMstConfigIdRevLevel),
	static_cast<uint32_p::member_setter_t>(&bridge::SetMstConfigIdRevLevel),
	0,
};

const config_id_digest_p bridge::MstConfigIdDigest {
	"Digest", &mst_group, nullptr, ui_visible::yes,
	static_cast<temp_string_p::member_getter_t>(&bridge::GetMstConfigIdDigest),
	nullptr,
	std::nullopt,
};

#pragma region Timer and related parameters from Table 13-5
const uint32_p bridge::migrate_time_property {
	"MigrateTime", &bridge_times_group, nullptr, ui_visible::yes,
	[](const object* o) { return 3u; },
	nullptr,
	3,
};

const uint32_p bridge::bridge_hello_time_property {
	"BridgeHelloTime", &bridge_times_group, nullptr, ui_visible::yes,
	[](const object* o) { return 2u; },
	nullptr,
	2,
};

const uint32_p bridge::bridge_max_age_property {
	"BridgeMaxAge", &bridge_times_group, nullptr, ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&bridge_max_age),
	static_cast<uint32_p::member_setter_t>(&set_bridge_max_age),
	20,
};

const uint32_p bridge::bridge_forward_delay_property {
	"BridgeForwardDelay", &bridge_times_group, nullptr, ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&bridge_forward_delay),
	static_cast<uint32_p::member_setter_t>(&set_bridge_forward_delay),
	15,
};

const uint32_p bridge::tx_hold_count_property {
	"TxHoldCount", &bridge_times_group, nullptr, ui_visible::yes,
	static_cast<uint32_p::member_getter_t>(&tx_hold_count),
	static_cast<uint32_p::member_setter_t>(&set_tx_hold_count),
	6
};

const uint32_p bridge::max_hops_property {
	"MaxHops",
	&bridge_times_group,
	"Setting this is not yet implemented in the library",
	ui_visible::yes,
	[](const object* o) { return 20u; },
	nullptr,
	20
};

const float_p bridge::x_property {
	"X", nullptr, nullptr, ui_visible::no,
	static_cast<float_p::member_getter_t>(&x),
	static_cast<float_p::member_setter_t>(&set_x),
	std::nullopt
};

const float_p bridge::y_property {
	"Y", nullptr, nullptr, ui_visible::no,
	static_cast<float_p::member_getter_t>(&y),
	static_cast<float_p::member_setter_t>(&set_y),
	std::nullopt
};

const float_p bridge::width_property {
	"Width", nullptr, nullptr, ui_visible::no,
	static_cast<float_p::member_getter_t>(&width),
	static_cast<float_p::member_setter_t>(&set_width),
	std::nullopt
};

const float_p bridge::height_property {
	"Height", nullptr, nullptr, ui_visible::no,
	static_cast<float_p::member_getter_t>(&height),
	static_cast<float_p::member_setter_t>(&set_height),
	std::nullopt
};

const typed_object_collection_property<bridge, bridge_tree> bridge::trees_property {
	"BridgeTrees", nullptr, nullptr, ui_visible::no,
	&tree_count, &tree
};

const typed_object_collection_property<bridge, port> bridge::ports_property {
	"Ports", nullptr, nullptr, ui_visible::no,
	&port_count, &port
};
#pragma endregion

const edge::property* const bridge::_properties[] = {
	&bridge_address_property,
	&stp_enabled_property,
	&stp_version_property,
	&port_count_property,
	&msti_count_property,
	&mst_config_id_name_property,
	&MstConfigIdRevLevel,
	&mst_config_table_property,
	&MstConfigIdDigest,
	&migrate_time_property,
	&bridge_hello_time_property,
	&bridge_max_age_property,
	&bridge_forward_delay_property,
	&tx_hold_count_property,
	&max_hops_property,
	&x_property, &y_property, &width_property, &height_property,
	&trees_property,
	&ports_property,
};

const xtype<bridge, uint32_p, uint32_p, mac_address_p>  bridge::_type = {
	"Bridge",
	&base::_type,
	_properties,
	[](uint32_t port_count, uint32_t msti_count, mac_address address) { return new bridge(port_count, msti_count, address); },
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
	&StpCallback_OnNotifiedTopologyChange,
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

	PacketInfo info;
	info.data = move(b->_txPacketData);
	info.timestamp = b->_txTimestamp;
	b->event_invoker<PacketTransmitEvent>()(b, b->_txTransmittingPort->port_index(), move(info));
}

void bridge::StpCallback_EnableBpduTrapping (const STP_BRIDGE* bridge, bool enable, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	b->_bpdu_trapping_enabled = enable;
}

void bridge::StpCallback_EnableLearning (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	b->event_invoker<invalidate_e>()(b);
}

void bridge::StpCallback_EnableForwarding (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, bool enable, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	b->event_invoker<invalidate_e>()(b);
}

void bridge::StpCallback_FlushFdb (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, enum STP_FLUSH_FDB_TYPE flushType)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
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
				b->_logLines.push_back(make_unique<BridgeLogLine>(move(b->_currentLogLine)));
				b->event_invoker<LogLineGenerated>()(b, b->_logLines.back().get());
			}

			b->_currentLogLine.text.append (nullTerminatedString, (size_t) stringLength);
		}

		if (!b->_currentLogLine.text.empty() && (b->_currentLogLine.text.back() == L'\n'))
		{
			b->_logLines.push_back(make_unique<BridgeLogLine>(move(b->_currentLogLine)));
			b->event_invoker<LogLineGenerated>()(b, b->_logLines.back().get());
		}
	}

	if (flush && !b->_currentLogLine.text.empty())
	{
		b->_logLines.push_back(make_unique<BridgeLogLine>(move(b->_currentLogLine)));
		b->event_invoker<LogLineGenerated>()(b, b->_logLines.back().get());
	}
}

void bridge::StpCallback_OnTopologyChange (const STP_BRIDGE* bridge, unsigned int treeIndex, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	b->_trees[treeIndex]->on_topology_change(timestamp);
}

void bridge::StpCallback_OnNotifiedTopologyChange (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, unsigned int timestamp)
{
}

void bridge::StpCallback_OnPortRoleChanged (const STP_BRIDGE* bridge, unsigned int portIndex, unsigned int treeIndex, STP_PORT_ROLE role, unsigned int timestamp)
{
	auto b = static_cast<class bridge*>(STP_GetApplicationContext(bridge));
	b->event_invoker<invalidate_e>()(b);
}
#pragma endregion
