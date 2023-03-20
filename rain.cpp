/*
 * rain.cpp - from the Unicorn C(++) Examples collection
 *
 * This is an implementation of the terminal-era 'rain' program - it simulates
 * rain falling on your Unicorn!
 *
 * Copyright (C) 2023 Pete Favelle <ahnlak@ahnlak.com>
 * Released under the MIT License; see LICENSE for details.
 */

/* System headers. */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "pico/stdlib.h"

/* Local headers. */

#include "libraries/pico_graphics/pico_graphics.hpp"
#include "libraries/galactic_unicorn/galactic_unicorn.hpp"


/* Constants. */

#define  RAINDROP_LIFESPAN    7
#define  RAINDROP_MAX         10
#define  RAINDROP_MIN         2


/* Structs. */

typedef struct
{
  uint_fast8_t x;
  uint_fast8_t y;
  uint_fast8_t age;
  bool         alive;
} raindrop_t;


/* Functions. */


/*
 * main - this is such a small job, everything just slots into main(). Feels
 *        a little untidy, but at the same time I don't want to over-engineer!
 */

int main()
{
  int                               l_palette[RAINDROP_LIFESPAN];
  int                               l_black_pen;
  uint_fast8_t                      l_index, l_dropcount, l_dropgap;
  raindrop_t                        l_raindrops[RAINDROP_MAX];
  pimoroni::GalacticUnicorn        *l_unicorn;
  pimoroni::PicoGraphics_PenRGB565 *l_graphics;

  /*
   * First thing to do is to create the Unicorn and Graphics objects. Pimoroni
   * examples do this in variable declarations but I prefer it split out.
   */
  l_unicorn = new pimoroni::GalacticUnicorn();
  l_graphics = new pimoroni::PicoGraphics_PenRGB565( pimoroni::GalacticUnicorn::WIDTH,
                                                     pimoroni::GalacticUnicorn::HEIGHT,
                                                     nullptr );

  /* Next up, we need to intialise both the Pico and the Unicorn. */
  stdio_init_all();
  l_unicorn->init();

  /*
   * Our raindrops have a fairly simple, static palette - we only need to 
   * work this out once, at start up.
   */
  l_palette[0] = l_graphics->create_pen( 255, 255, 255 );
  l_palette[1] = l_graphics->create_pen(  50,  50, 150 );
  l_palette[2] = l_graphics->create_pen(  40,  40, 100 );
  l_palette[3] = l_graphics->create_pen(  30,  30,  80 );
  l_palette[4] = l_graphics->create_pen(  20,  20,  50 );
  l_palette[5] = l_graphics->create_pen(  10,  10,  20 );
  l_palette[6] = l_graphics->create_pen(   5,   5,  10 );

  l_black_pen = l_graphics->create_pen( 0, 0, 0 );

  /*
   * Initialise our raindrop array; compiler defaults should do this for us,
   * but there's no harm in being explicit about it. 
   */
  for( l_index = 0; l_index < RAINDROP_MAX; l_index++ )
  {
    l_raindrops[l_index].alive = false;
  }

  /* Lastly, we need to initialise our random number generator. */
  srand( time( NULL ) );

  /*
   * All set up, so now we enter effectively an infinite loop.
   */
  while( true )
  {
    /* Start the frame by clearing the screen. */
    l_graphics->set_pen( l_black_pen );
    l_graphics->clear();

    /* Look through the raindrop list; count the ones that are still alive. */
    l_dropcount = 0;
    l_dropgap = RAINDROP_MAX;
    for ( l_index = 0; l_index < RAINDROP_MAX; l_index++ )
    {
      /* If it's reached it's lifespan, it dies. */
      if ( l_raindrops[l_index].age >= RAINDROP_LIFESPAN )
      {
        l_raindrops[l_index].alive = false;
      }

      /* Only count the living. */
      if ( l_raindrops[l_index].alive )
      {
        /* Keep track of how many are alive. */
        l_dropcount++;
      }
      else
      {
        /* Remember this is an available gap for a raindrop! */
        l_dropgap = l_index;
      }
    }

    /* Decide if we need a new raindrop. */
    if ( ( l_dropgap < RAINDROP_MAX ) && 
         ( l_dropcount < ( RAINDROP_MIN + rand()%( RAINDROP_MAX - RAINDROP_MIN ) ) ) )
    {
      /* So, spawn a new raindrop in a random location. */
      l_raindrops[l_dropgap].x = rand()%pimoroni::GalacticUnicorn::WIDTH;
      l_raindrops[l_dropgap].y = rand()%pimoroni::GalacticUnicorn::HEIGHT;
      l_raindrops[l_dropgap].age = 0;
      l_raindrops[l_dropgap].alive = true;
    }

    /* Now, work through all living raindrops and render / age them. */
    for( l_index = 0; l_index < RAINDROP_MAX; l_index++ )
    {
      /* Skip any dead raindrops. */
      if ( !l_raindrops[l_index].alive )
      {
        continue;
      }

      /* First thing to do is to draw it then - outer circle first. */
      l_graphics->set_pen( l_palette[l_raindrops[l_index].age] );
      l_graphics->circle(
        pimoroni::Point( l_raindrops[l_index].x, l_raindrops[l_index].y ),
        l_raindrops[l_index].age
      );

      /*
       * But circles are filled, so we draw a slightly smaller black circle
       * inside it to turn it into an outline.
       */
      if ( l_raindrops[l_index].age > 1 )
      {
        l_graphics->set_pen( l_black_pen );
        l_graphics->circle(
          pimoroni::Point( l_raindrops[l_index].x, l_raindrops[l_index].y ),
          l_raindrops[l_index].age - 1
        );
      }

      /* Older drops are big enough that we have a central dot in them too. */
      if ( l_raindrops[l_index].age > 4 )
      {
        l_graphics->set_pen( l_palette[l_raindrops[l_index].age] );
        l_graphics->circle(
          pimoroni::Point( l_raindrops[l_index].x, l_raindrops[l_index].y ),
          1
        );
      }

      /* All drawn, so just age the drop. */
      l_raindrops[l_index].age++;
    }

    /* Raindrops are all processed - so, we ask the Unicorn to update. */
    l_unicorn->update( l_graphics );

    /* And wait a short while for the next frame. */
    sleep_ms( 125 );
  }

  /* We'll never get here! */
  return 0;
}

/* End of file rain.cpp */
