
#ifndef	__LCD_TSTAT_H__

#define	__LCD_TSTAT_H__



#include "bitmap.h"
#include "define.h"



#ifndef	TRUE
#define TRUE	1
#endif

#ifndef FALSE
#define FALSE	0
#endif


//#define FORM48X120		0
#define FORM32X64 		0
#define FORM15X30			1


#define CH_HEIGHT													36

#define THERM_METER_POS										5

#define IDLE_LINE1_POS										0
#define IDLE_LINE2_POS										50

#define SETPOINT_POS											108
#define FAN_MODE_POS											SETPOINT_POS+CH_HEIGHT+7
#define SYS_MODE_POS											FAN_MODE_POS+CH_HEIGHT+7

#define MENU_ITEM1      SETPOINT_POS+0
#define MENU_ITEM2      FAN_MODE_POS+0

#define TIME_POS										      SYS_MODE_POS + CH_HEIGHT + 7
#define MENU_ITEM_POS											130
#define MENU_VALUE_POS										SYS_MODE_POS//MENU_ITEM_POS + CH_HEIGHT
#define ICON_POS													274
#define ICON_XPOS 												2





/* The link indicators share the top left corner: the wifi bars, and under them
 * the RS485 send and receive arrows.  Both sit left of FIRST_CH_POS (30), so
 * the column is clear of the big number for its whole height. */
#define WIFI_XPOS						0
#define WIFI_YPOS						0
#define WIFI_XDOTS						26
#define WIFI_YDOTS						26
#define LINK_XDOTS						13
#define LINK_YDOTS						26
#define LINK_TX_XPOS					0
#define LINK_RX_XPOS					(LINK_TX_XPOS + LINK_XDOTS)
#define LINK_YPOS						(WIFI_YPOS + WIFI_YDOTS + 2)

/* The unit sits at the top of the number rather than at its foot.  Both faces
 * ink below the top of their cell, by different amounts, so two constants are
 * needed to put one ink line through all three.  UNIT_YPOS is the cap line of
 * the big digits, nine rows into their 96 dot cell; the degree ring is a raw
 * 14x14 bitmap with no padding so it is drawn straight at that line, while the
 * letter beside it is a 24x36 cell that inks four rows in and so starts four
 * rows above it. */
#define CHLIB_CAP_TOP					9
#define CHSMALL_CAP_TOP					4
#define UNIT_YPOS						(THERM_METER_POS + CHLIB_CAP_TOP)
#define UNIT_TEXT_YPOS					(UNIT_YPOS - CHSMALL_CAP_TOP)

/* A two character unit cannot hang off UNIT_POS the way a one character one
 * does: two 23 dot advances from UNIT_POS - 23 start at x=166, inside the
 * third digit cell, so "%R" sat on top of the hundreds digit.  It starts
 * where the digits end instead, and still finishes six dots clear of the page
 * marks.  The band spans everything any unit can touch -- the degree ring at
 * UNIT_POS - 14 through the right edge of a two character unit -- and is
 * wiped before each draw so that changing unit leaves no tail behind. */
#define UNIT2_POS						(THIRD_CH_POS + 48)
#define UNIT_BAND_XPOS					UNIT2_POS
#define UNIT_BAND_XDOTS					(2 * 23 + 1)
#define UNIT_BAND_YPOS					UNIT_TEXT_YPOS
#define UNIT_BAND_YDOTS					36

/* The value text sits two dots below the cell top so that its ink centres in
 * the box draw_tangle() puts round it: the box runs y-3 to y+40 and the 15x30
 * face inks rows 4 to 28 of its cell, which lands two dots high without it. */
#define VALUE_YOFF						2

/* draw_tangle() starts the frame this far above the row it belongs to.  It
 * was three literal y values in menuIdle.c that happened to agree with the
 * row constants; moving a row would have detached its box silently. */
#define VALUE_BOX_YOFF					3
/* The three state icons across the bottom.  Each is a 4 bit per pixel index
 * map with its own sixteen entry palette -- see tools/icongen.py -- so a 72x45
 * cell costs 1620 bytes instead of the 6480 the literal RGB565 icons cost, and
 * ten states fit in less flash than three of the old ones did.  The height is
 * still 45 so the cells clear exactly where the old row did. */
