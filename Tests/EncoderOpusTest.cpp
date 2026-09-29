#include "pch.h"
#include "audio/EncoderOpus.h"
#include "audio/AudioUtil.h"

namespace {
	using namespace Audio;
	
	class SampleRates : public testing::TestWithParam<Opus::SampleRate> {
	public:
		void SetUp() override { sampleRate_ = GetParam(); }
	protected:
		Opus::SampleRate sampleRate_ = {};
	};

	TEST_P(SampleRates, CreatesWithValidSampleRate)  {
		EXPECT_NO_THROW(EncoderOpus(Compression::kbps_128, sampleRate_, Opus::Channels::mono));
		EXPECT_NO_THROW(EncoderOpus(Compression::kbps_128, sampleRate_, Opus::Channels::stereo));
	}

	INSTANTIATE_TEST_SUITE_P(EncoderOpusTest, SampleRates, ::testing::Values(
		Opus::SampleRate::khz_8,
		Opus::SampleRate::khz_12,
		Opus::SampleRate::khz_16,
		Opus::SampleRate::khz_24,
		Opus::SampleRate::khz_48
	), [](const testing::TestParamInfo<SampleRates::ParamType>& info) {
		const int value = static_cast<int>(info.param);
		return std::to_string(value);
	});

	class Compressions : public testing::TestWithParam<Compression> {
	public:
		void SetUp() override { compression_ = GetParam(); }
	protected:
		Compression compression_ = {};
	};

	TEST_P(Compressions, CreatesWithValidCompression) {
		EXPECT_NO_THROW(EncoderOpus(compression_, Opus::SampleRate::khz_48, Opus::Channels::mono));
		EXPECT_NO_THROW(EncoderOpus(compression_, Opus::SampleRate::khz_48, Opus::Channels::stereo));
	}

	INSTANTIATE_TEST_SUITE_P(EncoderOpusTest, Compressions, ::testing::Values(
		Compression::kbps_64,
		Compression::kbps_128,
		Compression::kbps_192,
		Compression::kbps_256,
		Compression::kbps_320
		), [](const testing::TestParamInfo<Compressions::ParamType>& info) {
		const int value = static_cast<int>(info.param);
		return std::to_string(value);
	});
}
