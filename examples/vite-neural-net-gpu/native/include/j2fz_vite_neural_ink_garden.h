#ifndef FOZZY_J2FZ_VITE_NEURAL_INK_GARDEN_H
#define FOZZY_J2FZ_VITE_NEURAL_INK_GARDEN_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef int32_t (*fz_callback_i32_v0)(int32_t arg);
int32_t fz_host_init(void);
int32_t fz_host_shutdown(void);
int32_t fz_host_cleanup(void);
int32_t fz_host_last_error_code(void);
int32_t fz_host_last_error_class(void);
const char* fz_host_last_error_message(void);
int32_t fz_host_register_callback_i32(int32_t slot, fz_callback_i32_v0 cb);
int32_t fz_host_invoke_callback_i32(int32_t slot, int32_t arg);

int32_t randomize_model(const float* weights_borrowed, size_t weights_len, int32_t weight_count, int32_t seed);
int32_t evaluate_model(const float* weights_borrowed, size_t weights_len, int32_t weight_count, const float* points_xy_borrowed, size_t points_xy_len, int32_t point_count, const int32_t* point_classes_borrowed, size_t point_classes_len, int32_t class_count, const float* stats_out_out, size_t stats_out_len, int32_t stats_count);
int32_t train_steps(const float* weights_inout, size_t weights_len, int32_t weight_count, const float* points_xy_borrowed, size_t points_xy_len, int32_t point_count, const int32_t* point_classes_borrowed, size_t point_classes_len, int32_t class_count, int32_t steps, float learning_rate, const float* stats_out_out, size_t stats_out_len, int32_t stats_count);
int32_t render_field(const float* coords_borrowed, size_t coords_len, int32_t coord_count, const float* weights_borrowed, size_t weights_len, int32_t weight_count, int32_t mode, int32_t selected_neuron, int32_t time_tick, const uint32_t* out_rgba_out, size_t out_rgba_len, int32_t out_count);

#ifdef __cplusplus
}
#endif

#endif