#define ICON3_XDOTS						72
#define ICON3_YDOTS						45
#define ICON3_BYTES						(ICON3_XDOTS * ICON3_YDOTS / 2)
#define ICON3_FAN_POS						6
#define ICON3_MODE_POS						84
#define ICON3_WALL_POS						162

/* Which state each icon shows comes from a VAR, so a Control Basic program or
 * T3000 drives them with no protocol change.  These sit just past the paged
 * rows (PAGE_MARK_MAX * IDLE_PAGE_ROWS = 24), so a page can never reach them.
 * A digital VAR contributes its control bit; an analogue one its value/1000.
 * Anything out of range reads as state 0.
 *
 *   VAR25  circulation fan   0 off, 1 on, 2 auto
 *   VAR26  call             0 idle, 1 heat, 2 cool, 3 heat override, 4 cool override
 *   VAR27  sidewalls        0 down, 1 up                                          */
#define ICON3_FAN_VAR						24
#define ICON3_MODE_VAR						25
#define ICON3_WALL_VAR						26
#define ICON3_FAN_STATES					3
#define ICON3_MODE_STATES					5
#define ICON3_WALL_STATES					2


#define THERM_METER_XPOS									30
#define TEMP_FIRST_BLANK						      0//30  //+= blank width
#define FIRST_CH_POS											TEMP_FIRST_BLANK + THERM_METER_XPOS
#define SECOND_CH_POS											FIRST_CH_POS+48
#define THIRD_CH_POS											SECOND_CH_POS+48
#define UNIT_POS													THIRD_CH_POS + 48+ 15
#define BUTTON_DARK_COLOR   							0X0BA7
#define BTN_OFFSET												CH_HEIGHT+7

/* Page indicator. The three value rows run to x=222 and the frame drawn round
 * them by draw_tangle() ends at x=224, which leaves the strip from x=227 to the
 * right edge free the whole height of the rows. One mark per page goes there. */
/* The corner humidity readout: the value and its percent sign on two 12 dot
 * lines under the RS485 arrows, in the strip left of the big number.  Thirty
 * dots is two cells, so three characters cannot go across and the sign sits on
 * its own line.  TOP_RH_VAR follows the three icon VARs, and like them carries
 * whole units in value/1000.  Page 1 only; the other pages blank the strip. */
#define TOP_RH_VAR						27
#define RH_XPOS							3
#define RH_YPOS							58
#define RH_UNIT_XPOS					9
#define RH_UNIT_YPOS					(RH_YPOS + LABEL_CH_YDOTS)
#define RH_XDOTS						30
#define RH_YDOTS						(RH_UNIT_YPOS + LABEL_CH_YDOTS - RH_YPOS)

/* The page indicator: a column of marks in the top right corner, one per page.
 * It runs from PAGE_MARK_YPOS down, and the strip has to clear both the unit,
 * whose 24 dot cell ends at UNIT_POS + 23, and the first value box, whose frame
 * starts at SETPOINT_POS - 3.  Eight marks at a pitch of 12 end at row 97,
 * seven rows clear of that box. */
#define PAGE_MARK_MAX					8
#define PAGE_MARK_XPOS					227
#define PAGE_MARK_YPOS					5
#define PAGE_MARK_XDOTS					9
#define PAGE_MARK_YDOTS					9
#define PAGE_MARK_PITCH					12
#define PAGE_MARK_STRIP_YDOTS			(PAGE_MARK_MAX * PAGE_MARK_PITCH)
/* Row labels. Str_variable_point.label is nine bytes, so eight characters is
 * the whole of it. draw_tangle() puts the value box at x=102, which leaves
 * x=0..101 for the label: eight 12 dot cells from x=2 end at x=97, clear of it.
 * The glyphs are 24 tall against a 36 tall row, so they drop by 6 to sit level
 * with the value beside them. */
/* Glyph storage. A pixel is a coverage level of <table>_BPP bits, packed least
 * significant field first and straight across the row boundaries; disp_ch()
 * turns the level into a colour through a palette interpolated between dcolor
 * and bgcolor. The big number carries 4 bits because it is the text the eye
 * goes to and it is only twelve glyphs; the rest carry 2, which is where
 * nearly all of the benefit already is.
 *
 * Two invariants. Each table's <t>_BYTES must stay w*h*bpp/8, and a renderer
 * must write exactly cp*pp pixels between LCD_SetPos() calls -- write fewer
 * and the panel write desyncs, which corrupts the whole screen rather than
 * one glyph. tools/fontgen.py generates the arrays to match these. */
