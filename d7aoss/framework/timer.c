/*! \file timer.c
 *
 * \copyright (C) Copyright 2013 University of Antwerp (http://www.cosys-lab.be) and others.\n
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.\n
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * \author maarten.weyn@uantwerpen.be
 * \author glenn.ergeerts@uantwerpen.be
 */
#include "timer.h"

#include "../hal/timer.h"
#include "log.h"

#define LOG_FWK_ENABLED

// turn on/off the debug prints
#ifdef LOG_FWK_ENABLED
#define DPRINT(...) log_print_stack_string(LOG_FWK, __VA_ARGS__)
#else
#define DPRINT(...)  
#endif


void timer_init()
{
    hal_timer_init();

    queue_init(&event_queue, (uint8_t*)&event_array, sizeof(event_array));
    started = false;
}

bool timer_insert_value_in_queue(timer_event* event)
{
	uint16_t current_timer = hal_timer_getvalue();

        DPRINT("timer_insert current_time %d", current_timer);
        DPRINT(" - next %d queue len %d", event->next_event, event_queue.length);

    // TODO: substract time already gone
	uint8_t position = 0;
	uint16_t next_event =  event->next_event;
	event->next_event = current_timer + event->next_event;

	while (position < event_queue.length)
	{

        DPRINT(" - position  %d queue len %d", position, event_queue.length);
		timer_event *temp_event = (timer_event*) queue_read_value(&event_queue, position, sizeof(timer_event));

        DPRINT(" - tempevent  next_event %d event %p", temp_event->next_event, temp_event->f);
        DPRINT(" - tempevent  ne - ct %d",  temp_event->next_event - current_timer);

        if ((temp_event->next_event - current_timer)  > next_event)
		{
        	DPRINT(" - ne-ct > ne");
			if (position == 0)
			{
				DPRINT(" - position == 0");
				hal_timer_disable_interrupt();
				started = false;
			}
			queue_insert_value(&event_queue, (void*) event, position, sizeof(timer_event));
			position = 0;
			break;
		}
		position++;
	}

	if (position == event_queue.length)
	{
		if (!queue_push_value(&event_queue, (void*) event, sizeof(timer_event)))
			return false;
	}

	#ifdef LOG_FWK_ENABLED
	position = 0;
	for (;position<event_queue.length; position++)
	{
		timer_event *temp_event = (timer_event*) queue_read_value(&event_queue, position, sizeof(timer_event));
		DPRINT("Queue: %d", temp_event->next_event);
	}
	DPRINT("timer_insert E - Timer current value: %d",  hal_timer_getvalue());
	#endif
	/*
	// code when timer was up instead of continous
    int16_t sum_next_event = - hal_timer_getvalue();

    while (position < event_queue.length)
    {
        timer_event *temp_event = (timer_event*) queue_read_value(&event_queue, position, sizeof(timer_event));
        if (event->next_event > sum_next_event + temp_event->next_event)
        {
            sum_next_event += temp_event->next_event;
        } else {
            uint16_t elapsed = 0;
            if (position == 0)
            {
                elapsed = hal_timer_getvalue();
                hal_timer_disable_interrupt();
                started = false;
            } else {c
                event->next_event -= sum_next_event;
            }

            queue_insert_value(&event_queue, (void*) event, position, sizeof(timer_event));
            temp_event = (timer_event*) queue_read_value(&event_queue, position+1, sizeof(timer_event));

            temp_event->next_event -= (event->next_event + elapsed);
            return true;
        }
        position++;
    }

    if (position == event_queue.length)
    {
        if (started) event->next_event -= sum_next_event;
        return queue_push_value(&event_queue, (void*) event, sizeof(timer_event));
    }
    */

    return true;
}

bool timer_add_event(timer_event* event)
{

    DPRINT("Add event: current timer: %d.", hal_timer_getvalue());

    if (event->next_event == 0)
    {
        event->f();
        return true;
    }

    timer_event new_event;
    new_event.f = event->f;
    new_event.next_event = event->next_event;


    DPRINT(" - new event : %d %p", new_event.next_event, new_event.f);

    if (timer_insert_value_in_queue(&new_event))
    {
        if (!started)
        {
            hal_timer_enable_interrupt();
            started = true;
            hal_timer_setvalue(new_event.next_event);
        }
        uint16_t diff = hal_timer_getvalue() - new_event.next_event;
        if (diff < 1000)
		{
                    DPRINT("timer_add_event M timer overrun : %d", hal_timer_getvalue());
                    DPRINT("timer_add_event M: %d", new_event.next_event);
                    timer_completed();
		}
    } else {
        DPRINT("Cannot add event, queue is full");
        return false;
    }

    DPRINT("timer_add_event timer E : %d", hal_timer_getvalue());

    return true;
}

void timer_completed()
{

    DPRINT("Timer Completed: cti: %d", hal_timer_getvalue());

    timer_event* event = (timer_event*) queue_pop_value(&event_queue, sizeof(timer_event));

    DPRINT(" - event  next_event %d event %p", event->next_event, event->f);
    DPRINT(" - event_queue lenght   %d ", event_queue.length);

    bool directly_fire_new_event = false;

    if (event_queue.length == 0)
    {
        hal_timer_disable_interrupt();
        started = false;
    } else {
    	DPRINT(" - next_event  next_event %d event %p", ((timer_event*) event_queue.front)->next_event, ((timer_event*) event_queue.front)->f);

        hal_timer_setvalue(((timer_event*) event_queue.front)->next_event);
        uint16_t diff = hal_timer_getvalue() - ((timer_event*) event_queue.front)->next_event;
        DPRINT(" - diff   %d ", diff);

        if (diff < 1000)
        {
            directly_fire_new_event = true;
        }
    }

    event->f();

    if (directly_fire_new_event)
    {
    	timer_completed();
    }
}

uint16_t timer_get_counter_value()
{
    return hal_timer_getvalue();
}

volatile bool waiting;

void timer_wait_done(void)
{
    waiting = false;
}

timer_event timer_wait = { .next_event = 0, .f = timer_wait_done };

void timer_wait_ms(uint16_t ms)
{
    /*
	waiting = true;
    timer_wait.next_event = ms;
    timer_add_event( &timer_wait );
    while(waiting);
    */
    int i;
    for( i=0 ; i<ms ; i++ )
    {
            volatile uint32_t n = 32000;
            while(n--);
    }
}


void benchmarking_timer_init()
{
	hal_benchmarking_timer_init();
}

uint32_t benchmarking_timer_getvalue()
{
    return hal_benchmarking_timer_getvalue();
}
void benchmarking_timer_start()
{
	hal_benchmarking_timer_start();
}

void benchmarking_timer_stop()
{
	hal_benchmarking_timer_stop();
}
