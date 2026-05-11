#pragma once
#include <stdint.h>
#include <stdbool.h>

void lightshow_dispatch(void);
void spotlight_anim_update(void);

void lightshow_chase(void);
void lightshow_twinkles(void);
void lightshow_rainbow(void);
void lightshow_rain(void);
void lightshow_fire(void);
void lightshow_snake(void);
void lightshow_cylon(void);
void lightshow_green_matrix(void);

void spectrum_task_fn(void *arg);