#define GLYPH_MAX_LEVELS		16		/* the 4 bpp palette; the widest in use */

#define CHLIB_BPP				4
#define CHLIB_XDOTS			48
#define CHLIB_YDOTS			96
#define CHLIB_BYTES			(CHLIB_XDOTS * CHLIB_YDOTS * CHLIB_BPP / 8)

#define CHSMALL_BPP			2
#define CHSMALL_XDOTS			24
#define CHSMALL_YDOTS			36
#define CHSMALL_BYTES			(CHSMALL_XDOTS * CHSMALL_YDOTS * CHSMALL_BPP / 8)

#define CH16_BPP				2
#define CH16_XDOTS				16
#define CH16_YDOTS				24
#define CH16_BYTES				(CH16_XDOTS * CH16_YDOTS * CH16_BPP / 8)

#define LABEL_CH_BPP			2

/* The value box. Temperatures are whole numbers of at most three digits and
 * the words that share the box are four letters, so the box holds four
 * characters rather than five. Its right edge stays beside the page marks and
 * the left edge moves in, which widens the gap to the label rather than
 * leaving a hole on the right. disp_str() advances 23 per character. */
#define VALUE_CHARS			4
#define VALUE_ADV				23
#define VALUE_BOX_W			(VALUE_CHARS * VALUE_ADV + 8)
#define VALUE_BOX_XPOS		(234 - VALUE_BOX_W)
#define VALUE_XPOS			(VALUE_BOX_XPOS + 4)

/* The clock line: "Sep 20 | 12:00 PM" in the 12 dot face. Seventeen characters
 * is 204 dots, so it is centred with a margin either side; the old face could
 * only fit ten characters across the screen. */
#define CLOCK_CHARS			17
#define CLOCK_XPOS			((240 - CLOCK_CHARS * LABEL_CH_XDOTS) / 2)
#define CLOCK_YOFF			6

#define LABEL_CHARS			8
#define LABEL_XPOS				2
#define LABEL_CH_XDOTS			12
#define LABEL_CH_YDOTS			24
#define LABEL_CH_BYTES			(LABEL_CH_XDOTS * LABEL_CH_YDOTS * LABEL_CH_BPP / 8)
#define LABEL_YOFF				6

#define PAGE_MARK_DIM_COLOR			0x29c9	/* #2E3A48 a page you are not on */


#define TOP_AREA_DISP_ITEM_TEMPERATURE   	0
#define TOP_AREA_DISP_ITEM_HUM					 	1
#define TOP_AREA_DISP_ITEM_CO2				   	2
//#define TOP_AREA_DISP_ITEM_OFFON					3

#define TOP_AREA_DISP_UNIT_C   					 	0
#define TOP_AREA_DISP_UNIT_F					 	 	1
#define TOP_AREA_DISP_UNIT_PPM				   	2
#define TOP_AREA_DISP_UNIT_PERCENT			 	3
#define TOP_AREA_DISP_UNIT_Pa			 				4
#define TOP_AREA_DISP_UNIT_kPa						5
#define TOP_AREA_DISP_UNIT_RH							6


#define TOP_AREA_DISP_UNIT_NONE			 			100


/* Screen palette, RGB565.

   The screen used to be white on a light teal (0x7E19).  It is now white on a
   near black blue, which reads better in a plant room and stops the backlight
   lighting up a dark space.

   The icons cannot follow a constant: disp_icon() blits their pixels literally,
   so each one carries the background it was drawn against.  They were
   recomposited onto this background by tools/recolour_icons.py -- change the
   background here and that has to be run again, from a checkout whose icons
   still hold the old one. */
#define TSTAT8_CH_COLOR   	0xffff	/* white, the only ink colour */
#define TSTAT8_MENU_COLOR   0x1106	/* #172230 panel behind a value */
#define SCH_COLOR  			0xffff
#define SCH_BACK_COLOR  	0x3bef	/* unused */
#define TSTAT8_BACK_COLOR1  0x220b	/* #22435E row picked with the LEFT key;
									   has to sit lighter than the background */
