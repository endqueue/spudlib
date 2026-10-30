/*
 * spud_noalloc_tests — no allocation per frame (ROADMAP.md in the workspace,
 * section 3.1, "Data-oriented checks").
 *
 * Each module's hot path, warmed up, then counted for 100 iterations with
 * every heap allocation the counting thread makes, through tests/alloctrace.h,
 * which says what each platform's hook sees and how each allocation is put
 * down to the code that asked for it. Only ours (this executable's, where
 * SpudLib is linked) must be none; what a driver, the graphics runtime or the
 * system asks for is reported apart and not counted. Where no hook is
 * available the test says so and passes nothing off as checked.
 *
 * The hot paths:
 * - SpudGPU recording and submitting a frame offscreen on a real device:
 *   reset the allocator, begin, a barrier, a pass that clears an image, a
 *   barrier out, end, spudgpu_queue_submit() signalling a fence, and a wait
 *   for the frame before. On every platform with an adapter.
 * - SpudGPU's submit and present loop: the same frame drawn into a swap
 *   chain's image, acquired, submitted with
 *   spudgpu_submit_command_lists_synced() and presented. Windows presents to
 *   a hidden window, macOS to a CAMetalLayer with no window; Linux has no
 *   surface here without a display, so it is skipped there.
 * - SpudAudio's stream thread: an output stream on the general default
 *   device writing silence, counted on the callback's own thread from its
 *   Nth call to its (N+100)th, everything SpudAudio does on that thread
 *   between the calls included. Skipped with no output device.
 * - SpudNet with a loopback connection open and idle: a receive with
 *   SPUDNET_NO_WAIT on each end and a wait set's poll, per round.
 * - SpudInput's poll, on the platform's native API, gamepads or none.
 *
 * Each scenario also times its iterations, uncounted, and prints the best
 * of a few runs: what a change to its path costs or saves.
 *
 *   spud_noalloc_tests [--adapter N]
 *
 * --adapter picks the GPU, by its index in spudgpu_enumerate_adapters(); 0 by
 * default. Exits 0 when every check passes, 1 when one fails.
 */

#include <spudaudio.h>
#include <spudcore.h>
#include <spudgpu.h>
#include <spudinput.h>
#include <spudnet.h>
#include <spudperf.h>

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloctrace.h"

#if SPUDGPU_COMPILE_METAL_API
#include "spudgpu_test_metal_layer.h"
#endif

#if defined(_WIN32)
#include <windows.h>
#else
#include <time.h>
#endif

static int failures = 0;

#define CHECK(cond)                                                              \
	do {                                                                         \
		if (!(cond)) {                                                           \
			fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
			++failures;                                                          \
		}                                                                        \
	} while (0)

#define ITERATIONS 100
#define WARM_UP 8
/* The timed runs: how many, and iterations in each. */
#define TIMED_RUNS 5
#define TIMED_ITERATIONS 200

static void sleep_ms(uint32_t ms) {
#if defined(_WIN32)
	Sleep(ms);
#else
	struct timespec t = {(time_t)(ms / 1000), (long)(ms % 1000) * 1000000L};
	nanosleep(&t, NULL);
#endif
}

/* Ours must be none; what other modules asked for is reported, not counted. */
static void report(const char *what) {
	char line[256];
	snprintf(line, sizeof(line), "spud_noalloc_tests: %s, %d iterations", what, ITERATIONS);
	alloc_trace_report(line, 6);
	if (alloc_trace_ours() != 0)
		++failures;
}

static void print_time(const char *what, uint64_t best_ns, uint32_t iterations) {
	printf("spud_noalloc_tests: time: %s: %.2f us an iteration (best of %d runs of %u)\n", what,
	       (double)best_ns / (double)iterations / 1000.0, TIMED_RUNS, iterations);
}

/* The hook counts at all: one malloc and one calloc of this thread, counted
 * as two of ours. A zero everywhere else means nothing if this is zero too. */
static void test_hook_counts(void) {
	alloc_trace_start();
	void *volatile a = malloc(24);
	void *volatile b = calloc(3, 8);
	alloc_trace_stop();
	free(a);
	free(b);
	printf("spud_noalloc_tests: the hook counted %lu of 2 known allocations of ours\n", alloc_trace_ours());
	CHECK(alloc_trace_ours() == 2);
}

// ---- SpudGPU ---------------------------------------------------------------

#define TARGET_SIZE 256

typedef struct gpu_frame {
	spudgpu_device device;
	spudgpu_command_queue queue;
	spudgpu_command_allocator allocator;
	spudgpu_command_list cmd;
} gpu_frame;

/* The commands of one frame: the target into a colour attachment, a pass
 * that clears it, and the target out to [after]. */
