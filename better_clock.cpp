/*
 * better_clock.cpp - from the Unicorn C(++) Examples collection
 *
 * This is an 'improved' version of the 'clock.py' program from the Pimoroni
 * examples. This version fixes a few niggles I've always had with it, namely:
 *
 * - uses a custom, fixed-width font (so the time doesn't shift left/right)
 * - handles timezones better
 * - adjusts brightness based on ambient light
 *
 * Copyright (C) 2023 Pete Favelle <ahnlak@ahnlak.com>
 * Released under the MIT License; see LICENSE for details.
 */

/* System headers. */

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "hardware/rtc.h"
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "pico/util/datetime.h"

#include "lwip/dns.h"
#include "lwip/pbuf.h"
#include "lwip/udp.h"


/* Local headers. */

#include "libraries/pico_graphics/pico_graphics.hpp"
#include "libraries/galactic_unicorn/galactic_unicorn.hpp"
#include "numeric_font.hpp"


/* Constants. */

#define BC_DIM_FREQUENCY_SECS    60LLU
#define BC_NTP_FREQUENCY_SECS    3600LLU
#define BC_USECS_IN_SEC          1000000LLU

#define NTP_SERVER               "pool.ntp.org"
#define NTP_PORT                 123
#define NTP_PACKET_LEN           48
#define NTP_EPOCH_OFFSET         2208988800L


/* Structs. */

typedef struct
{
  ip_addr_t       server;
  struct udp_pcb *socket;
  bool            active_query;
  uint32_t        time;
} ntpstate_t;


/* Functions. */

/*
 * dimmer - applies a suitable dimmer / brightness adjustment, based on the
 *          requested base brightness and modified depending on the ambient
 *          lighting conditions.
 */

void dimmer( pimoroni::GalacticUnicorn *p_unicorn, float p_brightness )
{
  /* We adjust the desired brightness by the ambient light reading. */
  float l_brightness =  p_brightness / 2048 * ( p_unicorn->light() + 512 );

  /* But also make sure we don't set it *too* low. */
  if ( l_brightness < 0.1f )
  {
    l_brightness = 0.1f;
  }

  /* Just set it then, and we're done. */
  p_unicorn->set_brightness( l_brightness );

  /* All done. */
  return;
}


/*
 * ntp_request - sends an NTP request to the server; once called, we should
 *               receive a response back!
 */

void ntp_request( ntpstate_t *p_ntpstate )
{
  struct pbuf  *l_buffer;
  uint8_t      *l_payload; 

  /* Calls into lwIP need to be correctly locked. */
  cyw43_arch_lwip_begin();

  /* Allocate the packet buffer we'll send. */
  l_buffer = pbuf_alloc( PBUF_TRANSPORT, NTP_PACKET_LEN, PBUF_RAM );
  l_payload = (uint8_t *)l_buffer->payload;

  /* Just set the flag in the start of that packet as a V3 client request. */
  memset( l_payload, 0, NTP_PACKET_LEN );
  l_payload[0] = 0x1b;

  /* And send it. */
  udp_sendto( p_ntpstate->socket, l_buffer, &p_ntpstate->server, NTP_PORT );

  /* Lastly free up the buffer. */
  pbuf_free( l_buffer );

  /* End of lwIP locked calls. */
  cyw43_arch_lwip_end();

  /* All done. */
  return;
}


/*
 * ntpcb_* - a collection of callback functions used to handle the NTP request;
 *           lwIP uses callbacks to remain async.
 */

