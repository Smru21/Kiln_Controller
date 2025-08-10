// file - firing_presets.h
#ifndef FIRING_PRESETS_H
#define FIRING_PRESETS_H

#include <stdlib.h>
#include <Arduino.h>

#define glaze_steps 6

extern uint16_t glaze_reachTime[glaze_steps];
extern uint16_t glaze_targetTemp[glaze_steps];
extern uint16_t glaze_holdTime[glaze_steps];

#define bisque_steps 5

extern uint16_t bisque_reachTime[bisque_steps];
extern uint16_t bisque_targetTemp[bisque_steps];
extern uint16_t bisque_holdTime[bisque_steps];

#endif //FIRING_PRESETS_H
