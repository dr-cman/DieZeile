#define FONT_SMALL_4x7         179   
#define FONT_TALL_4x8        159   
#define FONT_BOLD_5x8         148   
#define FONT_NORMAL_5x7       48           // standard ASCII offset    

#define CHAR_TIMER_LOGO       176
#define CHAR_HOURGLASS        174
#define CHAR_SPACE_4COLS      175
#define CHAR_SPACE_10COLS     178
#define CHAR_SPACE_22COLS     177
#define CHAR_DOT_1x8          171
#define CHAR_DOT_1x7          172
#define CHAR_ARROW_UP_5x7     24
#define CHAR_ARROW_DOWN_5x7   25
#define CHAR_COLON_2X8        147
#define CHAR_COLON_3X8        158
#define CHAR_COLON_5X7        173

const uint8_t charset[] PROGMEM = {
  0,                                                        // 0 - 'Empty Cell'
  5, 0x3e, 0x5b, 0x4f, 0x5b, 0x3e,                          // 1 - 'Sad Smiley'
  5, 0x3e, 0x6b, 0x4f, 0x6b, 0x3e,                          // 2 - 'Happy Smiley'
  5, 0x1c, 0x3e, 0x7c, 0x3e, 0x1c,                          // 3 - 'Heart'
  5, 0x18, 0x3c, 0x7e, 0x3c, 0x18,                          // 4 - 'Diamond'
  5, 0x1c, 0x57, 0x7d, 0x57, 0x1c,                          // 5 - 'Clubs'
  5, 0x1c, 0x5e, 0x7f, 0x5e, 0x1c,                          // 6 - 'Spades'
  4, 0x00, 0x18, 0x3c, 0x18,                                // 7 - 'Bullet Point'
  5, 0xff, 0xe7, 0xc3, 0xe7, 0xff,                          // 8 - 'Rev Bullet Point'
  4, 0x00, 0x18, 0x24, 0x18,                                // 9 - 'Hollow Bullet Point'
  5, 0xff, 0xe7, 0xdb, 0xe7, 0xff,                          // 10 - 'Rev Hollow BP'
  5, 0x30, 0x48, 0x3a, 0x06, 0x0e,                          // 11 - 'Male'
  5, 0x26, 0x29, 0x79, 0x29, 0x26,                          // 12 - 'Female'
  5, 0x40, 0x7f, 0x05, 0x05, 0x07,                          // 13 - 'Music Note 1'
  5, 0x40, 0x7f, 0x05, 0x25, 0x3f,                          // 14 - 'Music Note 2'
  5, 0x5a, 0x3c, 0xe7, 0x3c, 0x5a,                          // 15 - 'Snowflake'
  5, 0x7f, 0x3e, 0x1c, 0x1c, 0x08,                          // 16 - 'Right Pointer'
  5, 0x08, 0x1c, 0x1c, 0x3e, 0x7f,                          // 17 - 'Left Pointer'
  5, 0x14, 0x22, 0x7f, 0x22, 0x14,                          // 18 - 'UpDown Arrows'
  5, 0x5f, 0x5f, 0x00, 0x5f, 0x5f,                          // 19 - 'Double Exclamation'
  5, 0x22, 0x4d, 0x55, 0x59, 0x22,                          // 20 - 'Paragraph Mark'
  4, 0x66, 0x89, 0x95, 0x6a,                                // 21 - 'Section Mark'
  5, 0x60, 0x60, 0x60, 0x60, 0x60,                          // 22 - 'Double Underline'
  5, 0x94, 0xa2, 0xff, 0xa2, 0x94,                          // 23 - 'UpDown Underlined'
  5, 0x08, 0x04, 0x7e, 0x04, 0x08,                          // 24 - 'Up Arrow'
  5, 0x10, 0x20, 0x7e, 0x20, 0x10,                          // 25 - 'Down Arrow'
  5, 0x08, 0x08, 0x2a, 0x1c, 0x08,                          // 26 - 'Right Arrow'
  5, 0x08, 0x1c, 0x2a, 0x08, 0x08,                          // 27 - 'Left Arrow'
  5, 0x1e, 0x10, 0x10, 0x10, 0x10,                          // 28 - 'Angled'
  5, 0x0c, 0x1e, 0x0c, 0x1e, 0x0c,                          // 29 - 'Squashed #'
  5, 0x30, 0x38, 0x3e, 0x38, 0x30,                          // 30 - 'Up Pointer'
  5, 0x06, 0x0e, 0x3e, 0x0e, 0x06,                          // 31 - 'Down Pointer'
  5, 0x00, 0x00, 0x00, 0x00, 0x00,                          // 32 - SPACE
  5, 0x00, 0x00, 0x5F, 0x00, 0x00,                          // 33 - !
  5, 0x00, 0x03, 0x00, 0x03, 0x00,                          // 34 - "
  5, 0x14, 0x3E, 0x14, 0x3E, 0x14,                          // 35 - #
  5, 0x24, 0x2A, 0x7F, 0x2A, 0x12,                          // 36 - $
  5, 0x43, 0x33, 0x08, 0x66, 0x61,                          // 37 - %
  5, 0x36, 0x49, 0x55, 0x22, 0x50,                          // 38 - &
  5, 0x00, 0x05, 0x03, 0x00, 0x00,                          // 39 - '
  5, 0x00, 0x1C, 0x22, 0x41, 0x00,                          // 40 - (
  5, 0x00, 0x41, 0x22, 0x1C, 0x00,                          // 41 - )
  5, 0x14, 0x08, 0x3E, 0x08, 0x14,                          // 42 - *
  5, 0x08, 0x08, 0x3E, 0x08, 0x08,                          // 43 - +
  5, 0x00, 0x50, 0x30, 0x00, 0x00,                          // 44 - ,
  5, 0x08, 0x08, 0x08, 0x08, 0x08,                          // 45 - -
  5, 0x00, 0x00, 0x60, 0x60, 0x00,                          // 46 - .
  5, 0x20, 0x10, 0x08, 0x04, 0x02,                          // 47 - /
  5, 0x3E, 0x51, 0x49, 0x45, 0x3E,                          // 48 - 0
  5, 0x04, 0x02, 0x7F, 0x00, 0x00,                          // 49 - 1
  5, 0x42, 0x61, 0x51, 0x49, 0x46,                          // 50 - 2
  5, 0x22, 0x41, 0x49, 0x49, 0x36,                          // 51 - 3
  5, 0x18, 0x14, 0x12, 0x7F, 0x10,                          // 52 - 4
  5, 0x27, 0x45, 0x45, 0x45, 0x39,                          // 53 - 5
  5, 0x3E, 0x49, 0x49, 0x49, 0x32,                          // 54 - 6
  5, 0x01, 0x01, 0x71, 0x09, 0x07,                          // 55 - 7
  5, 0x36, 0x49, 0x49, 0x49, 0x36,                          // 56 - 8
  5, 0x26, 0x49, 0x49, 0x49, 0x3E,                          // 57 - 9
  5, 0x00, 0x00, 0x36, 0x36, 0x00,                          // 58 - :
  5, 0x00, 0x56, 0x36, 0x00, 0x00,                          // 59 - ;
  5, 0x08, 0x14, 0x22, 0x41, 0x00,                          // 60 - <
  5, 0x14, 0x14, 0x14, 0x14, 0x14,                          // 61 - = 
  5, 0x00, 0x41, 0x22, 0x14, 0x08,                          // 62 - >
  5, 0x02, 0x01, 0x51, 0x09, 0x06,                          // 63 - ?
  5, 0x3E, 0x41, 0x59, 0x55, 0x5E,                          // 64 - @
  4, B01111110, B00010001, B00010001, B01111110,            // 65 - A
  4, B01111111, B01001001, B01001001, B00110110,            // 66 - B
  4, B00111110, B01000001, B01000001, B00100010,            // 67 - C
  4, B01111111, B01000001, B01000001, B00111110,            // 68 - D
  4, B01111111, B01001001, B01001001, B01000001,            // 69 - E
  4, B01111111, B00001001, B00001001, B00000001,            // 70 - F
  4, B00111110, B01000001, B01001001, B01111010,            // 71 - G
  4, B01111111, B00001000, B00001000, B01111111,            // 72 - H
  3, B01000001, B01111111, B01000001,                       // 73 - I
  4, B00110000, B01000000, B01000001, B00111111,            // 74 - J
  4, B01111111, B00001000, B00010100, B01100011,            // 75 - K
  4, B01111111, B01000000, B01000000, B01000000,            // 76 - L
  5, B01111111, B00000010, B00001100, B00000010, B01111111, // 77 - M
  5, B01111111, B00000100, B00001000, B00010000, B01111111, // 78 - N
  4, B00111110, B01000001, B01000001, B00111110,            // 79 - O
  4, B01111111, B00001001, B00001001, B00000110,            // 80 - P
  4, B00111110, B01000001, B01000001, B10111110,            // 81 - Q
  4, B01111111, B00001001, B00001001, B01110110,            // 82 - R
  4, B01000110, B01001001, B01001001, B00110010,            // 83 - S
  5, B00000001, B00000001, B01111111, B00000001, B00000001, // 84 - T
  4, B00111111, B01000000, B01000000, B00111111,            // 85 - U
  5, B00001111, B00110000, B01000000, B00110000, B00001111, // 86 - V
  5, B00111111, B01000000, B00111000, B01000000, B00111111, // 87 - W
  5, B01100011, B00010100, B00001000, B00010100, B01100011, // 88 - X
  5, B00000111, B00001000, B01110000, B00001000, B00000111, // 89 - Y
  4, B01100001, B01010001, B01001001, B01000111,            // 90 - Z
  2, B01111111, B01000001,                                  // 91 - [
  4, B00000001, B00000110, B00011000, B01100000,            // 92 - \ backslash
  2, B01000001, B01111111,                                  // 93 - ]
  3, B00000010, B00000001, B00000010,                       // 94 - hat
  4, B01000000, B01000000, B01000000, B01000000,            // 95 - _
  2, B00000001, B00000010,                                  // 96 - `
  4, B00100000, B01010100, B01010100, B01111000,            // 97 - a
  4, B01111111, B01000100, B01000100, B00111000,            // 98 - b
  4, B00111000, B01000100, B01000100, B00101000,            // 99 - c
  4, B00111000, B01000100, B01000100, B01111111,            // 100 - d
  4, B00111000, B01010100, B01010100, B00011000,            // 101 - e
  3, B00000100, B01111110, B00000101,                       // 102 - f
  4, B10011000, B10100100, B10100100, B01111000,            // 103 - g
  4, B01111111, B00000100, B00000100, B01111000,            // 104 - h
  3, B01000100, B01111101, B01000000,                       // 105 - i
  4, B01000000, B10000000, B10000100, B01111101,            // 106 - j
  4, B01111111, B00010000, B00101000, B01000100,            // 107 - k
  3, B01000001, B01111111, B01000000,                       // 108 - l
  5, B01111100, B00000100, B01111100, B00000100, B01111000, // 109 - m
  4, B01111100, B00000100, B00000100, B01111000,            // 110 - n
  4, B00111000, B01000100, B01000100, B00111000,            // 111 - o
  4, B11111100, B00100100, B00100100, B00011000,            // 112 - p
  4, B00011000, B00100100, B00100100, B11111100,            // 113 - q
  4, B01111100, B00001000, B00000100, B00000100,            // 114 - r
  4, B01001000, B01010100, B01010100, B00100100,            // 115 - s
  3, B00000100, B00111111, B01000100,                       // 116 - t
  4, B00111100, B01000000, B01000000, B01111100,            // 117 - u
  5, B00011100, B00100000, B01000000, B00100000, B00011100, // 118 - v
  5, B00111100, B01000000, B00111100, B01000000, B00111100, // 119 - w
  5, B01000100, B00101000, B00010000, B00101000, B01000100, // 120 - x
  4, B10011100, B10100000, B10100000, B01111100,            // 121 - y
  3, B01100100, B01010100, B01001100,                       // 122 - z
  3, B00001000, B00110110, B01000001,                       // 123 - {
  1, B01111111,                                             // 124 - |
  3, B01000001, B00110110, B00001000,                       // 125 - }
  4, B00001000, B00000100, B00001000, B00000100,            // 126 - ~
  5, B10101010, B01010101, B10101010, B01010101, B10101010, // 127 - DEL unused -> Chess symbol
//4, B00100000, B01010101, B01010100, B01111001,            // 128 - a umlaut (ä)  
  5, B00100000, B01010101, B01010100, B01010101, B00111000, // 128 - a umlaut (ä)  
//5, B00111000, B01000110, B01000100, B01000110, B00111000, // 129 - o umlaut (ö)
  5, B00111000, B01000101, B01000100, B01000101, B00111000, // 129 - o umlaut (ö)
  4, B00111101, B01000000, B01000000, B01111101,            // 130 - u umlaut (ü)  
  4, 0x7d, 0x12, 0x12, 0x7d,                                // 131 - A umlaut (Ä)
  4, 0x3d, 0x42, 0x42, 0x3d,                                // 132 - O umlaut (Ö)
  5, 0x3e, 0x41, 0x40, 0x41, 0x3e,                          // 133 - U umlaut (Ü)
  4, 0x7c, 0x2a, 0x2a, 0x14,                                // 134 - Eszett (sharp S)
  5, 0x1c, 0x2a, 0x49, 0x49, 0x22,                          // 135 - Euro sign
  4, 0x48, 0x7e, 0x49, 0x4a,                                // 136 - Pound sign
  5, B10101010, B01010101, B10101010, B01010101, B10101010, // 137 - Chess symbol
  2, 0x60, 0x60,                                            // 138 - small point
  1, B10000000,                                             // 139 - partial column 1/8
  1, B11000000,                                             // 140 - partial column 2/8
  1, B11100000,                                             // 141 - partial column 3/8
  1, B11110000,                                             // 142 - partial column 4/8
  1, B11111000,                                             // 143 - partial column 5/8
  1, B11111100,                                             // 144 - partial column 6/8
  1, B11111110,                                             // 145 - partial column 7/8
  1, B11111111,                                             // 146 - partial column 8/8
  2, 0x36, 0x36,                                            // 147 - small :

  // clock digits in bold font (5 pixels width)
  5, B11111111, B11111111, B11000011, B11111111, B11111111, // 148 - 0 (Bold)
  5, B00000000, B00000000, B00000000, B11111111, B11111111, // 149 - 1 (Bold)
  5, B11111011, B11111011, B11011011, B11011111, B11011111, // 150 - 2 (Bold)
  5, B11000011, B11011011, B11011011, B11111111, B11111111, // 151 - 3 (Bold)
  5, B00011111, B00011111, B00011000, B11111111, B11111111, // 152 - 4 (Bold)
  5, B11011111, B11011111, B11011011, B11111011, B11111011, // 153 - 5 (Bold)
  5, B11111111, B11111111, B11011011, B11111011, B11111011, // 154 - 6 (Bold)
  5, B00000011, B00000011, B00000011, B11111111, B11111111, // 155 - 7 (Bold)
  5, B11111111, B11111111, B11011011, B11111111, B11111111, // 156 - 8 (Bold)
  5, B11011111, B11011111, B11011011, B11111111, B11111111, // 157 - 9 (Bold)
  3, B11101110, B11101110, B11101110,                       // 158 - : (Bold)
  
  // clock digits in small serif font (3 pixels width, 8 pixels high)
  3, B11111111, B10000001, B11111111,                       // 159 - 0 (Small)
  3, B00000000, B11111111, B00000000,                       // 160 - 1 (Small)
  3, B11111001, B10001001, B11001111,                       // 161 - 2 (Small)
  3, B11000001, B10001001, B11111111,                       // 162 - 3 (Small)
  3, B00001111, B00001000, B11111111,                       // 163 - 4 (Small)
  3, B11001111, B10001001, B11111001,                       // 164 - 5 (Small)
  3, B11111111, B10001001, B11111011,                       // 165 - 6 (Small)
  3, B00000011, B00000001, B11111111,                       // 166 - 7 (Small)
  3, B11111111, B10001001, B11111111,                       // 167 - 8 (Small)
  3, B11001111, B10001001, B11111111,                       // 168 - 9 (Small)
  1, B00100100,                                             // 169 - : (Small)
  3, B00000000, B00000000, B00000000,                       // 170 - space (Small)
  1, B10000000,                                             // 171 - . (Small)
  1, B01000000,                                             // 172 - . (Small) alternative

  // some special characters
  6, B00000000, B00000000, B00110110, B00110110, B00000000, B00000000,  // 173 - : alternative
  5, B11000011, B11100101, B11011101, B11100101, B11000011,             // 174 - hourglas symbol
  4, B00000000, B00000000, B00000000, B00000000,                        // 175 - space with 4 columns

  // timer logo                                             // 176 TiMER
 19, B00000001, B00011111, B00000001,                       // T
     B00000000,                                             //
     B00011101,                                             // i          
     B00000000,                                             //
     B00011111, B00000010, B00000100, B00000010, B00011111, // M
     B00000000,                                             // 
     B00011111, B00010101, B00010001,                       // E
     B00000000,                                             //
     B00011111, B00000101, B00011010,                       // R

  // some special spaces
 22, B00000000, B00000000, B00000000, B00000000, B00000000, // 177 - space 22 cols wide
     B00000000, B00000000, B00000000, B00000000, B00000000, 
     B00000000, B00000000, B00000000, B00000000, B00000000, 
     B00000000, B00000000, B00000000, B00000000, B00000000, 
     B00000000, B00000000,  
 10, B00000000, B00000000, B00000000, B00000000, B00000000, // 178 - space 10 cols wide
     B00000000, B00000000, B00000000, B00000000, B00000000, 

  // clock digits in small font (3 pixels width, 7 pixels high)
  3, B01111111, B01000001, B01111111,                       // 179 - 0 (Small)
  3, B00000000, B01111111, B00000000,                       // 180 - 1 (Small)
  3, B01111001, B01001001, B01001111,                       // 181 - 2 (Small)
  3, B01000001, B01001001, B01111111,                       // 182 - 3 (Small)
  3, B00001111, B00001000, B01111111,                       // 183 - 4 (Small)
  3, B01001111, B01001001, B01111001,                       // 184 - 5 (Small)
  3, B01111111, B01001001, B01111001,                       // 185 - 6 (Small)
  3, B00000001, B00000001, B01111111,                       // 186 - 7 (Small)
  3, B01111111, B01001001, B01111111,                       // 187 - 8 (Small)
  3, B01001111, B01001001, B01111111,                       // 188 - 9 (Small)
  1, B00100100,                                             // 189 - : (Small)
  3, B00000000, B00000000, B00000000,                       // 180 - space (Small)
  1, B01000000,                                             // 181 - . (Small)
  1, B01000000,                                             // 182 - . (Small) alternative

};
