
#ifndef TIMER_H
#define TIMER_H

void Timer_Init (unsigned int clockFrequency, unsigned int IntrPriority);

unsigned int Timer_GetTimeMilliseconds ();

void Timer_Wait (unsigned int milliseconds);

#endif