#define TSTAT8_BACK_COLOR   0x0083	/* #001018 the screen itself */
#define TSTAT8_MENU_COLOR2  0x1106
#define TANGLE_COLOR        0xbe9c	/* frame round each value; left light on
									   purpose, and draw_tangle()'s corner
									   bitmaps carry this same colour */

#define FAN_OFF 	0
#define FAN_AUTO 	4
#define FAN_ON		1
#define FAN_SPEED1 1
#define FAN_SPEED2 2
#define FAN_SPEED3 3

#define SCH_XPOS  10

#define HC_CFG_AUTO			0
#define HC_CFG_COOL			1
#define HC_CFG_HEAT			2


#define	LONG_PRESS_TIMER_SPEED_100	200
#define	LONG_PRESS_TIMER_SPEED_50	100
#define	LONG_PRESS_TIMER_SPEED_10	30
#define	LONG_PRESS_TIMER_SPEED_1	20


#define KEY_SPEED_1			(0x0000)
#define KEY_SPEED_10		(0x0100)
#define KEY_SPEED_50		(0x0200)
#define KEY_SPEED_100		(0x0300)
#define KEY_SPEED_MASK		(0x00ff)
#define KEY_FUNCTION_MASK	(0xff00)

#define	KEY_UP_MASK			2//(1 << 1)
#define	KEY_DOWN_MASK		4//(1 << 2)
#define	KEY_LEFT_MASK		8//(1 << 3)
#define	KEY_RIGHT_MASK		1//(1 << 0)
#define	KEY_LEFT_RIGHT_MASK		9//(1 << 0)

#define SMALL_SIZE_HIGH  24

#define TSTAT10_SCH_DAY_X   80
#define TSTAT10_SCH_DAY_Y   24//0



#define TSTAT10_SCH_ON1_X    0
#define TSTAT10_SCH_ON1_Y    24*2
#define TSTAT10_SCH_OFF1_X   0
#define TSTAT10_SCH_OFF1_Y   (24*3)

#define TSTAT10_SCH_ON2_X    0
#define TSTAT10_SCH_ON2_Y    (24*4)
#define TSTAT10_SCH_OFF2_X   0
#define TSTAT10_SCH_OFF2_Y   (24*5)

#define TSTAT10_SCH_ON3_X    0
#define TSTAT10_SCH_ON3_Y    (24*6)
#define TSTAT10_SCH_OFF3_X   0
#define TSTAT10_SCH_OFF3_Y   (24*7)

#define TSTAT10_SCH_ON4_X    0
#define TSTAT10_SCH_ON4_Y    (24*8)
#define TSTAT10_SCH_OFF4_X   0
#define TSTAT10_SCH_OFF4_Y   (24*9)


#define TSTAT10_SCH_ON1_TIME_X   100
#define TSTAT10_SCH_ON1_TIME_Y   TSTAT10_SCH_ON1_Y
#define TSTAT10_SCH_OFF1_TIME_X   100
#define TSTAT10_SCH_OFF1_TIME_Y   TSTAT10_SCH_OFF1_Y

#define TSTAT10_SCH_ON2_TIME_X   100
#define TSTAT10_SCH_ON2_TIME_Y   TSTAT10_SCH_ON2_Y
#define TSTAT10_SCH_OFF2_TIME_X   100
#define TSTAT10_SCH_OFF2_TIME_Y   TSTAT10_SCH_OFF2_Y

#define TSTAT10_SCH_ON3_TIME_X   100
#define TSTAT10_SCH_ON3_TIME_Y   TSTAT10_SCH_ON3_Y
#define TSTAT10_SCH_OFF3_TIME_X   100
#define TSTAT10_SCH_OFF3_TIME_Y   TSTAT10_SCH_OFF3_Y

#define TSTAT10_SCH_ON4_TIME_X   100
#define TSTAT10_SCH_ON4_TIME_Y   TSTAT10_SCH_ON4_Y
#define TSTAT10_SCH_OFF4_TIME_X   100
#define TSTAT10_SCH_OFF4_TIME_Y   TSTAT10_SCH_OFF4_Y


typedef struct my_point
{
	unsigned short x_pos;
	unsigned short y_pos;
}str_my_point;



