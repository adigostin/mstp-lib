
// This file is part of the mstp-lib library, available at https://github.com/adigostin/mstp-lib
// Copyright (c) 2011-2026 Adrian Gostin, distributed under Apache License v2.0.

#include "pch.h"
#include "internal/stp_bpdu.h"
#include "TestHelpers.h"

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

TEST_CLASS(bpdu_tests)
{
	static constexpr uint8_t stp_config_bpdu[35] =  {
		0, 0,
		0, // protocolVersionId = LegacySTP
		0, // bpduType = STP Config
		0, // cistFlags
		0, 1, 2, 3, 4, 5, 6, 7, // cistRootId
		0, 0, 0, 0, // cistExternalPathCost
		0, 1, 2, 3, 4, 5, 6, 7, // cistRegionalRootId
		0, 0, // cistPortId
		0, 0, // MessageAge
		0, 0, // MaxAge
		0, 0, // HelloTime
		0, 0, // ForwardDelay
	};

	static constexpr uint8_t tcn_bpdu[4] = { 0, 0, 0, 0x80 };

	static constexpr uint8_t rstp_bpdu[36] = {
		0, 0,
		2, // protocolVersionId RSTP
		2, // RST / MST / SPT BPDU
		0, // cistFlags
		0, 1, 2, 3, 4, 5, 6, 7, // cistRootId
		0, 0, 0, 0, // cistExternalPathCost
		0, 1, 2, 3, 4, 5, 6, 7, // cistRegionalRootId
		0, 0, // cistPortId
		0, 0, // MessageAge
		0, 0, // MaxAge
		0, 0, // HelloTime
		0, 0, // ForwardDelay
		0,    // Version1Length
	};

	static constexpr uint8_t mstp_bpdu_without_mstis[102] = {
		0, 0,
		3, // protocolVersionId MSTP
		2, // RST / MST / SPT BPDU
		0, // cistFlags
		0, 1, 2, 3, 4, 5, 6, 7, // cistRootId
		0, 0, 0, 0, // cistExternalPathCost
		0, 1, 2, 3, 4, 5, 6, 7, // cistRegionalRootId
		0, 0, // cistPortId
		0, 0, // MessageAge
		0, 0, // MaxAge
		0, 0, // HelloTime
		0, 0, // ForwardDelay
		0,    // Version1Length
		0, 64, // Version3Length

		// mstConfigId
		0, // ConfigurationIdentifierFormatSelector
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, // ConfigurationName
		0, // RevisionLevelHigh
		0, // RevisionLevelLow
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, // ConfigurationDigest

		0, 0, 0, 0, // cistInternalRootPathCost
		0, 1, 2, 3, 4, 5, 6, 7, // cistBridgeId
		0, // cistRemainingHops

		// mstiConfigMessages
	};

	static constexpr uint8_t mstp_bpdu_with_mstis[] = {
		0, 0,
		3, // protocolVersionId MSTP
		2, // RST / MST / SPT BPDU
		0, // cistFlags
		0, 1, 2, 3, 4, 5, 6, 7, // cistRootId
		0, 0, 0, 0, // cistExternalPathCost
		0, 1, 2, 3, 4, 5, 6, 7, // cistRegionalRootId
		0, 0, // cistPortId
		0, 0, // MessageAge
		0, 0, // MaxAge
		0, 0, // HelloTime
		0, 0, // ForwardDelay
		0,    // Version1Length
		0, 64 + 16, // Version3Length

		// mstConfigId
		0, // ConfigurationIdentifierFormatSelector
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, // ConfigurationName
		0, // RevisionLevelHigh
		0, // RevisionLevelLow
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5, // ConfigurationDigest

		0, 0, 0, 0, // cistInternalRootPathCost
		0, 1, 2, 3, 4, 5, 6, 7, // cistBridgeId
		0, // cistRemainingHops

		// mstiConfigMessages
		0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 0, 1, 2, 3, 4, 5,
	};

	TEST_METHOD(validate_truncated_bpdu_header)
	{
		static constexpr uint8_t bpdu[3] = { };
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_UNKNOWN, STP_GetValidatedBpduType (STP_VERSION_MSTP, bpdu, sizeof(bpdu)));
	}

	TEST_METHOD(validate_truncated_stp_config_bpdu)
	{
		uint8_t bpdu[sizeof(stp_config_bpdu) - 1];
		memcpy (bpdu, stp_config_bpdu, sizeof(bpdu));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_UNKNOWN, STP_GetValidatedBpduType (STP_VERSION_MSTP, bpdu, sizeof(bpdu)));
	}

	TEST_METHOD(validate_bad_protocol_identifier)
	{
		static constexpr uint8_t bpdu[36] = { 1, 0, 0, 0 };
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_UNKNOWN, STP_GetValidatedBpduType (STP_VERSION_MSTP, bpdu, sizeof(bpdu)));
	}

	TEST_METHOD(validate_bad_bpdu_type)
	{
		static constexpr uint8_t bpdu[36] = { 0, 0, 0, 1 };
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_UNKNOWN, STP_GetValidatedBpduType (STP_VERSION_MSTP, bpdu, sizeof(bpdu)));
	}

	TEST_METHOD(validate_bad_protocol_version_identifier)
	{
		static constexpr uint8_t bpdu[36] = { 0, 0, 1, 2 };
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_UNKNOWN, STP_GetValidatedBpduType (STP_VERSION_MSTP, bpdu, sizeof(bpdu)));
	}

	// ===========================================================================================
	TEST_METHOD(validate_stp_config_bpdu_while_running_mstp)
	{
		auto type = STP_GetValidatedBpduType (STP_VERSION_MSTP, stp_config_bpdu, sizeof(stp_config_bpdu));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_STP_CONFIG, type);
	}

	TEST_METHOD(validate_tcn_bpdu_while_running_mstp)
	{
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_STP_TCN, STP_GetValidatedBpduType (STP_VERSION_MSTP, tcn_bpdu, sizeof(tcn_bpdu)));
	}

	TEST_METHOD(validate_tcn_bpdu_with_padding_while_runing_mstp)
	{
		uint8_t bpdu[50] = { };
		memcpy (bpdu, tcn_bpdu, sizeof(tcn_bpdu));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_STP_TCN, STP_GetValidatedBpduType (STP_VERSION_MSTP, bpdu, sizeof(bpdu)));
	}

	// ===========================================================================================

	TEST_METHOD(validate_rstp_bpdu_while_running_mstp)
	{
		auto type = STP_GetValidatedBpduType (STP_VERSION_MSTP, rstp_bpdu, sizeof(rstp_bpdu));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_RST, type);
	}

	TEST_METHOD(validate_truncated_rstp_bpdu_while_running_mstp)
	{
		uint8_t bpdu [sizeof(rstp_bpdu) - 1];
		memcpy (bpdu, rstp_bpdu, sizeof(bpdu));
		auto type = STP_GetValidatedBpduType (STP_VERSION_MSTP, bpdu, sizeof(bpdu));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_UNKNOWN, type);
	}

	// ===========================================================================================

	TEST_METHOD(test11)
	{
		static constexpr uint8_t bpdu[35] = { 0, 0, 3, 2 };
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_RST, STP_GetValidatedBpduType (STP_VERSION_MSTP, bpdu, sizeof(bpdu)));
	}

	TEST_METHOD(test12)
	{
		static constexpr uint8_t bpdu[35] = { 0, 0, 3, 2 };
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_RST, STP_GetValidatedBpduType (STP_VERSION_MSTP, bpdu, sizeof(bpdu)));
	}

	TEST_METHOD(validate_rstp_bpdu_while_running_legacy_stp)
	{
		// Tests validation of a BPDU received from a bridge that runs RSTP, when we're running LegacySTP.
		auto our_protocol = STP_VERSION_LEGACY_STP;
		auto type = STP_GetValidatedBpduType (our_protocol, rstp_bpdu, sizeof(rstp_bpdu));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_RST, type);
	}

	TEST_METHOD(validate_mstp_bpdu_while_running_legacy_stp)
	{
		// Tests validation of a BPDU received from a bridge that runs MSTP, when we're running LegacySTP.
		auto type = STP_GetValidatedBpduType (STP_VERSION_LEGACY_STP, mstp_bpdu_without_mstis, sizeof(mstp_bpdu_without_mstis));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_RST, type);
		type = STP_GetValidatedBpduType (STP_VERSION_LEGACY_STP, mstp_bpdu_with_mstis, sizeof(mstp_bpdu_with_mstis));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_RST, type);
	}

	TEST_METHOD(validate_mstp_bpdu_while_running_rstp)
	{
		// Tests validation of a BPDU received from a bridge that runs MSTP, when we're running RSTP.
		auto type = STP_GetValidatedBpduType (STP_VERSION_RSTP, mstp_bpdu_without_mstis, sizeof(mstp_bpdu_without_mstis));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_RST, type);
	}

	TEST_METHOD(validate_mstp_bpdu_while_running_mstp)
	{
		// Tests validation of a BPDU received from a bridge that runs MSTP, when we're running MSTP.
		auto type = STP_GetValidatedBpduType (STP_VERSION_MSTP, mstp_bpdu_without_mstis, sizeof(mstp_bpdu_without_mstis));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_MST, type);

		static_assert (sizeof(mstp_bpdu_with_mstis) > 102);
		type = STP_GetValidatedBpduType (STP_VERSION_MSTP, mstp_bpdu_with_mstis, sizeof(mstp_bpdu_with_mstis));
		Assert::AreEqual<int> (VALIDATED_BPDU_TYPE_MST, type);
	}

	TEST_METHOD(BpduWithZeroCistPortRoleIsTreatedAsConfig)
	{
		test_bridge bridge(1, 0, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		STP_StartBridge(bridge, 0);
		STP_OnPortEnabled(bridge, 0, 100, true, 0);

		static constexpr uint8_t bpdu[36] = {
			0, 0, 2, 2, // Id, Version Id, BPDU Type
			0x30, // Flags: Unknown Port Role (00), Learning and Forwarding
			0, 0, 2, 2, 0, 0, 0, 1, // Root Id: 0000.020200000001
			0, 0, 0, 0, // Root Path Cost: 0
			0, 0, 2, 2, 0, 0, 0, 1, // Designated Bridge Id
			0x80, 1, // Designated Port Id
			0, 0, // Message Age
			20, 0, // Max Age: 20 seconds
			2, 0, // Hello Time: 2 seconds
			15, 0, // Forward Delay: 15 seconds
			0 // Version 1 Length
		};
		STP_OnBpduReceived(bridge, 0, bpdu, sizeof(bpdu), 0);

		uint8_t rootPriorityVector[36];
		STP_GetRootPriorityVector(bridge, 0, rootPriorityVector);
		static constexpr uint8_t expectedRootId[8] = { 0, 0, 2, 2, 0, 0, 0, 1 };
		Assert::IsTrue(memcmp(rootPriorityVector, expectedRootId, sizeof(expectedRootId)) == 0);
	}

	TEST_METHOD(LoopbackConfigBpduIgnored)
	{
		test_bridge bridge(1, 0, 16, { 0x10, 0x20, 0x30, 0x40, 0x50, 0x60 });
		STP_SetStpVersion(bridge, STP_VERSION_LEGACY_STP, 0);
		STP_StartBridge(bridge, 0);
		STP_OnPortEnabled(bridge, 0, 100, true, 0);

		// The library should have sent one Config BPDU from the just enabled port.
		auto& txQueue = bridge.tx_queues[0];
		Assert::AreEqual<size_t>(1, txQueue.size());
		auto bpdu = txQueue.front();
		txQueue.pop();

		// Feed it back to the same port.
		STP_OnBpduReceived(bridge, 0, bpdu.data(), (unsigned)bpdu.size(), 0);

		// And wait two seconds to see if the loopback BPDU was processed or not.
		// If it was processed, the algorithm would be disturbed and would not send BPDUs for a while.
		// If it was (correctly) ignored, the algorithm would send BPDUs every two seconds.
		STP_OnOneSecondTick(bridge, 1);
		STP_OnOneSecondTick(bridge, 2);

		Assert::IsFalse(txQueue.empty());
	}
};
