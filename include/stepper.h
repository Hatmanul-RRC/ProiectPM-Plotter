#ifndef STEPPER_H
#define STEPPER_H

void init_steppers();
void init_timer1();
void moveTo(float target_x_mm, float target_y_mm);

#endif
