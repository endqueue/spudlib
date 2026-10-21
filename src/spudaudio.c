
#include "audio/spudaudiointernal.h"
#include <stddef.h>
#include "spudallocator.h"
#include "spudstructbase.h"

/* Platform-independent SpudAudio helpers. Everything that touches a native
 * audio API lives in src/audio/backends/. */

#if __cplusplus
extern "C" {
#endif

uint32_t spudaudio_sample_format_byte_size(SPUDAUDIO_SAMPLE_FORMAT fmt) {
	switch (fmt) {
	case SPUDAUDIO_SAMPLE_FORMAT_S16:
		return 2;
	case SPUDAUDIO_SAMPLE_FORMAT_S24_PACKED:
		return 3;
	case SPUDAUDIO_SAMPLE_FORMAT_S24_32_MSB:
	case SPUDAUDIO_SAMPLE_FORMAT_S24_32_LSB:
	case SPUDAUDIO_SAMPLE_FORMAT_S32:
	case SPUDAUDIO_SAMPLE_FORMAT_F32:
		return 4;
	default:
		return 0;
	}
}

bool spudaudio_format_has_layout(const SPUDAUDIO_FORMAT *format) {
	return format->positions[0] != SPUDAUDIO_CHANNEL_POSITION_UNSPECIFIED;
}

bool spudaudio_validate_positions(const SPUDAUDIO_FORMAT *format) {
	const uint32_t n = format->channel_count;
	if (!spudaudio_format_has_layout(format)) {
		for (uint32_t i = 1; i < n; ++i)
			if (format->positions[i] != SPUDAUDIO_CHANNEL_POSITION_UNSPECIFIED)
				return false; // partly specified
		return true;
	}
	bool seen[SPUDAUDIO_CHANNEL_POSITION_AUX_LAST + 1] = {false};
	for (uint32_t i = 0; i < n; ++i) {
		SPUDAUDIO_CHANNEL_POSITION p = format->positions[i];
		if (p == SPUDAUDIO_CHANNEL_POSITION_NA)
			continue; // the one position that may repeat
		bool speaker = p >= SPUDAUDIO_CHANNEL_POSITION_FL && p <= SPUDAUDIO_CHANNEL_POSITION_LAST_SPEAKER;
		bool aux     = p >= SPUDAUDIO_CHANNEL_POSITION_AUX0 && p <= SPUDAUDIO_CHANNEL_POSITION_AUX_LAST;
		bool mono    = p == SPUDAUDIO_CHANNEL_POSITION_MONO && n == 1;
		if ((!speaker && !aux && !mono) || seen[p])
			return false;
		seen[p] = true;
	}
	return true;
}

SPUDRESULT spudaudio_validate_instance_desc(const SPUDAUDIO_INSTANCE_DESC *desc, spudaudio_instance *out_instance) {
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_AUDIO_INSTANCE_DESC);
	if (base_sr != SPUD_SUCCESS)
		return base_sr;
	if (!out_instance)
		return SPUDRESULT_NULL_OUTPUT_PARAMETER;
	*out_instance = NULL;
	return spud_allocator_check(desc->allocator);
}

SPUDRESULT spudaudio_validate_stream_desc(const SPUDAUDIO_STREAM_DESC *desc) {
	if (!desc)
		return SPUDRESULT_NULL_DESC;
	const SPUDRESULT base_sr = spud_struct_check(&desc->base, SPUD_STRUCT_TYPE_AUDIO_STREAM_DESC);
	if (base_sr != SPUD_SUCCESS)
		return base_sr;
	if (!desc->device)
		return SPUDRESULT_SAUD_INVALID_DEVICE;
	if (desc->format.channel_count == 0 || desc->format.channel_count > SPUDAUDIO_MAX_CHANNELS ||
	    desc->format.sample_rate == 0 ||
	    spudaudio_sample_format_byte_size(desc->format.sample_format) == 0)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (!spudaudio_validate_positions(&desc->format))
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	if (desc->period_frames == 0)
		return SPUDRESULT_SAUD_PERIOD_OUT_OF_RANGE;
	if (desc->buffer_frames != 0 && desc->buffer_frames < desc->period_frames)
		return SPUDRESULT_SAUD_BUFFER_OUT_OF_RANGE;
	// Which mechanisms exist, and their parameters, are per-backend facts;
	// here only "was one chosen at all".
	if (desc->thread.priority == SPUDAUDIO_THREAD_PRIORITY_UNSPECIFIED || desc->thread.priority > SPUDAUDIO_THREAD_PRIORITY_PLATFORM)
		return SPUDRESULT_DESC_INVALID_PARAMETERS;
	return SPUD_SUCCESS;
}

#if __cplusplus
}
#endif
