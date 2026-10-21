// Tested before any include, so the CMake-defined platforms directly rather
// than spudaudio.h's SPUDAUDIO_PLATFORM_APPLE.
#if SPUDAUDIO_PLATFORM_MACOS || SPUDAUDIO_PLATFORM_IOS || SPUDAUDIO_PLATFORM_WATCHOS

#include "spudaudioapple.h"

#include "../../../spudallocator.h"
#include <string.h>

#if __cplusplus
extern "C" {
#endif

/*
 * AudioChannelLabel -> SpudAudio position.
 *
 * Front, center, LFE and the top labels follow the WAVE speakers Apple
 * documents them against (CoreAudioBaseTypes.h: CenterSurround is WAVE
 * "Back Center", VerticalHeightLeft is WAVE "Top Front Left", ...).
 * Surrounds follow Apple's own WAVE 7.1 layout tag — L R C LFE Rls Rrs
 * Ls Rs against WAVE's FL FR FC LFE BL BR SL SR — so Ls/Rs are the *side*
 * pair and Rls/Rrs the rear pair, matching how WASAPI 5.1/7.1 read.
 *
 * Unused (present but no destination) is NA. Labels with no speaker
 * meaning — Unknown, Discrete_n, matrix Lt/Rt, ambisonic and HOA channels,
 * headphone/binaural, coordinate-described — are unlabeled: AUX.
 */
static SPUDAUDIO_CHANNEL_POSITION apple_label_position(AudioChannelLabel label) {
	switch (label) {
	case kAudioChannelLabel_Unused:
		return SPUDAUDIO_CHANNEL_POSITION_NA;
	case kAudioChannelLabel_Mono:
		return SPUDAUDIO_CHANNEL_POSITION_MONO;
	case kAudioChannelLabel_Left:
		return SPUDAUDIO_CHANNEL_POSITION_FL;
	case kAudioChannelLabel_Right:
		return SPUDAUDIO_CHANNEL_POSITION_FR;
	case kAudioChannelLabel_Center:
		return SPUDAUDIO_CHANNEL_POSITION_FC;
	case kAudioChannelLabel_LFEScreen:
		return SPUDAUDIO_CHANNEL_POSITION_LFE;
	case kAudioChannelLabel_LeftSurround:
	case kAudioChannelLabel_LeftSurroundDirect:
	case kAudioChannelLabel_LeftSideSurround:
		return SPUDAUDIO_CHANNEL_POSITION_SL;
	case kAudioChannelLabel_RightSurround:
	case kAudioChannelLabel_RightSurroundDirect:
	case kAudioChannelLabel_RightSideSurround:
		return SPUDAUDIO_CHANNEL_POSITION_SR;
	case kAudioChannelLabel_LeftCenter:
		return SPUDAUDIO_CHANNEL_POSITION_FLC;
	case kAudioChannelLabel_RightCenter:
		return SPUDAUDIO_CHANNEL_POSITION_FRC;
	case kAudioChannelLabel_CenterSurround:
		return SPUDAUDIO_CHANNEL_POSITION_RC;
	case kAudioChannelLabel_RearSurroundLeft:
		return SPUDAUDIO_CHANNEL_POSITION_RL;
	case kAudioChannelLabel_RearSurroundRight:
		return SPUDAUDIO_CHANNEL_POSITION_RR;
	case kAudioChannelLabel_TopCenterSurround: // = CenterTopMiddle
		return SPUDAUDIO_CHANNEL_POSITION_TC;
	case kAudioChannelLabel_VerticalHeightLeft: // = LeftTopFront
		return SPUDAUDIO_CHANNEL_POSITION_TFL;
	case kAudioChannelLabel_VerticalHeightCenter: // = CenterTopFront
		return SPUDAUDIO_CHANNEL_POSITION_TFC;
	case kAudioChannelLabel_VerticalHeightRight: // = RightTopFront
		return SPUDAUDIO_CHANNEL_POSITION_TFR;
	case kAudioChannelLabel_TopBackLeft:
	case kAudioChannelLabel_LeftTopRear:
		return SPUDAUDIO_CHANNEL_POSITION_TRL;
	case kAudioChannelLabel_TopBackCenter:
	case kAudioChannelLabel_CenterTopRear:
		return SPUDAUDIO_CHANNEL_POSITION_TRC;
	case kAudioChannelLabel_TopBackRight:
	case kAudioChannelLabel_RightTopRear:
		return SPUDAUDIO_CHANNEL_POSITION_TRR;
	case kAudioChannelLabel_LeftTopMiddle:
		return SPUDAUDIO_CHANNEL_POSITION_TSL;
	case kAudioChannelLabel_RightTopMiddle:
		return SPUDAUDIO_CHANNEL_POSITION_TSR;
	case kAudioChannelLabel_LeftWide:
		return SPUDAUDIO_CHANNEL_POSITION_FLW;
	case kAudioChannelLabel_RightWide:
		return SPUDAUDIO_CHANNEL_POSITION_FRW;
	case kAudioChannelLabel_LFE2:
		return SPUDAUDIO_CHANNEL_POSITION_LFE2;
	case kAudioChannelLabel_LeftBottom:
		return SPUDAUDIO_CHANNEL_POSITION_BLC;
	case kAudioChannelLabel_RightBottom:
		return SPUDAUDIO_CHANNEL_POSITION_BRC;
	case kAudioChannelLabel_CenterBottom:
		return SPUDAUDIO_CHANNEL_POSITION_BC;
	default:
		return SPUDAUDIO_CHANNEL_POSITION_UNSPECIFIED; // caller numbers it as AUX
	}
}

// Fills positions from labels, keeping the result a valid layout: a label
// repeated (e.g. both Ls and Lsd, which share SL) keeps its first use and
// the rest become AUX, like every unlabeled channel.
static void apple_labels_to_positions(const AudioChannelLabel *labels, uint32_t n, SPUDAUDIO_FORMAT *out) {
	bool used[SPUDAUDIO_CHANNEL_POSITION_AUX_LAST + 1] = {false};
	uint32_t aux                                      = 0;
	for (uint32_t i = 0; i < n; ++i) {
		SPUDAUDIO_CHANNEL_POSITION p = apple_label_position(labels[i]);
		if (p == SPUDAUDIO_CHANNEL_POSITION_MONO && n != 1)
			p = SPUDAUDIO_CHANNEL_POSITION_UNSPECIFIED;
		if (p != SPUDAUDIO_CHANNEL_POSITION_UNSPECIFIED && p != SPUDAUDIO_CHANNEL_POSITION_NA && used[p])
			p = SPUDAUDIO_CHANNEL_POSITION_UNSPECIFIED;
		if (p == SPUDAUDIO_CHANNEL_POSITION_UNSPECIFIED)
			p = (SPUDAUDIO_CHANNEL_POSITION)(SPUDAUDIO_CHANNEL_POSITION_AUX0 + aux++);
		used[p]           = true;
		out->positions[i] = p;
	}
}

void spudaudio_apple_layout_to_positions(
    const SPUD_ALLOCATOR *allocator,
    const AudioChannelLayout *layout,
    SPUDAUDIO_FORMAT *out) {
	memset(out->positions, 0, sizeof(out->positions));
	const uint32_t n = out->channel_count;
	if (!layout || n == 0 || n > SPUDAUDIO_MAX_CHANNELS)
		return;

	AudioChannelLabel labels[SPUDAUDIO_MAX_CHANNELS];
	if (layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelBitmap) {
		// The bitmap mirrors WAVE's dwChannelMask bit for bit: channel
		// order is ascending bit order, bit b is position FL + b.
		uint32_t i = 0;
		for (uint32_t bit = 0; bit < 18 && i < n; ++bit)
			if (layout->mChannelBitmap & (1U << bit))
				out->positions[i++] = (SPUDAUDIO_CHANNEL_POSITION)(SPUDAUDIO_CHANNEL_POSITION_FL + bit);
		if (i != n)
			memset(out->positions, 0, sizeof(out->positions)); // doesn't describe n channels
		return;
	}

	if (layout->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions) {
		if (layout->mNumberChannelDescriptions != n)
			return;
		for (uint32_t i = 0; i < n; ++i)
			labels[i] = layout->mChannelDescriptions[i].mChannelLabel;
		apple_labels_to_positions(labels, n, out);
		return;
	}

#if SPUDAUDIO_APPLE_HAS_AUDIOTOOLBOX
	// A predefined tag: have AudioToolbox expand it into descriptions.
	AudioChannelLayoutTag tag = layout->mChannelLayoutTag;
	if (AudioChannelLayoutTag_GetNumberOfChannels(tag) != n)
		return;
	UInt32 size = 0;
	if (AudioFormatGetPropertyInfo(kAudioFormatProperty_ChannelLayoutForTag, sizeof(tag), &tag, &size) != noErr || size == 0)
		return;
	// Freed with the size it was allocated with: the call below may lower `size`.
	const UInt32 capacity        = size;
	AudioChannelLayout *expanded = (AudioChannelLayout *)spud_allocate(allocator, capacity, SPUD_ALIGNOF(AudioChannelLayout));
	if (!expanded)
		return;
	if (AudioFormatGetProperty(kAudioFormatProperty_ChannelLayoutForTag, sizeof(tag), &tag, &size, expanded) == noErr &&
	    expanded->mChannelLayoutTag == kAudioChannelLayoutTag_UseChannelDescriptions && expanded->mNumberChannelDescriptions == n) {
		for (uint32_t i = 0; i < n; ++i)
			labels[i] = expanded->mChannelDescriptions[i].mChannelLabel;
		apple_labels_to_positions(labels, n, out);
	}
	spud_free(allocator, expanded, capacity, SPUD_ALIGNOF(AudioChannelLayout));
#else
	(void)allocator;
#endif
}

uint64_t spudaudio_apple_host_time_to_ns(uint64_t host_time, const mach_timebase_info_data_t *timebase) {
	// Split so host_time * numer can't overflow (numer is 125 on Apple
	// silicon, which would overflow after ~4.6 years of uptime otherwise).
	return (host_time / timebase->denom) * timebase->numer + (host_time % timebase->denom) * timebase->numer / timebase->denom;
}

// Frames a buffer actually holds, so a short buffer is never overrun.
static uint32_t apple_buffer_frames(const AudioBuffer *b, uint32_t frames) {
	if (b->mNumberChannels == 0 || !b->mData)
		return 0;
	uint32_t held = b->mDataByteSize / (b->mNumberChannels * (uint32_t)sizeof(float));
	return held < frames ? held : frames;
}

void spudaudio_apple_copy_from_buffer_list(
    const AudioBufferList *list,
    uint32_t frames,
    uint32_t channels,
    float *interleaved,
    float *const *planes) {
	// Silence first: covers channels the list doesn't carry and short buffers.
	if (interleaved)
		memset(interleaved, 0, (size_t)frames * channels * sizeof(float));
	else
		for (uint32_t ch = 0; ch < channels; ++ch)
			memset(planes[ch], 0, (size_t)frames * sizeof(float));
	if (!list)
		return;

	// Fast path: one buffer already in SpudAudio's interleaved layout.
	if (interleaved && list->mNumberBuffers == 1 && list->mBuffers[0].mNumberChannels == channels) {
		memcpy(interleaved, list->mBuffers[0].mData, (size_t)apple_buffer_frames(&list->mBuffers[0], frames) * channels * sizeof(float));
		return;
	}
	uint32_t base = 0; // first SpudAudio channel this buffer carries
	for (UInt32 b = 0; b < list->mNumberBuffers && base < channels; ++b) {
		const AudioBuffer *buffer = &list->mBuffers[b];
		const float *src          = (const float *)buffer->mData;
		const uint32_t nb         = buffer->mNumberChannels;
		const uint32_t held       = apple_buffer_frames(buffer, frames);
		for (uint32_t c = 0; c < nb && base + c < channels; ++c) {
			const uint32_t ch = base + c;
			for (uint32_t f = 0; f < held; ++f) {
				if (interleaved)
					interleaved[(size_t)f * channels + ch] = src[(size_t)f * nb + c];
				else
					planes[ch][f] = src[(size_t)f * nb + c];
			}
		}
		base += nb;
	}
}

void spudaudio_apple_copy_to_buffer_list(
    AudioBufferList *list,
    uint32_t frames,
    uint32_t channels,
    const float *interleaved,
    float *const *planes) {
	if (!list)
		return;
	if (interleaved && list->mNumberBuffers == 1 && list->mBuffers[0].mNumberChannels == channels) {
		uint32_t held = apple_buffer_frames(&list->mBuffers[0], frames);
		memcpy(list->mBuffers[0].mData, interleaved, (size_t)held * channels * sizeof(float));
		return;
	}
	uint32_t base = 0;
	for (UInt32 b = 0; b < list->mNumberBuffers; ++b) {
		AudioBuffer *buffer = &list->mBuffers[b];
		float *dst          = (float *)buffer->mData;
		const uint32_t nb   = buffer->mNumberChannels;
		const uint32_t held = apple_buffer_frames(buffer, frames);
		for (uint32_t c = 0; c < nb; ++c) {
			const uint32_t ch = base + c;
			for (uint32_t f = 0; f < held; ++f) {
				float v = 0.0f; // device channels SpudAudio doesn't carry stay silent
				if (ch < channels)
					v = interleaved ? interleaved[(size_t)f * channels + ch] : planes[ch][f];
				dst[(size_t)f * nb + c] = v;
			}
		}
		base += nb;
	}
}

#if __cplusplus
}
#endif

#else
typedef int spudaudio_apple_not_compiled; // keeps a stray non-Apple build a valid translation unit
#endif // SPUDAUDIO_PLATFORM_MACOS || SPUDAUDIO_PLATFORM_IOS || SPUDAUDIO_PLATFORM_WATCHOS
