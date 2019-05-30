
#include "pch.h"
#include "internal/stp_bpdu.h"
#include "test_helpers.h"

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
};