void ntpcb_recv( void *p_ntpstate, struct udp_pcb *p_socket, struct pbuf *p_buffer, 
                 const ip_addr_t *p_addr, uint16_t p_port )
{
  ntpstate_t *l_ntpstate = (ntpstate_t *)p_ntpstate;
  uint8_t     l_mode, l_stratum;
  uint8_t     l_ntptime[4];

  /* Called whenever a packet is received; all we really need to do here is  */
  /* to make sure it looks like an NTP message and decode the provided time. */
  l_mode = pbuf_get_at( p_buffer, 0 ) & 0x07;
  l_stratum = pbuf_get_at( p_buffer, 1 );

  if ( ( p_port == NTP_PORT ) && ( p_buffer->tot_len == NTP_PACKET_LEN ) &&
       ( l_mode == 0x04 ) && ( l_stratum != 0 ) )
  {
    /* Looks valid; just extract the time value. */
    pbuf_copy_partial( p_buffer, l_ntptime, sizeof( l_ntptime ), 40 );
    l_ntpstate->time = l_ntptime[0] << 24 | l_ntptime[1] << 16 | l_ntptime[2] << 8 | l_ntptime[3];
  }

  /* All done. */
  return;
}

void ntpcb_dns( const char *p_name, const ip_addr_t *p_addr, void *p_ntpstate )
{
  ntpstate_t *l_ntpstate = (ntpstate_t *)p_ntpstate;

  /* Called when we get an answer back from the DNS lookup. Save it and kick */
  /* off the actual NTP request.                                             */
  if ( p_addr != nullptr )
  {
    /* Simply save it into our state. */
    memcpy( &l_ntpstate->server, p_addr, sizeof( ip_addr_t ) );

    /* And send the request. */
    ntp_request( l_ntpstate );
  }
  else
  {
    /* Indicates a DNS failure. */
    printf( "DNS failure\n" );
    l_ntpstate->active_query = false;
  }

  /* All done. */
  return;
}

datetime_t *ntp_apply_timezone( uint32_t p_ntptime, int8_t p_timezone )
{
  static datetime_t l_datetime;
  time_t            l_timet;
  struct tm        *l_tmstruct;

  /* The time will be in seconds since 1900; convert it to a tmstruct. */
  l_timet = p_ntptime - NTP_EPOCH_OFFSET + ( 3600 * p_timezone );
  l_tmstruct = gmtime( &l_timet );

  l_datetime.year  = l_tmstruct->tm_year;
  l_datetime.month = l_tmstruct->tm_mon;
  l_datetime.day   = l_tmstruct->tm_mday;
  l_datetime.dotw  = l_tmstruct->tm_wday;
  l_datetime.hour  = l_tmstruct->tm_hour;
  l_datetime.min   = l_tmstruct->tm_min;
  l_datetime.sec   = l_tmstruct->tm_sec;

  /* And return our internal datetime. */
  return &l_datetime;
}


/*
 * rtc_add_hours - this is a horrible, horrible bodge to try and handle timezones.
 *                 Basically, we need to add or subtract hours to a datetime_t
 *                 but there aren't any manipulation functions. Easiest is to 
 *                 turn it into a time_t, do the math and turn it back. Horrible.
 */

datetime_t *rtc_add_hours( const datetime_t *p_rtctime, int8_t p_offset )
{
  static datetime_t l_datetime;
  time_t            l_timet;
  struct tm         l_tmstruct;
  struct tm        *l_tmsptr;

  /* Convert the datetime_t into a tmstruct. */
  l_tmstruct.tm_year = p_rtctime->year;
  l_tmstruct.tm_mon  = p_rtctime->month;
  l_tmstruct.tm_mday = p_rtctime->day;
  l_tmstruct.tm_wday = p_rtctime->dotw;
  l_tmstruct.tm_hour = p_rtctime->hour;
  l_tmstruct.tm_min  = p_rtctime->min;
  l_tmstruct.tm_sec  = p_rtctime->sec;

  /* Get a time_t out of that. */
  l_timet = mktime( &l_tmstruct );

  /* Add / subtract our time. */
  l_timet += ( 3600 * p_offset );

  /* Turn it back (!) */
  l_tmsptr = gmtime( &l_timet );

  l_datetime.year  = l_tmsptr->tm_year;
  l_datetime.month = l_tmsptr->tm_mon;
  l_datetime.day   = l_tmsptr->tm_mday;
  l_datetime.dotw  = l_tmsptr->tm_wday;
  l_datetime.hour  = l_tmsptr->tm_hour;
  l_datetime.min   = l_tmsptr->tm_min;
  l_datetime.sec   = l_tmsptr->tm_sec;

  /* And return our internal datetime. */
  return &l_datetime;
}


