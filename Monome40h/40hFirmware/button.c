/* 
 *  Copyright (C) 2006, Joe Lake, monome.org
 * 
 *  This file is part of 40h.
 *
 *  40h is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  40h is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *  You should have received a copy of the GNU General Public License
 *  along with 40h; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 *  $Id: button.c,v. 1.1.1.1 2006/05/02 1:01:22
 */

#include "types.h"
#include "button.h"

uint8 button_current[8], 
      button_last[8], 
      button_state[8], 
      button_debounce_count[8][8], 
      button_event[8];


/***************************************************************************************************
 *
 * DESCRIPTION: initializes all button debouncing variables.
 *
 * ARGUMENTS:
 *
 * RETURNS:
 *
 * NOTES:       
 *
 ****************************************************************************************************/

void buttonInit(void)
{
    uint8 i;

    for (i = 0; i < 8; i++) {
        button_current[i] = 0x00;
        button_last[i] = 0x00;
        button_state[i] = 0x00;
        button_event[i] = 0x00;
    }
}


/***************************************************************************************************
 *
 * DESCRIPTION: debounces physical button states and queues up debounced button state change events
 *              in button_event array for outputting.
 *
 * ARGUMENTS:   row -   the row of buttons to be debounced.
 *              index - the index (column) of the button in the specified row to be debounced.
 *
 * RETURNS:
 *
 * NOTES:       we debounce buttons so that momentary, accidental changes in button state do not
 *              cause button press events to be reported.
 *
 ****************************************************************************************************/

void buttonCheck(uint8 row, uint8 index)
{
    if (((button_current[row] ^ button_last[row]) & (1 << index)) &&   // if the current physical button state is different from the
        ((button_current[row] ^ button_state[row]) & (1 << index))) {  // last physical button state AND the current debounced state

        if (button_current[row] & (1 << index)) {                      // if the current physical button state is depressed
            button_event[row] = kButtonNewEvent << index;              // queue up a new button event immediately
            button_state[row] |= (1 << index);                         // and set the debounced state to down.
        }
        else
            button_debounce_count[row][index] = kButtonUpDefaultDebounceCount;  // otherwise the button was previously depressed and now
                                                                                // has been released so we set our debounce counter.
    }
    else if (((button_current[row] ^ button_last[row]) & (1 << index)) == 0 &&  // if the current physical button state is the same as
             (button_current[row] ^ button_state[row]) & (1 << index)) {        // the last physical button state but the current physical
                                                                                // button state is different from the current debounce 
                                                                                // state...

        if (button_debounce_count[row][index] > 0 && --button_debounce_count[row][index] == 0) {  // if the the debounce counter has
                                                                                                  // been decremented to 0 (meaning the
                                                                                                  // the button has been up for 
                                                                                                  // kButtonUpDefaultDebounceCount 
                                                                                                  // iterations///

            button_event[row] = kButtonNewEvent << index;    // queue up a button state change event

            if (button_current[row] & (1 << index))          // and toggle the buttons debounce state.
                button_state[row] |= (1 << index);
            else
                button_state[row] &= ~(1 << index);
        }
    }
}
