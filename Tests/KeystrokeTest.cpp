#include "pch.h"
#include "domain/Keystroke.h"

using ::testing::HasSubstr;

namespace {
	constexpr int WIN = 1;
	constexpr int CTRL = (1 << 1);
	constexpr int SHIFT = (1 << 2);
	constexpr int ALT = (1 << 3);

	TEST(KeystrokeTest, ToStringNoMods) {
		const Keystroke keystroke(0x57, 0);
		const std::wstring expected = L"W";

		const std::wstring actual = keystroke.toString();

		EXPECT_EQ(actual, expected);
	}

	TEST(KeystrokeTest, ToStringAllMods) {
		const Keystroke keystroke(0x57, WIN | CTRL | SHIFT | ALT);
		const std::wstring expectedContain[] = {L"Win", L"Ctrl", L"Shift", L"Alt"};

		const std::wstring actual = keystroke.toString();

		for (auto&& modStr: expectedContain) {
			EXPECT_THAT(actual, HasSubstr(modStr));
		}
	}
}
