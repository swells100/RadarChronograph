#ifndef BUTTONS_H
#define BUTTONS_H

extern int ARM;
extern int Portrait;
//extern int variable3;
extern int ShotTime;
extern int ScreenBrightness;

void create_buttons();
void set_arm_state(bool armed); //set to false when data collection is done
#endif