/*
 * checktime - attempts to fetch the time via NTP, and set the RP2040's clock
 *             appropriately. Will only return TRUE once it has successfully
 *             done this, so that it can handle the (potentially long) process
 *             without interrupting updates.
 */

bool checktime( int8_t p_timezone )
{
  static bool       l_active = false;
  static bool       l_connecting = false;
  int               l_link_status, l_error;
  static ntpstate_t l_ntpstate;
  time_t            l_timet;
  struct tm        *l_tmstruct;
  datetime_t        l_rtctime;

  /* If the wireless isn't currently active, we need to kick that off. */
  if ( !l_active )
  {
    /* Initialise the WiFi. */
    cyw43_arch_init();
    cyw43_arch_enable_sta_mode();
    cyw43_arch_wifi_connect_async( WIFI_SSID, WIFI_PASSWORD, CYW43_AUTH_WPA2_AES_PSK );
    l_connecting = true;
    l_active = true;

    /* And reset the NTP state object. */
    if ( l_ntpstate.socket != nullptr )
    {
      udp_remove( l_ntpstate.socket );
      l_ntpstate.socket = nullptr;
    }
    l_ntpstate.active_query = false;
    l_ntpstate.time = 0;
  }

  /* We'll need to know the link status, whatever else we do. */
  l_link_status = cyw43_tcpip_link_status( &cyw43_state, CYW43_ITF_STA );

  /* So, if we're connecting we wait to see if it's up! */
  if ( l_connecting )
  {
    /* If it's failed hard, disconnect and we'll try again. */
    if ( ( l_link_status == CYW43_LINK_FAIL ) || ( l_link_status == CYW43_LINK_BADAUTH ) ||
         ( l_link_status == CYW43_LINK_NONET ) )
    {
      printf( "Failed to initialise WiFi (err %d)\n", l_link_status );
      cyw43_arch_deinit();
      l_active = false;
      l_connecting = false;
      return false;
    }

    /* If it's connected, we're out of that phase. */
    if ( l_link_status == CYW43_LINK_UP )
    {
      l_connecting = false;
    }
  }

  /* After those checks, if we're not connecting we *should* be connected. */
  if ( !l_connecting )
  {
    /* We'll need a PCB (which I'm gonna call a socket) for our work. */
    if ( l_ntpstate.socket == nullptr )
    {
      l_ntpstate.socket = udp_new_ip_type( IPADDR_TYPE_ANY );
      if ( l_ntpstate.socket == nullptr )
      {
        printf( "Failed to create UDP PCB socket\n" );
        return false;
      }

      /* So, as long as we have a valid socket, set up the recv handler. */
      udp_recv( l_ntpstate.socket, ntpcb_recv, &l_ntpstate );
    }

    /* If there's already a query, we just wait to have a response. */
    if ( l_ntpstate.active_query )
    {
      /* Wait until the time is set. */
      if ( l_ntpstate.time > 0 )
      {
        /* Apply our timezone and update the RTC with this time. */
        rtc_set_datetime( ntp_apply_timezone( l_ntpstate.time, p_timezone ) );

        /* Lastly, tear down the connection and indicate it's all worked. */
        cyw43_arch_deinit();
        l_active = false;
        l_connecting = false;
        return true;
      }
    }
    else
    /* If we don't already have a request outstanding, ask for server IP. */
    {
      /* Calls into lwIP need to be correctly locked. */
      cyw43_arch_lwip_begin();
      l_error = dns_gethostbyname( NTP_SERVER, &l_ntpstate.server, ntpcb_dns, &l_ntpstate );
      l_ntpstate.active_query = true;
      cyw43_arch_lwip_end();

      /* If we got an OK straight away, we had a cached DNS entry. */
      if ( l_error == ERR_OK )
      {
        /* Call the lookup directly. */
        ntp_request( &l_ntpstate );
      }
      else if ( l_error != ERR_INPROGRESS )
      {
        printf( "Failed to lookup NTP server DNS\n" );
        l_ntpstate.active_query = false;
        return false;
      }
    }
  }

  /* This false isn't a failure, it's just we have more work to do. */
  return false;
}


