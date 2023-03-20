/*
 * numeric_font.hpp - from the Unicorn C(++) Examples collection
 *
 * This is a lightweight class providing a simple, fixed width numeric font
 * for more predictable rendering on the Unicorn. Characters are all 7x4.
 *
 * On principle, I dislike rolling code into a header file, but for this use
 * case (it's just dropped into tiny, single-file examples) I can stomach it.
 *
 * Copyright (C) 2023 Pete Favelle <ahnlak@ahnlak.com>
 * Released under the MIT License; see LICENSE for details.
 */

/* Gate against multiple inclusion. #pragma once, but standard-compliant. */

#ifndef NUMERIC_FONT_HPP
#define NUMERIC_FONT_HPP


/* Local headers. */

#include "libraries/pico_graphics/pico_graphics.hpp"


/* Constants. */

#define NUMERIC_FONT_WIDTH    4
#define NUMERIC_FONT_HEIGHT   7


/* Class. */

class NumericFont
{
  private:
    static constexpr uint_fast8_t m_font_data[10][4] = {
      { 0x3e,0x41,0x41,0x3e },  // 0
      { 0x00,0x02,0x7f,0x00 },  // 1
      { 0x62,0x51,0x49,0x46 },  // 2
      { 0x21,0x49,0x4d,0x33 },  // 3
      { 0x18,0x16,0x11,0x7f },  // 4
      { 0x4f,0x49,0x49,0x31 },  // 5
      { 0x3c,0x4a,0x49,0x30 },  // 6
      { 0x01,0x61,0x19,0x07 },  // 7
      { 0x36,0x49,0x49,0x36 },  // 8
      { 0x06,0x49,0x29,0x1e }   // 9
    };
  public:
    static void render( pimoroni::PicoGraphics *p_graphics, uint_fast8_t p_x, uint_fast8_t p_y, uint_fast8_t p_digit )
    {
      const uint_fast8_t *l_font_data;
      uint_fast8_t        l_row, l_column;

      /* We only render single digits. */
      if ( p_digit > 9 )
      {
        return;
      }

      /* Select the correct font data then. */
      l_font_data = m_font_data[p_digit];

      /* Work through each column. */
      for( l_column = 0; l_column < NUMERIC_FONT_WIDTH; l_column++ )
      {
        for ( l_row = 0; l_row < NUMERIC_FONT_HEIGHT; l_row++ )
        {
          /* Only draw if the bit is set. */
          if ( l_font_data[l_column] & ( 0x01 << l_row ) )
          {
            p_graphics->pixel( pimoroni::Point( p_x + l_column, p_y + l_row ) );
          }
        }
      }
    }
};


#endif /* NUMERIC_FONT_HPP */

/* End of file numeric_font.hpp */
