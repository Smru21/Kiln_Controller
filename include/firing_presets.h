#ifndef FIRING_PRESETS_H
#define FIRING_PRESETS_H

#include <stdlib.h>
#include <Arduino.h>

#define glaze_steps

uint16_t glaze_reachTime[glaze_steps];
uint16_t glaze_targetTemp[glaze_steps];
uint16_t glaze_holdTime[glaze_steps];

#define bisque_steps

uint16_t bisque_reachTime[bisque_steps];
uint16_t bisque_targetTemp[bisque_steps];
uint16_t bisque_holdTime[bisque_steps];

#endif
