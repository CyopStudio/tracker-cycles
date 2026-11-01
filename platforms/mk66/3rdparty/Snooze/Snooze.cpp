/******************************************************************************
 * Low Power Library for Teensy LC/3.x
 * Copyright (c) 2014, Colin Duffy https://github.com/duff2013
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice, development funding notice, and this permission
 * notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *******************************************************************************
 *  Snooze.cpp
 *  Teensy 3.x/LC
 *
 * Purpose:    Provides routines for configuring the Teensy for low power.
 *
 * NOTE:       None
 *******************************************************************************/
#include "Arduino.h"

#include "Snooze.h"
#include "utility/clocks.h"
#include "utility/sleep.h"

void SnoozeClass::sleep()
{
    mode = false;
    // p->mode = VLPS;
    // p->enableDriver();
    pee_blpi();
    // pee_blpe( );
    enter_vlpr();
    // vlps( );
    // vlpw( );
    // exit_vlpr( );

    // blpe_pee();
    // blpi_pee( );
    // p->disableDriver( );
    //    p->mode = RUN;
}

void SnoozeClass::wakeUp()
{
    mode = true;

    exit_vlpr();
    blpi_pee();
}