static void record_frame(spudgpu_command_list cmd, spudgpu_image_view view, uint32_t width, uint32_t height, SPUDGPU_IMAGE_LAYOUT before,
                         SPUDGPU_IMAGE_LAYOUT after, uint64_t frame) {
	spudgpu_begin_command_list(cmd);
	spudgpu_cmd_image_barrier_view(cmd, view, before, SPUDGPU_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	spudgpu_rendering_begin_desc pass;
	memset(&pass, 0, sizeof(pass));
	pass.base.type                       = SPUD_STRUCT_TYPE_GPU_RENDERING_BEGIN_DESC;
	pass.color_attachments[0].image_view = view;
	pass.color_attachments[0].load_op    = SPUDGPU_LOAD_OP_CLEAR;
	pass.color_attachments[0].store_op   = SPUDGPU_STORE_OP_STORE;
	pass.color_attachments[0].clear_color[0] = (float)(frame % 16) / 16.0f;
	pass.color_attachments[0].clear_color[3] = 1.0f;
	pass.color_attachment_count          = 1;
	pass.width                           = width;
	pass.height                          = height;
	spudgpu_cmd_begin_rendering(cmd, &pass);
	spudgpu_cmd_end_rendering(cmd);
	spudgpu_cmd_image_barrier_view(cmd, view, SPUDGPU_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, after);
	spudgpu_end_command_list(cmd);
}

/* Offscreen: one frame, submitted with a fence signalled to [frame]; the
 * frame before waited for first, so the one allocator can be reset. */
typedef struct offscreen {
	gpu_frame f;
	spudgpu_image image;
	spudgpu_image_view view;
	spudgpu_fence fence;
	uint64_t frame;
	/* Of the timed runs: the time in spudgpu_queue_submit() alone. */
	uint64_t submit_ns;
} offscreen;

static bool offscreen_frame(offscreen *o) {
	if (o->frame > 0) {
		uint64_t value = o->frame;
		if (spudgpu_wait_for_fences(o->f.device, &o->fence, &value, 1, true, UINT64_MAX) != SPUD_SUCCESS)
			return false;
	}
	if (spudgpu_reset_command_allocator(o->f.allocator) != SPUD_SUCCESS)
		return false;
	record_frame(o->f.cmd, o->view, TARGET_SIZE, TARGET_SIZE,
	             o->frame == 0 ? SPUDGPU_IMAGE_LAYOUT_UNDEFINED : SPUDGPU_IMAGE_LAYOUT_SHADER_READ_ONLY,
	             SPUDGPU_IMAGE_LAYOUT_SHADER_READ_ONLY, o->frame);
	++o->frame;
	spudgpu_submit_desc submit;
	memset(&submit, 0, sizeof(submit));
	submit.base.type          = SPUD_STRUCT_TYPE_GPU_SUBMIT_DESC;
	submit.cmd_lists          = &o->f.cmd;
	submit.cmd_list_count     = 1;
	submit.signal_fence       = o->fence;
	submit.signal_fence_value = o->frame;
	const uint64_t start       = spudperf_get_monotonic_time_ns();
	const SPUDRESULT submitted = spudgpu_queue_submit(o->f.queue, &submit);
	o->submit_ns += spudperf_get_monotonic_time_ns() - start;
	return submitted == SPUD_SUCCESS;
}

static void test_gpu_offscreen(gpu_frame f) {
	offscreen o;
	memset(&o, 0, sizeof(o));
	o.f = f;
	spudgpu_image_desc image_desc;
	memset(&image_desc, 0, sizeof(image_desc));
	image_desc.base.type    = SPUD_STRUCT_TYPE_GPU_IMAGE_DESC;
	image_desc.usage        = SPUDGPU_IMAGE_USAGE_COLOR_ATTACHMENT | SPUDGPU_IMAGE_USAGE_SAMPLED;
	image_desc.type         = SPUDGPU_IMAGE_TYPE_2D;
	image_desc.format       = SPUDGPU_FORMAT_R8G8B8A8_UNORM;
	image_desc.clear_value.format = SPUDGPU_FORMAT_R8G8B8A8_UNORM;
	image_desc.width        = TARGET_SIZE;
	image_desc.height       = TARGET_SIZE;
	image_desc.depth        = 1;
	image_desc.array_layers = 1;
	image_desc.mip_levels   = 1;
	image_desc.debug_name   = "noalloc target";
	CHECK(spudgpu_create_image(f.device, &image_desc, &o.image) == SPUD_SUCCESS);
	spudgpu_image_view_desc view_desc;
	memset(&view_desc, 0, sizeof(view_desc));
	view_desc.base.type                           = SPUD_STRUCT_TYPE_GPU_IMAGE_VIEW_DESC;
	view_desc.parent_image                        = o.image;
	view_desc.type                                = SPUDGPU_IMAGE_VIEW_TYPE_2D;
	view_desc.subresource_range.aspect_mask       = SPUDGPU_IMAGE_ASPECT_COLOR;
	view_desc.subresource_range.mip_level_count   = 1;
	view_desc.subresource_range.array_layer_count = 1;
	if (o.image)
		CHECK(spudgpu_create_image_view(o.image, &view_desc, &o.view) == SPUD_SUCCESS);
	CHECK(spudgpu_create_fence(f.device, SPUDGPU_FENCE_FLAG_NONE, 0, &o.fence) == SPUD_SUCCESS);
	if (o.view && o.fence) {
		bool ok = true;
		for (int i = 0; i < WARM_UP && ok; ++i)
			ok = offscreen_frame(&o);
		CHECK(ok);
		alloc_trace_start();
		for (int i = 0; i < ITERATIONS && ok; ++i)
			ok = offscreen_frame(&o);
		alloc_trace_stop();
		CHECK(ok);
		report("SpudGPU recording and submitting a frame offscreen, a fence waited on");

		uint64_t best = UINT64_MAX, best_submit = UINT64_MAX;
		for (int run = 0; run < TIMED_RUNS && ok; ++run) {
			o.submit_ns          = 0;
			const uint64_t start = spudperf_get_monotonic_time_ns();
			for (int i = 0; i < TIMED_ITERATIONS && ok; ++i)
				ok = offscreen_frame(&o);
			const uint64_t took = spudperf_get_monotonic_time_ns() - start;
			if (took < best)
				best = took;
			if (o.submit_ns < best_submit)
				best_submit = o.submit_ns;
		}
		if (ok) {
			print_time("SpudGPU offscreen frame (recording, submit, fence wait)", best, TIMED_ITERATIONS);
			print_time("SpudGPU offscreen frame, spudgpu_queue_submit() alone", best_submit, TIMED_ITERATIONS);
		}
		uint64_t value = o.frame;
		spudgpu_wait_for_fences(f.device, &o.fence, &value, 1, true, UINT64_MAX);
	}
	spudgpu_destroy_fence(o.fence);
	spudgpu_destroy_image_view(o.view);
	spudgpu_destroy_image(o.image);
}

#if defined(_WIN32) || SPUDGPU_COMPILE_METAL_API
#define NOALLOC_PRESENT 1

typedef struct presenting {
	gpu_frame f;
	spudgpu_swap_chain swap_chain;
	uint32_t width, height;
	uint64_t frame;
	/* Of the timed runs: recording and the synced submit only, no acquire
	 * and no present, which wait; and the synced submit alone. */
	uint64_t recording_and_submit_ns;
	uint64_t submit_ns;
} presenting;

static bool present_frame(presenting *p) {
	const uint32_t index    = spudgpu_swap_chain_acquire_next_image(p->swap_chain);
	spudgpu_image_view view = spudgpu_get_swap_chain_image_view(p->swap_chain, index);
	if (!view)
		return false;
	const uint64_t start = spudperf_get_monotonic_time_ns();
	if (spudgpu_reset_command_allocator(p->f.allocator) != SPUD_SUCCESS)
		return false;
	record_frame(p->f.cmd, view, p->width, p->height, SPUDGPU_IMAGE_LAYOUT_UNDEFINED, SPUDGPU_IMAGE_LAYOUT_PRESENT_SRC, p->frame++);
	const uint64_t submit_start = spudperf_get_monotonic_time_ns();
	const SPUDRESULT submitted  = spudgpu_submit_command_lists_synced(p->f.queue, &p->f.cmd, 1, p->swap_chain);
	const uint64_t end          = spudperf_get_monotonic_time_ns();
	p->recording_and_submit_ns += end - start;
	p->submit_ns += end - submit_start;
	if (submitted != SPUD_SUCCESS)
		return false;
	spudgpu_swap_chain_present(p->swap_chain);
	return true;
}

static void test_gpu_present(spudgpu_instance instance, gpu_frame f, void *window) {
	presenting p;
	memset(&p, 0, sizeof(p));
	p.f                     = f;
	spudgpu_surface surface = NULL;
	CHECK(spudgpu_create_surface(instance, window, NULL, &surface) == SPUD_SUCCESS);
	if (!surface)
		return;
	spudgpu_swap_chain_desc desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type = SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC;
	desc.surface   = surface;
	desc.queue     = f.queue;
	desc.width     = TARGET_SIZE;
	desc.height    = TARGET_SIZE;
#if SPUDGPU_COMPILE_METAL_API
	desc.buffer_count = 1;
#else
	desc.buffer_count = 2;
#endif
	desc.format          = SPUDGPU_FORMAT_B8G8R8A8_UNORM;
	desc.present_mode    = SPUDGPU_PRESENT_MODE_FIFO;
	desc.present_scaling = SPUDGPU_PRESENT_SCALING_ONE_TO_ONE;
	desc.fullscreen_mode = SPUDGPU_FULLSCREEN_MODE_WINDOWED;
	CHECK(spudgpu_create_swap_chain(f.device, &desc, &p.swap_chain) == SPUD_SUCCESS);
	if (p.swap_chain) {
		spudgpu_swap_chain_desc created;
		memset(&created, 0, sizeof(created));
		created.base.type = SPUD_STRUCT_TYPE_GPU_SWAP_CHAIN_DESC;
		CHECK(spudgpu_get_swap_chain_desc(p.swap_chain, &created) == SPUD_SUCCESS);
		p.width  = created.width;
		p.height = created.height;
		bool ok  = true;
		for (int i = 0; i < WARM_UP && ok; ++i)
			ok = present_frame(&p);
		CHECK(ok);
		alloc_trace_start();
		for (int i = 0; i < ITERATIONS && ok; ++i)
			ok = present_frame(&p);
		alloc_trace_stop();
		CHECK(ok);
		report("SpudGPU's frame into a swap chain: acquire, record, synced submit, present");

		uint64_t best = UINT64_MAX, best_recording = UINT64_MAX, best_submit = UINT64_MAX;
		for (int run = 0; run < TIMED_RUNS && ok; ++run) {
			p.recording_and_submit_ns = 0;
			p.submit_ns               = 0;
			const uint64_t start      = spudperf_get_monotonic_time_ns();
			for (int i = 0; i < ITERATIONS && ok; ++i)
				ok = present_frame(&p);
			const uint64_t took = spudperf_get_monotonic_time_ns() - start;
			if (took < best)
				best = took;
			if (p.recording_and_submit_ns < best_recording)
				best_recording = p.recording_and_submit_ns;
			if (p.submit_ns < best_submit)
				best_submit = p.submit_ns;
		}
		if (ok) {
			print_time("SpudGPU present frame, whole (FIFO: paced by the display)", best, ITERATIONS);
			print_time("SpudGPU present frame, reset + recording + synced submit only", best_recording, ITERATIONS);
			print_time("SpudGPU present frame, spudgpu_submit_command_lists_synced() alone", best_submit, ITERATIONS);
		}
		spudgpu_queue_wait_idle(f.queue);
		spudgpu_destroy_swap_chain(p.swap_chain);
	}
	spudgpu_destroy_surface(surface);
}
#else
#define NOALLOC_PRESENT 0
#endif

#if defined(_WIN32)
static LRESULT CALLBACK window_proc(HWND window, UINT message, WPARAM w, LPARAM l) {
	return DefWindowProcW(window, message, w, l);
}

/* A window that is never shown: a surface needs one, nobody needs to see it. */
static HWND hidden_window_create(void) {
	WNDCLASSEXW window_class;
	memset(&window_class, 0, sizeof(window_class));
	window_class.cbSize        = sizeof(window_class);
	window_class.lpfnWndProc   = window_proc;
	window_class.hInstance     = GetModuleHandleW(NULL);
	window_class.lpszClassName = L"spud_noalloc_tests";
	RegisterClassExW(&window_class);
	RECT rect = {0, 0, TARGET_SIZE, TARGET_SIZE};
	AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
	return CreateWindowExW(0, L"spud_noalloc_tests", L"spud_noalloc_tests", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
	                       rect.right - rect.left, rect.bottom - rect.top, NULL, NULL, GetModuleHandleW(NULL), NULL);
}

static void hidden_window_destroy(HWND window) {
	DestroyWindow(window);
	MSG message;
	while (PeekMessageW(&message, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&message);
		DispatchMessageW(&message);
	}
	UnregisterClassW(L"spud_noalloc_tests", GetModuleHandleW(NULL));
}
#endif

static void test_gpu(uint32_t adapter_index) {
	const SPUD_ALLOCATOR allocator = spud_allocator_system();
	spudgpu_instance_desc instance_desc;
	memset(&instance_desc, 0, sizeof(instance_desc));
	instance_desc.base.type        = SPUD_STRUCT_TYPE_GPU_INSTANCE_DESC;
	instance_desc.allocator        = &allocator;
	instance_desc.application_name = "spud_noalloc_tests";
	instance_desc.engine_name      = "spud_noalloc_tests";
	spudgpu_instance instance      = NULL;
	if (spudgpu_create_instance(&instance_desc, &instance) != SPUD_SUCCESS) {
		printf("spud_noalloc_tests: SpudGPU: no instance, skipped\n");
		return;
	}
	const spudgpu_adapter *adapters = NULL;
	uint32_t adapter_count          = 0;
	spudgpu_enumerate_adapters(instance, &adapters, &adapter_count);
	if (adapter_index >= adapter_count) {
		printf("spud_noalloc_tests: SpudGPU: no adapter %u (%u listed), skipped\n", adapter_index, adapter_count);
		spudgpu_destroy_instance(instance);
		return;
	}
	SPUDGPU_ADAPTER_PROPERTIES properties;
	memset(&properties, 0, sizeof(properties));
	properties.base.type = SPUD_STRUCT_TYPE_GPU_ADAPTER_PROPERTIES;
	CHECK(spudgpu_get_adapter_properties(adapters[adapter_index], &properties) == SPUD_SUCCESS);
	printf("spud_noalloc_tests: SpudGPU on adapter %u: %s\n", adapter_index, properties.description);

	const bool can_present = NOALLOC_PRESENT && (properties.supported_features & SPUDGPU_FEATURE_PRESENT) != 0;
	spudgpu_device_desc device_desc;
	memset(&device_desc, 0, sizeof(device_desc));
	device_desc.base.type          = SPUD_STRUCT_TYPE_GPU_DEVICE_DESC;
	device_desc.required_features  = can_present ? SPUDGPU_FEATURE_PRESENT : 0;
	device_desc.direct_queue_count = 1;
	device_desc.max_descriptors    = 16;
	device_desc.max_samplers       = 4;
	device_desc.debug_name         = "spud_noalloc_tests";
	gpu_frame f;
	memset(&f, 0, sizeof(f));
	CHECK(spudgpu_create_device(adapters[adapter_index], &device_desc, &f.device) == SPUD_SUCCESS);
	if (f.device) {
		CHECK(spudgpu_get_command_queue(f.device, SPUDGPU_COMMAND_LIST_TYPE_DIRECT, 0, &f.queue) == SPUD_SUCCESS);
		spudgpu_command_allocator_desc allocator_desc;
		memset(&allocator_desc, 0, sizeof(allocator_desc));
		allocator_desc.base.type = SPUD_STRUCT_TYPE_GPU_COMMAND_ALLOCATOR_DESC;
		allocator_desc.type      = SPUDGPU_COMMAND_LIST_TYPE_DIRECT;
		CHECK(spudgpu_create_command_allocator(f.device, &allocator_desc, &f.allocator) == SPUD_SUCCESS);
		if (f.allocator)
			CHECK(spudgpu_create_command_list(f.allocator, &f.cmd) == SPUD_SUCCESS);
		if (f.queue && f.cmd) {
			test_gpu_offscreen(f);
#if NOALLOC_PRESENT
			if (!can_present) {
				printf("spud_noalloc_tests: SpudGPU: the adapter can't present, the present loop skipped\n");
			} else {
#if defined(_WIN32)
				HWND window = hidden_window_create();
				CHECK(window != NULL);
				if (window) {
					test_gpu_present(instance, f, window);
					hidden_window_destroy(window);
				}
#else
				void *layer = spudgpu_test_metal_layer_create(false);
				test_gpu_present(instance, f, layer);
				spudgpu_test_metal_layer_destroy(layer);
#endif
			}
#else
			printf("spud_noalloc_tests: SpudGPU: no surface without a display here, the present loop skipped\n");
#endif
			spudgpu_queue_wait_idle(f.queue);
		}
		spudgpu_destroy_command_list(f.cmd);
		spudgpu_destroy_command_allocator(f.allocator);
		spudgpu_destroy_device(f.device);
	}
	spudgpu_destroy_instance(instance);
}

// ---- SpudAudio -------------------------------------------------------------

/* The first counted call, and how many are counted. */
#define AUDIO_FIRST_COUNTED 20

typedef struct silence {
	uint32_t channels;
	uint32_t sample_bytes;
	bool interleaved;
	/* Written only by the callback's thread; read by main after the stream
	 * has stopped. */
	volatile uint32_t calls;
	volatile bool counted;
	/* Of the callbacks after the counted ones: the time from one call to
	 * the next is the device's period, so what is timed is the callback's
	 * own share: the time SpudAudio's thread spends from the start of one
	 * call to the start of the next, less the time it waits, is not
	 * visible here. The callback's own time is. */
} silence;

static void silence_callback(spudaudio_stream stream, void *frames, uint32_t frame_count, const SPUDAUDIO_CALLBACK_INFO *info, void *user_data) {
	(void)stream;
	(void)info;
	silence *s           = (silence *)user_data;
	const uint32_t calls = s->calls;
	if (calls == AUDIO_FIRST_COUNTED)
		alloc_trace_start();
	if (calls == AUDIO_FIRST_COUNTED + ITERATIONS) {
		alloc_trace_end();
		s->counted = true;
	}
	if (s->interleaved) {
		memset(frames, 0, (size_t)frame_count * s->channels * s->sample_bytes);
	} else {
		for (uint32_t c = 0; c < s->channels; ++c)
			memset(((void **)frames)[c], 0, (size_t)frame_count * s->sample_bytes);
	}
	s->calls = calls + 1;
}

static void test_audio(void) {
#if SPUDAUDIO_COMPILE_COREAUDIO
	const SPUDAUDIO_NATIVE_API api = SPUDAUDIO_NATIVE_API_COREAUDIO;
#elif SPUDAUDIO_COMPILE_WASAPI
	const SPUDAUDIO_NATIVE_API api = SPUDAUDIO_NATIVE_API_WASAPI;
#elif SPUDAUDIO_COMPILE_ALSA
	const SPUDAUDIO_NATIVE_API api = SPUDAUDIO_NATIVE_API_ALSA;
#else
	const SPUDAUDIO_NATIVE_API api = SPUDAUDIO_NATIVE_API_NONE;
#endif
	if (api == SPUDAUDIO_NATIVE_API_NONE) {
		printf("spud_noalloc_tests: SpudAudio: no native API in this build, skipped\n");
		return;
	}
	const SPUD_ALLOCATOR allocator = spud_allocator_system();
	SPUDAUDIO_INSTANCE_DESC instance_desc;
	memset(&instance_desc, 0, sizeof(instance_desc));
	instance_desc.base.type     = SPUD_STRUCT_TYPE_AUDIO_INSTANCE_DESC;
	instance_desc.allocator     = &allocator;
	instance_desc.native_api    = api;
	spudaudio_instance instance = NULL;
	if (spudaudio_create_instance(&instance_desc, &instance) != SPUD_SUCCESS) {
		printf("spud_noalloc_tests: SpudAudio: no instance, skipped\n");
		return;
	}
	spudaudio_device *devices = NULL;
	uint32_t device_count     = 0;
	spudaudio_enumerate_devices(instance, SPUDAUDIO_DIRECTION_OUTPUT, &devices, &device_count);
	SPUDAUDIO_DEVICE_PROPERTIES props;
	spudaudio_device device = NULL;
	for (uint32_t i = 0; i < device_count && !device; ++i) {
		memset(&props, 0, sizeof(props));
		props.base.type = SPUD_STRUCT_TYPE_AUDIO_DEVICE_PROPERTIES;
		if (spudaudio_get_device_properties(devices[i], &props) == SPUD_SUCCESS && (props.default_roles & SPUDAUDIO_DEFAULT_ROLE_GENERAL))
			device = devices[i];
	}
	if (!device && device_count > 0) {
		device = devices[0];
		memset(&props, 0, sizeof(props));
		props.base.type = SPUD_STRUCT_TYPE_AUDIO_DEVICE_PROPERTIES;
		spudaudio_get_device_properties(device, &props);
	}
	if (!device) {
		printf("spud_noalloc_tests: SpudAudio: no output device, skipped\n");
		spudaudio_destroy_instance(instance);
		return;
	}

	SPUDAUDIO_STREAM_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type = SPUD_STRUCT_TYPE_AUDIO_STREAM_DESC;
	desc.device    = device;
	if (props.native_format.sample_format != SPUDAUDIO_SAMPLE_FORMAT_UNKNOWN) {
		desc.format = props.native_format;
	} else {
		desc.format.sample_format = SPUDAUDIO_SAMPLE_FORMAT_F32;
		desc.format.sample_rate   = 48000;
		desc.format.channel_count = 2;
		desc.format.interleaved   = true;
	}
	desc.flags           = SPUDAUDIO_STREAM_FLAG_NONE;
	desc.thread.priority = SPUDAUDIO_THREAD_PRIORITY_NORMAL;
	desc.callback        = silence_callback;
	SPUDAUDIO_TIMING_CAPS caps;
	memset(&caps, 0, sizeof(caps));
	caps.base.type = SPUD_STRUCT_TYPE_AUDIO_TIMING_CAPS;
	if (spudaudio_get_timing_caps(device, &desc.format, desc.flags, &caps) == SPUD_SUCCESS) {
		desc.period_frames = caps.default_period_frames ? caps.default_period_frames : caps.min_period_frames;
		if (caps.max_buffer_frames) {
			desc.buffer_frames = desc.period_frames * 2;
			if (desc.buffer_frames < caps.min_buffer_frames)
				desc.buffer_frames = caps.min_buffer_frames;
			if (desc.buffer_frames > caps.max_buffer_frames)
				desc.buffer_frames = caps.max_buffer_frames;
		}
	} else {
		desc.period_frames = desc.format.sample_rate / 100;
		desc.buffer_frames = api == SPUDAUDIO_NATIVE_API_ALSA ? desc.period_frames * 2 : 0;
	}
	/* Each rejected field takes the probe's suggestion, a few rounds. */
	SPUDAUDIO_STREAM_CONFIG config;
	bool accepted = false;
	for (int round = 0; round < 8 && !accepted; ++round) {
		memset(&config, 0, sizeof(config));
		config.base.type   = SPUD_STRUCT_TYPE_AUDIO_STREAM_CONFIG;
		const SPUDRESULT r = spudaudio_probe_stream(&desc, &config);
		if (r == SPUD_SUCCESS)
			accepted = true;
		else if (r == SPUDRESULT_SAUD_FORMAT_NOT_SUPPORTED && config.format.sample_format != SPUDAUDIO_SAMPLE_FORMAT_UNKNOWN)
			desc.format = config.format;
		else if (r == SPUDRESULT_SAUD_PERIOD_OUT_OF_RANGE && config.period_frames)
			desc.period_frames = config.period_frames;
		else if (r == SPUDRESULT_SAUD_BUFFER_OUT_OF_RANGE)
			desc.buffer_frames = config.buffer_frames;
		else
			break;
	}
	if (!accepted) {
		printf("spud_noalloc_tests: SpudAudio: the device takes no stream this test asks for, skipped\n");
		spudaudio_destroy_instance(instance);
		return;
	}
	silence s;
	memset(&s, 0, sizeof(s));
	desc.user_data          = &s;
	spudaudio_stream stream = NULL;
	CHECK(spudaudio_create_stream(&desc, &stream) == SPUD_SUCCESS);
	if (stream) {
		memset(&config, 0, sizeof(config));
		config.base.type = SPUD_STRUCT_TYPE_AUDIO_STREAM_CONFIG;
		CHECK(spudaudio_stream_get_config(stream, &config) == SPUD_SUCCESS);
		s.channels     = config.format.channel_count;
		s.sample_bytes = spudaudio_sample_format_byte_size(config.format.sample_format);
		s.interleaved  = config.format.interleaved;
		CHECK(spudaudio_stream_start(stream) == SPUD_SUCCESS);
		/* The counted calls are a little over a second at a 10 ms period;
		 * ten seconds is plenty for any period a device takes. */
		for (int waited = 0; waited < 10000 && !s.counted; waited += 10)
			sleep_ms(10);
		CHECK(spudaudio_stream_stop(stream) == SPUD_SUCCESS);
		CHECK(s.counted);
		if (s.counted) {
			alloc_trace_attribute();
			char what[160];
			snprintf(what, sizeof(what), "SpudAudio's stream thread writing silence, %u-frame period, callbacks %d to %d",
			         config.period_frames, AUDIO_FIRST_COUNTED, AUDIO_FIRST_COUNTED + ITERATIONS);
			report(what);
		}
		spudaudio_destroy_stream(stream);
	}
	CHECK(spudaudio_destroy_instance(instance) == SPUD_SUCCESS);
}

// ---- SpudNet ---------------------------------------------------------------

typedef struct idle_connection {
	spudnet_tcp_socket client, server;
	spudnet_wait_set set;
} idle_connection;

static bool net_round(const idle_connection *c) {
	uint8_t buffer[64];
	uint64_t received = 0;
	spudnet_wait_event events[4];
	uint32_t count = 0;
	return spudnet_tcp_socket_recv(c->client, buffer, sizeof(buffer), SPUDNET_NO_WAIT, &received) == SPUDRESULT_SPUDNET_TIMED_OUT &&
	       spudnet_tcp_socket_recv(c->server, buffer, sizeof(buffer), SPUDNET_NO_WAIT, &received) == SPUDRESULT_SPUDNET_TIMED_OUT &&
	       spudnet_wait_set_wait(c->set, SPUDNET_NO_WAIT, events, 4, &count) == SPUDRESULT_SPUDNET_TIMED_OUT;
}

static void test_net(void) {
	const SPUD_ALLOCATOR allocator = spud_allocator_system();
	spudnet_instance_desc instance_desc;
	memset(&instance_desc, 0, sizeof(instance_desc));
	instance_desc.base.type   = SPUD_STRUCT_TYPE_NET_INSTANCE_DESC;
	instance_desc.allocator   = &allocator;
	spudnet_instance instance = NULL;
	if (spudnet_instance_create(&instance_desc, &instance, NULL) != SPUD_SUCCESS) {
		printf("spud_noalloc_tests: SpudNet: no instance, skipped\n");
		return;
	}
	spudnet_tcp_listener_desc listen_desc;
	memset(&listen_desc, 0, sizeof(listen_desc));
	listen_desc.base.type         = SPUD_STRUCT_TYPE_NET_TCP_LISTENER_DESC;
	listen_desc.address           = "127.0.0.1";
	listen_desc.backlog           = 1;
	spudnet_tcp_listener listener = NULL;
	if (spudnet_tcp_listener_create(instance, &listen_desc, &listener, NULL) != SPUD_SUCCESS) {
		printf("spud_noalloc_tests: SpudNet: no loopback listener, skipped\n");
		spudnet_instance_destroy(instance);
		return;
	}
	uint16_t port = 0;
	CHECK(spudnet_tcp_listener_get_port(listener, &port) == SPUD_SUCCESS);
	idle_connection c;
	memset(&c, 0, sizeof(c));
	CHECK(spudnet_tcp_socket_create(instance, &c.client) == SPUD_SUCCESS);
	spudnet_tcp_socket_connect_desc connect_desc;
	memset(&connect_desc, 0, sizeof(connect_desc));
	connect_desc.base.type  = SPUD_STRUCT_TYPE_NET_TCP_SOCKET_CONNECT_DESC;
	connect_desc.address    = "127.0.0.1";
	connect_desc.port       = port;
	connect_desc.timeout_ms = 5000;
	if (c.client)
		CHECK(spudnet_tcp_socket_connect(c.client, &connect_desc) == SPUD_SUCCESS);
	CHECK(spudnet_tcp_listener_accept(listener, 5000, &c.server) == SPUD_SUCCESS);
	CHECK(spudnet_wait_set_create(instance, &c.set, NULL) == SPUD_SUCCESS);
	static int client_tag, server_tag;
	if (c.client && c.server && c.set) {
		CHECK(spudnet_wait_set_add_tcp_socket(c.set, c.client, SPUDNET_WAIT_FOR_INCOMING, &client_tag) == SPUD_SUCCESS);
		CHECK(spudnet_wait_set_add_tcp_socket(c.set, c.server, SPUDNET_WAIT_FOR_INCOMING, &server_tag) == SPUD_SUCCESS);
		bool ok = true;
		for (int i = 0; i < WARM_UP && ok; ++i)
			ok = net_round(&c);
		CHECK(ok);
		alloc_trace_start();
		for (int i = 0; i < ITERATIONS && ok; ++i)
			ok = net_round(&c);
		alloc_trace_stop();
		CHECK(ok);
		report("SpudNet's receives with SPUDNET_NO_WAIT on an idle loopback connection's two ends and a wait set's poll");
		uint64_t best = UINT64_MAX;
		for (int run = 0; run < TIMED_RUNS && ok; ++run) {
			const uint64_t start = spudperf_get_monotonic_time_ns();
			for (int i = 0; i < TIMED_ITERATIONS && ok; ++i)
				ok = net_round(&c);
			const uint64_t took = spudperf_get_monotonic_time_ns() - start;
			if (took < best)
				best = took;
		}
		if (ok)
			print_time("SpudNet idle round (two receives, a wait set poll)", best, TIMED_ITERATIONS);
	}
	spudnet_wait_set_destroy(c.set);
	spudnet_tcp_socket_destroy(c.server);
	spudnet_tcp_socket_destroy(c.client);
	spudnet_tcp_listener_destroy(listener);
	spudnet_instance_destroy(instance);
}

// ---- SpudInput -------------------------------------------------------------

static void test_input(void) {
#if SPUDINPUT_COMPILE_GAMECONTROLLER
	const SPUDINPUT_NATIVE_API api = SPUDINPUT_NATIVE_API_GAMECONTROLLER;
#elif SPUDINPUT_COMPILE_GAMEINPUT
	const SPUDINPUT_NATIVE_API api = SPUDINPUT_NATIVE_API_GAMEINPUT;
#elif SPUDINPUT_COMPILE_XINPUT
	const SPUDINPUT_NATIVE_API api = SPUDINPUT_NATIVE_API_XINPUT;
#elif SPUDINPUT_COMPILE_EVDEV
	const SPUDINPUT_NATIVE_API api = SPUDINPUT_NATIVE_API_EVDEV;
#else
	const SPUDINPUT_NATIVE_API api = SPUDINPUT_NATIVE_API_NONE;
#endif
	if (api == SPUDINPUT_NATIVE_API_NONE) {
		printf("spud_noalloc_tests: SpudInput: no native API in this build, skipped\n");
		return;
	}
	const SPUD_ALLOCATOR allocator = spud_allocator_system();
	SPUDINPUT_INSTANCE_DESC desc;
	memset(&desc, 0, sizeof(desc));
	desc.base.type  = SPUD_STRUCT_TYPE_INPUT_INSTANCE_DESC;
	desc.allocator  = &allocator;
	desc.native_api = api;
#if SPUDINPUT_COMPILE_XINPUT || SPUDINPUT_COMPILE_EVDEV
	desc.read_in_background = true; /* these always read */
#else
	desc.read_in_background = false;
#endif
	desc.motion_sample_capacity = 0;
	spudinput_instance instance = NULL;
	const SPUDRESULT created    = spudinput_instance_create(&desc, &instance);
	if (created != SPUD_SUCCESS) {
		printf("spud_noalloc_tests: SpudInput: no instance (%s), skipped\n", spudresult_str(created));
		return;
	}
	bool ok = true;
	for (int i = 0; i < WARM_UP && ok; ++i)
		ok = spudinput_instance_poll(instance) == SPUD_SUCCESS;
	CHECK(ok);
	const spudinput_gamepad_id *ids = NULL;
	uint32_t count                  = 0;
	spudinput_instance_get_gamepads(instance, &ids, &count);
	alloc_trace_start();
	for (int i = 0; i < ITERATIONS && ok; ++i)
		ok = spudinput_instance_poll(instance) == SPUD_SUCCESS;
	alloc_trace_stop();
	CHECK(ok);
	char what[128];
	snprintf(what, sizeof(what), "SpudInput's poll, %u gamepad(s) connected", count);
	report(what);
	uint64_t best = UINT64_MAX;
	for (int run = 0; run < TIMED_RUNS && ok; ++run) {
		const uint64_t start = spudperf_get_monotonic_time_ns();
		for (int i = 0; i < TIMED_ITERATIONS && ok; ++i)
			ok = spudinput_instance_poll(instance) == SPUD_SUCCESS;
		const uint64_t took = spudperf_get_monotonic_time_ns() - start;
		if (took < best)
			best = took;
	}
	if (ok)
		print_time("SpudInput poll", best, TIMED_ITERATIONS);
	spudinput_instance_destroy(instance);
}

int main(int argc, char **argv) {
	uint32_t adapter = 0;
	for (int i = 1; i < argc; ++i) {
		if (strcmp(argv[i], "--adapter") == 0 && i + 1 < argc) {
			adapter = (uint32_t)strtoul(argv[++i], NULL, 10);
		} else {
			fprintf(stderr, "usage: spud_noalloc_tests [--adapter N]\n");
			return 2;
		}
	}
	if (!alloc_trace_available()) {
		printf("spud_noalloc_tests: no allocation hook on this platform or build (a sanitizer's heap): nothing measured\n");
		return 0;
	}
	test_hook_counts();
	test_gpu(adapter);
	test_audio();
	test_net();
	test_input();
	if (failures) {
		printf("spud_noalloc_tests: %d failure(s)\n", failures);
		return 1;
	}
	printf("spud_noalloc_tests: no allocation of ours in any hot path\n");
	return 0;
}
