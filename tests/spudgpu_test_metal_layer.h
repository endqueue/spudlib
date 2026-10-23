#ifndef SPUDGPU_TEST_METAL_LAYER_H
#define SPUDGPU_TEST_METAL_LAYER_H

/* A CAMetalLayer with no window (spudgpu_test_metal_layer.m), Metal builds
 * of spudgpu_objects_test only. */

#include <stdbool.h>

/* A new 64x64 layer; [geometry_flipped] sets its geometryFlipped. */
void *spudgpu_test_metal_layer_create(bool geometry_flipped);
void spudgpu_test_metal_layer_destroy(void *layer);
/* The layer's contentsGravity: "topLeft", "bottomLeft", "resize",
 * "resizeAspect", or "other" for anything else. */
const char *spudgpu_test_metal_layer_gravity(void *layer);
/* Sets a gravity no present scaling uses (center), so a test can see that
 * swap chain creation replaced it. */
void spudgpu_test_metal_layer_set_gravity_other(void *layer);

#endif