void vStartKeyTasks( unsigned char uxPriority);
void vStartMenuTask(unsigned char uxPriority);


#ifndef TSTAT7_ARM

#define MAX_SCOROLL 20//16

extern uint8 *scroll;
extern uint8 scroll_ram[5][MAX_SCOROLL];
extern uint8 fan_flag;
extern uint8 display_flag;
extern uint8 schedule_hour_minute; //indicate current display item is "hour" or "minute"
extern uint8 blink_parameter;
extern uint8 clock_blink_flag;
void LCD_Intial(void);
extern uint8 const chlib[];
extern uint8 const chlibsmall[];
extern uint8 const char_16_24[];
extern uint8 const char_12_24[];
extern uint16 const athome[];
extern uint16 const offhome[];
extern uint16 const sunicon[];
extern uint16 const moonicon[];
extern uint16 const heaticon[]; 
extern uint16 const coolicon[];
extern uint16 const fanspeed0a[];
extern uint16 const fanspeed1a[];
extern uint16 const fanspeed2a[];
extern uint16 const fanspeed3a[];
extern uint16 const fanbladeA[];
extern uint16 const fanbladeB[];

extern uint16 const degree_o[];
//extern uint16 const therm_meter[];
extern uint16 const leftup[];
extern uint16 const leftdown[];
extern uint16 const rightdown[];
extern uint16 const rightup[];
extern uint16 const cmnct_send[]; 
extern uint16 const cmnct_rcv[]; 
extern uint16 const wifi_0[];
extern uint16 const wifi_1[];
extern uint16 const wifi_2[];
extern uint16 const wifi_3[];
extern uint16 const wifi_4[];
extern uint16 const wifi_none[];

typedef struct   
{
 uint8 unit;
 uint8 setpoint;
 uint8 fan;
 uint8 sysmode;
 uint8 occ_unocc;
 uint8 heatcool;
 uint8 fanspeed;
 uint8 cmnct_send;
 uint8 cmnct_rcv; 	
} DISP_CHANGE; 
extern DISP_CHANGE icon;
//extern uint16 const angle[];
void draw_tangle(uint8 xpos, uint16 ypos, uint8 w);
void ClearScreen(unsigned int bColor);
void disp_ch(uint8 form, uint16 x, uint16 y,uint8 value,uint16 dcolor,uint16 bgcolor);		
void disp_icon(uint16 cp, uint16 pp, uint16 const *icon_name, uint16 x,uint16 y,uint16 dcolor, uint16 bgcolor);
void disp_null_icon(uint16 cp, uint16 pp, uint16 const *icon_name, uint16 x,uint16 y,uint16 dcolor, uint16 bgcolor);
void disp_str(uint8 form, uint16 x,uint16 y,uint8 *str,uint16 dcolor,uint16 bgcolor);	
void disp_str_16_24(uint8 form, uint16 x, uint16 y, uint8 *str, uint16 dcolor, uint16 bgcolor);
void disp_ch_12_24(uint16 x, uint16 y, uint8 value, uint16 dcolor, uint16 bgcolor);
void disp_str_12_24(uint16 x, uint16 y, uint8 *str, uint16 dcolor, uint16 bgcolor);
void display_SP(int16 setpoint);
void display_screen_value(uint8 type);
void display_screen_value_var(uint8 type, uint8 var_index);
void display_page_marks(uint8 current, uint8 count);
void display_clock(void);
void display_top_rh(uint8 page);
void display_fanspeed(int16 speed);
void display_mode(uint8 heat_cool_user);
void display_icon(void);
void disp_icon4(uint16 cp, uint16 pp, uint8 const *bits, uint16 const *pal, uint16 x, uint16 y);
void display_value(uint16 pos,int16 disp_value, uint8 disp_unit);
//void display_menu(uint16 pos, uint8 *item);
void display_menu (uint8 *item1, uint8 *item2);
void clear_line(uint8 linenum);
void clear_lines(void);
//void display_clock_date(int8 item, int16 value);
//void display_clock_time(int8 item, int16 value);
void display_scroll(void);
void scroll_warning(uint8 item);
void display_schedule_time(int8 schedule_time_sel, uint8 hour_minute);
void Top_area_display(uint8 item, int16 value, uint8 unit);

#endif //TSTAT7_ARM

#endif