/*
 * main - entry point, from which everything is controlled.
 */

int main()
{
  int                               l_black_pen, l_white_pen;
  bool                              l_blink;
  uint_fast8_t                      l_adjusted_brightness = 0, l_adjusted_timezone = 0;
  float                             l_base_brightness;
  uint64_t                          l_current_tick, l_dim_tick, l_ntp_tick;
  uint_fast8_t                      l_index;
  datetime_t                        l_time;
  datetime_t                       *l_newtime;
  int8_t                            l_timezone = 0;
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
  l_blink = true;
  l_base_brightness = 0.5f;

  /* Set up some standard pens we will always need. */
  l_black_pen = l_graphics->create_pen( 0, 0, 0 );
  l_white_pen = l_graphics->create_pen( 255, 255, 255 );

  /* Need to initialise the RTC, which appears not to actually run until set. */
  rtc_init();
  l_time.year = 2023;
  l_time.month = 1;
  l_time.day = 1;
  l_time.dotw = 0;
  l_time.hour = l_time.min = l_time.sec = 0;
  rtc_set_datetime( &l_time );

  /* Lastly, we need to initialise our random number generator and other bits. */
  l_dim_tick = l_ntp_tick = 0;
  l_current_tick = time_us_64();
  srand( l_current_tick );

  /*
   * All set up, so now we enter effectively an infinite loop.
   */
  while( true )
  {
    /*
     * Update.
     */

    /* Check the time - this is seconds since boot, not 'real' time. */
    l_current_tick = time_us_64();

    /* Should we check the ambient light? */
    if ( ( l_current_tick < BC_USECS_IN_SEC ) ||
         ( l_current_tick > ( l_dim_tick + ( BC_DIM_FREQUENCY_SECS*BC_USECS_IN_SEC ) ) ) )
    {
      dimmer( l_unicorn, l_base_brightness );
      l_dim_tick = l_current_tick;
    }

    /* And the clock? */
    if ( ( l_ntp_tick == 0 ) ||
         ( l_current_tick > ( l_ntp_tick + ( BC_NTP_FREQUENCY_SECS*BC_USECS_IN_SEC ) ) ) )
    {
      /* If we succeed, we're done until the next check. */
      if ( checktime( l_timezone ) )
      {
        l_ntp_tick = l_current_tick;
      }
    }


    /*
     * User Input.
     */

    /* First up, brightness - controlled by the Unicorn's LUX buttons. */
    if ( l_unicorn->is_pressed( pimoroni::GalacticUnicorn::SWITCH_BRIGHTNESS_UP ) )
    {
      if ( ( l_base_brightness += 0.1f ) > 1.0f )
      {
        l_base_brightness = 1.0f;
      }
      dimmer( l_unicorn, l_base_brightness );
      l_adjusted_brightness = 4;
    }
    if ( l_unicorn->is_pressed( pimoroni::GalacticUnicorn::SWITCH_BRIGHTNESS_DOWN ) )
    {
      if ( ( l_base_brightness -= 0.1f ) < 0.1f )
      {
        l_base_brightness = 0.1f;
      }
      dimmer( l_unicorn, l_base_brightness );
      l_adjusted_brightness = 4;
    }

    /* Next, adjusting the timezone using the volume buttons (like clock.py) */
    if ( l_unicorn->is_pressed( pimoroni::GalacticUnicorn::SWITCH_VOLUME_UP ) )
    {
      if ( l_timezone < 14 )
      {
        /* Increment the timezone, and add that hour to the RTC. */
        l_adjusted_timezone = 4;
        l_timezone++;
        rtc_get_datetime( &l_time );
        l_newtime = rtc_add_hours( &l_time, 1 );
        rtc_set_datetime( l_newtime );

        /* Need to wait for the RTC to actually update. */
        sleep_us( 64 );
      }
    }
    if ( l_unicorn->is_pressed( pimoroni::GalacticUnicorn::SWITCH_VOLUME_DOWN ) )
    {
      if ( l_timezone > -12 )
      {
        /* Increment the timezone, and add that hour to the RTC. */
        l_adjusted_timezone = 4;
        l_timezone--;
        rtc_get_datetime( &l_time );
        l_newtime = rtc_add_hours( &l_time, -1 );
        rtc_set_datetime( l_newtime );

        /* Need to wait for the RTC to actually update. */
        sleep_us( 64 );
      }
    }

    /*
     * Render.
     */

    /* Start the frame by clearing the screen. */
    l_graphics->set_pen( l_black_pen );
    l_graphics->clear();
    l_graphics->set_pen( l_white_pen );

    /* If we're adjusting timezones, just display that. */
    if ( l_adjusted_timezone > 0 )
    {
      l_adjusted_timezone--;

      /* "UTC" */
      NumericFont::render( l_graphics, 10, 2, 10 );
      NumericFont::render( l_graphics, 15, 2, 11 );
      NumericFont::render( l_graphics, 20, 2, 12 );

      /* Sign. */
      if ( l_timezone > 0 )
      {
        NumericFont::render( l_graphics, 25, 2, 13 );
      }
      else if ( l_timezone < 0 )
      {
        NumericFont::render( l_graphics, 25, 2, 14 );
      }
      else
      {
        NumericFont::render( l_graphics, 25, 2, 15 );
      }

      /* And the timezone. */
      NumericFont::render( l_graphics, 30, 2, abs(l_timezone)/10 );
      NumericFont::render( l_graphics, 35, 2, abs(l_timezone)%10 );      
    }
    else
    {
      /* Otherwise, render the current time, in hours minutes and seconds. */
      rtc_get_datetime( &l_time );

      /* Hours first. */
      NumericFont::render( l_graphics, 10, 2, l_time.hour/10 );
      NumericFont::render( l_graphics, 15, 2, l_time.hour%10 );

      /* Then minutes. */
      NumericFont::render( l_graphics, 22, 2, l_time.min/10 );
      NumericFont::render( l_graphics, 27, 2, l_time.min%10 );

      /* And lastly seconds. */
      NumericFont::render( l_graphics, 34, 2, l_time.sec/10 );
      NumericFont::render( l_graphics, 39, 2, l_time.sec%10 );

      /* Blinking separators next. */
      if ( l_blink )
      {
        l_graphics->pixel( pimoroni::Point( 20, 4 ) );
        l_graphics->pixel( pimoroni::Point( 20, 6 ) );

        l_graphics->pixel( pimoroni::Point( 32, 4 ) );
        l_graphics->pixel( pimoroni::Point( 32, 6 ) );
      }
      l_blink = !l_blink;
    }

    /* If the brightness was adjusted, show the sliding scale on the right. */
    if ( l_adjusted_brightness > 0 )
    {
      for ( l_index = 0; l_index < pimoroni::GalacticUnicorn::HEIGHT; l_index++ )
      {
        if ( l_index <= ( l_base_brightness * pimoroni::GalacticUnicorn::HEIGHT ) )
        {
          l_graphics->pixel( pimoroni::Point( 
                              pimoroni::GalacticUnicorn::WIDTH - 1,
                              pimoroni::GalacticUnicorn::HEIGHT - l_index - 1
                            ) );
        }
      }
      l_adjusted_brightness--;
    }

    /* All drawing is complete - so, we ask the Unicorn to update. */
    l_unicorn->update( l_graphics );

    /* And wait a short while for the next frame. */
    sleep_ms( 500 );
  }

  /* We'll never get here! */
  return 0;
}

/* End of file better_clock.cpp */
