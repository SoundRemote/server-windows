#include "pch.h"
#include "common/Util.h"

namespace {
	struct VersionParams {
		std::string current;
		std::string latest;
		int expected;
	};

	class IsNewerVersion : public testing::TestWithParam<VersionParams> {
	public:	
		void SetUp() override { params = GetParam(); }
	protected:
		VersionParams params;
	};

	TEST_P(IsNewerVersion, ComparesVersionsCorrectly) { 
		const int actual = Util::isNewerVersion(params.current, params.latest);
		if (params.expected < 0) {
			// Check that actual is negative, not for an exact match to the params.expected
			EXPECT_TRUE(actual < 0);
		} else {
			EXPECT_EQ(params.expected, actual);
		}
	}

	INSTANTIATE_TEST_SUITE_P(Util, IsNewerVersion, ::testing::Values<VersionParams>(
		// not newer
		VersionParams{ { "1.0.0.0" }, { "1.0.0.0" }, 0 },
		VersionParams{ { "0.2.0.0" }, { "v0.2.0" }, 0 },
		VersionParams{ { "v1.0.8.0" }, { "1.0.8.0" }, 0 },
		VersionParams{ { "0.2.3.0" }, { "Prefix 0.2.3.0" }, 0 },
		VersionParams{ { "1.2.3.4" }, { "1.2.3.4-rc1" }, 0 },
		VersionParams{ { "1.1.0.100" }, { "1.0.99" }, 0 },
		VersionParams{ { "58.2.0.0" }, { "0x58.2.0.0" }, 0 },
		// newer
		VersionParams{ { "0.4.1.0" }, { "0.5.0" }, 1 },
		VersionParams{ { "v0.0.3" }, { "0.0.3.0" }, 1 },
		VersionParams{ { "1.2.3.4-alpha" }, { "1.2.3.4" }, 1 },
		VersionParams{ { "1.2.3" }, { "1.2.3.0-alpha" }, 1 },
		VersionParams{ { "0.0.4.9" }, { "0.0.4.10" }, 1 },
		// invalid
		VersionParams{ { "1.2.3.4" }, { "1.2" }, -1 },
		VersionParams{ { "1.2" }, { "1.2.0" }, -1 },
		VersionParams{ { "0.2.0.0" }, { "1.2..0.0" }, -1 },
		VersionParams{ { "1.a.0.0" }, { "1.2.0.0" }, -1 }
	));
}
