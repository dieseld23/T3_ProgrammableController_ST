#include "main.h"
#include "define.h"
#include "LCD_TSTAT.h"
#include "menu.h"
#include "wifi.h"
#define	NODES_POLL_PERIOD	30

char UI_DIS_LINE1[LABEL_CHARS + 1]; //Corresponds to the old setpoint, fan and sys
char UI_DIS_LINE2[LABEL_CHARS + 1];
char UI_DIS_LINE3[LABEL_CHARS + 1];
char UI_DIS_TOP[10];

static uint8 display_around_time_ctr = NODES_POLL_PERIOD;
static uint8 disp_index = 0;
static uint8 set_msv = 0;
static uint8 warming_state = TRUE;
static uint8 force_refresh = TRUE;
uint8 flag_left_key = 0;
uint8	count_left_key = 0;
uint8 flag_digital_top_area = 0;
uint8 digital_top_area_type = 0;
uint8 digital_top_area_num = 0;
uint8 digital_top_area_changed = 0;
void set_output_raw(uint8_t point,uint16_t value);
extern uint16_t count_suspend_mstp;
/* ---- idle-screen pages ---------------------------------------------------
 * Page 0 is the screen as it always was: VAR1-VAR3 on the three rows. Each
 * further page shows the next three VARs, so a program can drive
 * PAGE_MARK_MAX * 3 values instead of three. The RIGHT key steps through the
 * pages -- it did nothing on this screen before -- while LEFT still picks a
 * row within the page and LEFT+RIGHT together still opens the menu.
 *
 * Pages past the last VAR that carries a label are left out, so a panel that
 * labels VAR1-VAR6 gets two pages rather than eight.
 */
/* a point_type no drawing branch matches, so nothing is drawn */
#define TOP_AREA_NO_POINT	0xff

#define IDLE_PAGE_ROWS		3
static uint8 page_index = 0;

/* First VAR on the page showing: page 0 -> vars[0], page 1 -> vars[3]. */
static uint8 page_var_base(void)
{
	return page_index * IDLE_PAGE_ROWS;
}

/* A VAR earns a row once somebody has given it a label in T3000. 0xff is what
 * erased flash reads back as, so it counts as no label rather than as one. */
static uint8 var_is_labelled(uint8 num)
{
	uint8 first = (uint8)vars[num].label[0];

	return (first != 0) && (first != ' ') && (first != 0xff);
}

/* Copy a VAR label into a row buffer. At most LABEL_CHARS characters; anything
 * outside printable ASCII ends the string, which covers both the NUL that
 * T3000 writes and the 0xff of erased flash. The rest is padded with spaces so
 * that drawing the buffer repaints every cell -- a shorter label can then not
 * leave the tail of a longer one behind it. */
static void load_label(char *dst, uint8 num)
{
	uint8 i, c, ended = 0;

	for(i = 0;i < LABEL_CHARS;i++)
	{
		c = (uint8)vars[num].label[i];
		if(c < ' ' || c > '~')
			ended = 1;
		dst[i] = ended ? ' ' : (char)c;
	}
	dst[LABEL_CHARS] = 0;
}

/* The top area shows one configured point. npoint.number is 1-based and comes
 * from T3000, so 0 underflows to 255, and nothing range checks it against the
 * table it indexes; point_type is unchecked too and can hold any of the
 * MAX_POINT_TYPE kinds rather than the three drawn here. Flash supplies a sane
 * default only when it reads back erased, so a bad pair written over Modbus
 * arrives here untouched.
 *
 * An unusable pair selects no point at all: the type becomes one that none of
 * the drawing branches match, so the top area stays blank, and the index is
 * pinned to 0 so that anything reading it anyway stays inside the table. Blank
 * is the honest thing to show for a point that is not there. */
static void load_top_area_point(uint8 *type, uint8 *num)
{
	uint8 t = Setting_Info.reg.display_lcd.lcd_mod_reg.npoint.point_type;
	uint8 n = (uint8)(Setting_Info.reg.display_lcd.lcd_mod_reg.npoint.number - 1);
	uint8 ok = (t == IN && n < MAX_INS)
			|| (t == OUT && n < MAX_OUTS)
			|| (t == VAR && n < MAX_VARS);

	*type = ok ? t : TOP_AREA_NO_POINT;
	*num = ok ? n : 0;
}

/* label[] is nine bytes and need not carry a NUL, so copying all nine into
 * UI_DIS_TOP and handing it to disp_str_16_24() ran off the end of the buffer
 * until it happened to meet a zero. */
static void load_top_label(const void *label)
{
	memcpy(UI_DIS_TOP, label, 9);
	UI_DIS_TOP[9] = 0;
}

/* Pages to offer: page 0 always, then every page up to the last labelled VAR. */
static uint8 idle_page_count(void)
{
	uint8 page, row, count;

	count = 1;
	for(page = 1;page < PAGE_MARK_MAX;page++)
	{
		for(row = 0;row < IDLE_PAGE_ROWS;row++)
		{
			if(var_is_labelled(page * IDLE_PAGE_ROWS + row))
			{
				count = page + 1;
				break;
			}
		}
	}
	return count;
}

/* Draw the three rows for the page showing, and the marks beside them. The row
 * labels are cached in UI_DIS_LINE1..3, so a page change has to reload them or
 * the rows keep the previous page's words. The values are cleared too: a five
 * character value followed by a shorter one would otherwise leave the tail of
 * the first one on screen. */
static void show_page_rows(void)
{
	uint8 pages = idle_page_count();
	uint8 base;

	// labelling a VAR from T3000 can shrink the page count under our feet
	if(page_index >= pages)
		page_index = 0;
	base = page_var_base();

	disp_str(FORM15X30, VALUE_XPOS,  SETPOINT_POS + VALUE_YOFF, "    ",SCH_COLOR,TSTAT8_MENU_COLOR2);
	disp_str(FORM15X30, VALUE_XPOS,  FAN_MODE_POS + VALUE_YOFF, "    ",SCH_COLOR,TSTAT8_MENU_COLOR2);
	disp_str(FORM15X30, VALUE_XPOS,  SYS_MODE_POS + VALUE_YOFF, "    ",SCH_COLOR,TSTAT8_MENU_COLOR2);

	load_label(UI_DIS_LINE1, base);
	load_label(UI_DIS_LINE2, base + 1);
	load_label(UI_DIS_LINE3, base + 2);

	disp_str_12_24(LABEL_XPOS, SETPOINT_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE1, SCH_COLOR, TSTAT8_BACK_COLOR);
	disp_str_12_24(LABEL_XPOS, FAN_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE2, SCH_COLOR, TSTAT8_BACK_COLOR);
	disp_str_12_24(LABEL_XPOS, SYS_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE3, SCH_COLOR, TSTAT8_BACK_COLOR);

	display_page_marks(page_index, pages);
}

void MenuIdle_init(void)
{
	uint8 i,j;
	
	//LCDtest();
	ClearScreen(TSTAT8_BACK_COLOR);
	page_index = 0;
	flag_digital_top_area = 0;
	digital_top_area_type = 0;
  digital_top_area_num = 0;
	digital_top_area_changed = 0;

	load_top_area_point(&digital_top_area_type, &digital_top_area_num);
						
	memset(UI_DIS_TOP,0,sizeof(UI_DIS_TOP));
	digital_top_area_changed = 0;
	
	disp_str(FORM15X30, SCH_XPOS,  0, "              ",SCH_COLOR,TSTAT8_BACK_COLOR);					
	disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "            ",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
	disp_str(FORM15X30, SCH_XPOS,  CH_HEIGHT, "              ",SCH_COLOR,TSTAT8_BACK_COLOR);
	disp_str(FORM15X30, SCH_XPOS,  CH_HEIGHT * 2 - 7, "              ",SCH_COLOR,TSTAT8_BACK_COLOR);	
	
	if(digital_top_area_type == IN)
	{
		load_top_label(inputs[digital_top_area_num].label);		
	}
	else if(digital_top_area_type == OUT)
		load_top_label(outputs[digital_top_area_num].label);
	else if(digital_top_area_type == VAR)
		load_top_label(vars[digital_top_area_num].label);		

	disp_null_icon(240, 36, 0, 0,TIME_POS,TSTAT8_CH_COLOR, TSTAT8_MENU_COLOR2);
	
  scroll = &scroll_ram[0][0];
//	fanspeedbuf = fan_speed_user;
	
	
	draw_tangle(VALUE_BOX_XPOS,105,VALUE_BOX_W);
	draw_tangle(VALUE_BOX_XPOS,148,VALUE_BOX_W);
	draw_tangle(VALUE_BOX_XPOS,191,VALUE_BOX_W);

	show_page_rows();

	//msv_data[MAX_MSV][STR_MSV_MULTIPLE_COUNT]
	for (i = 0;i < MAX_MSV;i++)
		for (j = 0; j < STR_MSV_MULTIPLE_COUNT;j++)
		{
			if(msv_data[i][j].status == 255)
			{
				msv_data[i][j].status = 0;
			}
		}
//#if ARM_UART_DEBUG
//	uart1_init(115200);
//	DEBUG_EN = 1;
//	printf("IDLE init \r\n");
//#endif	
}

 
void get_data_format(u8 loc,float num,char *s)
{
	u8 i,s_len,s_start,buf_start;
	
	if(loc == 0)
		sprintf(s,"%9.0f",num);
	else if(loc == 1)
		sprintf(s,"%9.1f",num);
	else if(loc == 2)
		sprintf(s,"%9.2f",num);
	else if(loc == 3)
		sprintf(s,"%9.3f",num);
	else if(loc == 4)
		sprintf(s,"%9.4f",num);
	else if(loc == 5)
		sprintf(s,"%9.5f",num);
	else if(loc == 6)
		sprintf(s,"%9.6f",num);
	else
		sprintf(s,"%f",num);
	
	for(i=0;i<9;i++)
	{
		if(s[i]!= 0x20) break;
	}
	s_len = 9 - i;   					//Data length
	s_start = i;     					//Start of the data
	buf_start = i - i / 2; 				//Start position after rearranging
	
	for(i=0;i<s_len;i++) 				//Shift the data left
	{
		s[buf_start + i] = s[s_start + i];
	}
	for(i=buf_start + s_len;i<9;i++ ) 	//Pad with " "
	{
		s[i] = 0x20;
	} 
}
 
void MenuIdle_display(void)
{
   	static u8 count_tx = 0;
		static u8 count_rx = 0;
		uint8 base = page_var_base();
		char label[LABEL_CHARS + 1];
		
		/* A label edited in T3000 arrives without a page change, so each row is
		 * reloaded and repainted where it stands. Comparing the normalised copy
		 * rather than the raw label keeps the padding from counting as a change. */
		load_label(label, base);
		if(memcmp(UI_DIS_LINE1, label, LABEL_CHARS))
		{
			memcpy(UI_DIS_LINE1, label, LABEL_CHARS + 1);
			disp_str_12_24(LABEL_XPOS, SETPOINT_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE1, SCH_COLOR, TSTAT8_BACK_COLOR);
		}
		load_label(label, base + 1);
		if(memcmp(UI_DIS_LINE2, label, LABEL_CHARS))
		{
			memcpy(UI_DIS_LINE2, label, LABEL_CHARS + 1);
			disp_str_12_24(LABEL_XPOS, FAN_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE2, SCH_COLOR, TSTAT8_BACK_COLOR);
		}
		load_label(label, base + 2);
		if(memcmp(UI_DIS_LINE3, label, LABEL_CHARS))
		{
			memcpy(UI_DIS_LINE3, label, LABEL_CHARS + 1);
			disp_str_12_24(LABEL_XPOS, SYS_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE3, SCH_COLOR, TSTAT8_BACK_COLOR);
		}
		
    //display_input_value(inputs[0].value);
		//display_value(inputs[0].value);
		display_screen_value_var(1, base); // show the var values where set, fan and sys used to be
		display_screen_value_var(2, base + 1);
		display_screen_value_var(3, base + 2);

		//display_SP(inputs[0].value / 1000);
		//display_fanspeed(outputs[0].value / 1000);
		//display_mode(vars[0].value / 1000);
		if(Modbus.disable_tstat10_display == 0)
		{
			display_clock();			
			display_icon();
		}		
		else if(Modbus.disable_tstat10_display == 1)
		{
			disp_str(FORM15X30, 0,TIME_POS,"            ",TSTAT8_CH_COLOR,TSTAT8_MENU_COLOR2); 
			disp_null_icon(ICON3_XDOTS, ICON3_YDOTS, 0, ICON3_FAN_POS, ICON_POS, TSTAT8_BACK_COLOR, TSTAT8_BACK_COLOR);
			disp_null_icon(ICON3_XDOTS, ICON3_YDOTS, 0, ICON3_MODE_POS, ICON_POS, TSTAT8_BACK_COLOR, TSTAT8_BACK_COLOR);
			disp_null_icon(ICON3_XDOTS, ICON3_YDOTS, 0, ICON3_WALL_POS, ICON_POS, TSTAT8_BACK_COLOR, TSTAT8_BACK_COLOR);
		}
		else if(Modbus.disable_tstat10_display == 2)
		{
			display_clock();		
			disp_null_icon(ICON3_XDOTS, ICON3_YDOTS, 0, ICON3_FAN_POS, ICON_POS, TSTAT8_BACK_COLOR, TSTAT8_BACK_COLOR);
			disp_null_icon(ICON3_XDOTS, ICON3_YDOTS, 0, ICON3_MODE_POS, ICON_POS, TSTAT8_BACK_COLOR, TSTAT8_BACK_COLOR);
			disp_null_icon(ICON3_XDOTS, ICON3_YDOTS, 0, ICON3_WALL_POS, ICON_POS, TSTAT8_BACK_COLOR, TSTAT8_BACK_COLOR);
		}

//		if(Setting_Info.reg.display_lcd.lcddisplay[0] == 0)
//		{
//			if(Modbus.mini_type == MINI_T10P)
//			{
//				if((inputs[HI_COMMON_CHANNEL].digital_analog == 1) && inputs[HI_COMMON_CHANNEL].range == R10K_40_250DegF) //if the range is 10K type2 F, display F
//				{	
//					Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[HI_COMMON_CHANNEL].value / 100, TOP_AREA_DISP_UNIT_F);
//				}
//				else
//				{
//					Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[HI_COMMON_CHANNEL].value / 100, TOP_AREA_DISP_UNIT_C);
//				}
//			}
//			else
//			{
//				if((inputs[COMMON_CHANNEL].digital_analog == 1) && inputs[COMMON_CHANNEL].range == R10K_40_250DegF) //if the range is 10K type2 F, display F
//				{	
//					Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[COMMON_CHANNEL].value / 100, TOP_AREA_DISP_UNIT_F);
//				}
//				else
//				{
//					Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[COMMON_CHANNEL].value / 100, TOP_AREA_DISP_UNIT_C);
//				}
//			}
//		}
//		else
		{
			char type,num;
			//if(Setting_Info.reg.display_lcd.lcddisplay[0] == 1) // modbus
			{				
				{
					uint8 new_type, new_num;

					load_top_area_point(&new_type, &new_num);
					if(digital_top_area_type != new_type || digital_top_area_num != new_num)
					{
						digital_top_area_type = new_type;
						digital_top_area_num = new_num;
						digital_top_area_changed = 1;
					}
				}
			
				type = digital_top_area_type;
				num = digital_top_area_num;
				
				if(digital_top_area_changed)
				{
					digital_top_area_changed = 0;
					disp_str(FORM15X30, SCH_XPOS,  0, "              ",SCH_COLOR,TSTAT8_BACK_COLOR);					
					disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "            ",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
					disp_str(FORM15X30, SCH_XPOS,  CH_HEIGHT, "              ",SCH_COLOR,TSTAT8_BACK_COLOR);
					disp_str(FORM15X30, SCH_XPOS,  CH_HEIGHT * 2 - 7, "              ",SCH_COLOR,TSTAT8_BACK_COLOR);
				}
				
				if(type == IN)
				{					
					if(inputs[num].digital_analog == 1)
					{
						flag_digital_top_area = 0;		
						if(inputs[num].range == R10K_40_250DegF)
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[num].value / 100, TOP_AREA_DISP_UNIT_F);
						else if(inputs[num].range == R10K_40_120DegC)
						{
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[num].value / 100, TOP_AREA_DISP_UNIT_C);
						}
						else if(inputs[num].range == 27)  // humidity
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[num].value / 100, TOP_AREA_DISP_UNIT_RH);
						else 
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[num].value / 1000, TOP_AREA_DISP_UNIT_NONE);
					
					}
					else
					{			
						flag_digital_top_area = 1;						
						load_top_label(inputs[digital_top_area_num].label);
						disp_str_16_24(FORM15X30, SCH_XPOS + 20,  IDLE_LINE1_POS, UI_DIS_TOP,SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
						
						
						if(inputs[num].control)
						{
							if(inputs[num].range == OFF_ON)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ON",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == CLOSED_OPEN)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OPEN",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == STOP_START)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "STRAT",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == DISABLED_ENABLED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ENABLED",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == NORMAL_ALARM)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ALARM",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == UNOCCUPIED_OCCUPIED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OCC",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == LOW_HIGH)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "HIGH",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ON",SCH_COLOR,TSTAT8_BACK_COLOR);
						}
						else
						{
							if(inputs[num].range == OFF_ON)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OFF",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == CLOSED_OPEN)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "CLOSED",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == STOP_START)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "STOP",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == DISABLED_ENABLED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "DISABLED",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == NORMAL_ALARM)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "NORMAL",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == UNOCCUPIED_OCCUPIED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "UNOCC",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(inputs[num].range == LOW_HIGH)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE1_POS, "LOW",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OFF",SCH_COLOR,TSTAT8_BACK_COLOR);
						}
					
					}
				}
				else if(type == OUT)
				{					
					if(outputs[num].digital_analog == 1)
					{
						flag_digital_top_area = 0;	
						// tbd:
//						if(outputs[num].range == R10K_40_250DegF)
//							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[num].value / 100, TOP_AREA_DISP_UNIT_F);
//						else if(inputs[num].range == R10K_40_120DegC)
//							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[num].value / 100, TOP_AREA_DISP_UNIT_C);
//						else if(inputs[num].range == 27)  // humidity
//							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[num].value / 100, TOP_AREA_DISP_UNIT_RH);
//						else 
//							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, inputs[num].value / 1000, TOP_AREA_DISP_UNIT_NONE);
					}
					else
					{
						flag_digital_top_area = 1;

						load_top_label(outputs[digital_top_area_num].label);
						disp_str_16_24(FORM15X30, SCH_XPOS + 20,  IDLE_LINE1_POS, UI_DIS_TOP,SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
						if(outputs[num].control)
						{
							if(outputs[num].range == OFF_ON)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ON",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == CLOSED_OPEN)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OPEN",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == STOP_START)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "START",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == DISABLED_ENABLED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ENABLED",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == NORMAL_ALARM)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ALARM",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == UNOCCUPIED_OCCUPIED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OCC",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == LOW_HIGH)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "HIGH",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ON",SCH_COLOR,TSTAT8_BACK_COLOR);
						}
						else
						{
							if(outputs[num].range == OFF_ON)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OFF",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == CLOSED_OPEN)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "CLOSED",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == STOP_START)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "STOP",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == DISABLED_ENABLED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "DISABLED",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == NORMAL_ALARM)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "NORMAL",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == UNOCCUPIED_OCCUPIED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "UNOCC",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(outputs[num].range == LOW_HIGH)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE1_POS, "LOW",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OFF",SCH_COLOR,TSTAT8_BACK_COLOR);
						}
					
					}
				}
				else if(type == VAR)
				{
					if(vars[num].digital_analog == 1)
					{
						flag_digital_top_area = 0;	
						if(vars[num].range == degF) //If range is set to 10K type2 F, display F
						{	
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, vars[num].value / 100, TOP_AREA_DISP_UNIT_F);
						}
						else	if(vars[num].range == degC) //If range is set to 10K type2 F, display F
						{
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, vars[num].value / 100, TOP_AREA_DISP_UNIT_C);
						}
						else	if(vars[num].range == KPa) //If range is set to 10K type2 F, display F
						{
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, vars[num].value / 1000, TOP_AREA_DISP_UNIT_kPa);
						}
						else	if(vars[num].range == Pa) //If range is set to 10K type2 F, display F
						{
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, vars[num].value / 1000, TOP_AREA_DISP_UNIT_Pa);
						}
						else	if(vars[num].range == RH)
						{
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, vars[num].value / 1000, TOP_AREA_DISP_UNIT_RH);
						}
						else 	if(vars[num].range == ppm)
						{
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, vars[num].value / 1000, TOP_AREA_DISP_UNIT_PPM);
						}
						else
							Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, vars[num].value / 1000, TOP_AREA_DISP_UNIT_NONE);
					}
					else
					{
						flag_digital_top_area = 1;						
						
						load_top_label(vars[digital_top_area_num].label);
						disp_str_16_24(FORM15X30, SCH_XPOS + 20,  IDLE_LINE1_POS, UI_DIS_TOP,SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
						
						if(vars[num].control)
						{
							if(vars[num].range == OFF_ON)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ON",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == CLOSED_OPEN)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OPEN",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == STOP_START)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "START",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == DISABLED_ENABLED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ENABLED",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == NORMAL_ALARM)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ALARM",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == UNOCCUPIED_OCCUPIED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OCC",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == LOW_HIGH)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "HIGH",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "ON",SCH_COLOR,TSTAT8_BACK_COLOR);
						}
						else
						{
							if(vars[num].range == OFF_ON)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OFF",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == CLOSED_OPEN)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "CLOSED",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == STOP_START)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "STOP",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == DISABLED_ENABLED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "DISABLED",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == NORMAL_ALARM)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "NORMAL",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == UNOCCUPIED_OCCUPIED)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "UNOCC",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else if(vars[num].range == LOW_HIGH)
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE1_POS, "LOW",SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
							else
								disp_str(FORM15X30, SCH_XPOS,  IDLE_LINE2_POS, "OFF",SCH_COLOR,TSTAT8_BACK_COLOR);
						}
					
					}
//					else
//					{						
//						Top_area_display(TOP_AREA_DISP_ITEM_TEMPERATURE, vars[num].control, TOP_AREA_DISP_UNIT_NONE);
//					}
//					else
//					{// uint is not 
//						// tbd: add more
//						
//					}
				}
				// ..... tbd: add more type
			}
// 			if(Setting_Info.reg.display_lcd.lcddisplay[0] == 1) // bacnet
//			{
//			}
			
		}
		
		if(count_left_key > 5) 
			disp_index = 0;
		else
			count_left_key++;

		if(disp_index == 1)
		{
			if(flag_digital_top_area == 1)
				disp_str_16_24(FORM15X30, SCH_XPOS + 20,  IDLE_LINE1_POS, UI_DIS_TOP,SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
			disp_str_12_24(LABEL_XPOS, SETPOINT_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE1, SCH_COLOR, TSTAT8_BACK_COLOR1);//TSTAT8_BACK_COLOR
			disp_str_12_24(LABEL_XPOS, FAN_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE2, SCH_COLOR, TSTAT8_BACK_COLOR);
			disp_str_12_24(LABEL_XPOS, SYS_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE3, SCH_COLOR, TSTAT8_BACK_COLOR);
		}
		else if(disp_index == 2)
		{
			if(flag_digital_top_area == 1)
				disp_str_16_24(FORM15X30, SCH_XPOS + 20,  IDLE_LINE1_POS, UI_DIS_TOP,SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
			disp_str_12_24(LABEL_XPOS, SETPOINT_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE1, SCH_COLOR, TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
			disp_str_12_24(LABEL_XPOS, FAN_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE2, SCH_COLOR, TSTAT8_BACK_COLOR1);
			disp_str_12_24(LABEL_XPOS, SYS_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE3, SCH_COLOR, TSTAT8_BACK_COLOR);
		}
		else if(disp_index == 3)
		{
			if(flag_digital_top_area == 1)
				disp_str_16_24(FORM15X30, SCH_XPOS + 20,  IDLE_LINE1_POS, UI_DIS_TOP,SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
			disp_str_12_24(LABEL_XPOS, SETPOINT_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE1, SCH_COLOR, TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
			disp_str_12_24(LABEL_XPOS, FAN_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE2, SCH_COLOR, TSTAT8_BACK_COLOR);
			disp_str_12_24(LABEL_XPOS, SYS_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE3, SCH_COLOR, TSTAT8_BACK_COLOR1);
		}
		else if(disp_index == 4) // top area
		{
			if(flag_digital_top_area == 1)
				disp_str_16_24(FORM15X30, SCH_XPOS + 20,  IDLE_LINE1_POS, UI_DIS_TOP,SCH_COLOR,TSTAT8_BACK_COLOR1);//TSTAT8_BACK_COLOR
			disp_str_12_24(LABEL_XPOS, SETPOINT_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE1, SCH_COLOR, TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
			disp_str_12_24(LABEL_XPOS, FAN_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE2, SCH_COLOR, TSTAT8_BACK_COLOR);
			disp_str_12_24(LABEL_XPOS, SYS_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE3, SCH_COLOR, TSTAT8_BACK_COLOR);
		}
		else
		{
			if(flag_digital_top_area == 1)
				disp_str_16_24(FORM15X30, SCH_XPOS + 20,  IDLE_LINE1_POS, UI_DIS_TOP,SCH_COLOR,TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
			disp_str_12_24(LABEL_XPOS, SETPOINT_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE1, SCH_COLOR, TSTAT8_BACK_COLOR);//TSTAT8_BACK_COLOR
			disp_str_12_24(LABEL_XPOS, FAN_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE2, SCH_COLOR, TSTAT8_BACK_COLOR);
			disp_str_12_24(LABEL_XPOS, SYS_MODE_POS + LABEL_YOFF, (uint8 *)UI_DIS_LINE3, SCH_COLOR, TSTAT8_BACK_COLOR);
		}

        //sprintf(test_char, "%d", SSID_Info.IP_Wifi_Status); //for testing: show the wifi status value in the top left of the screen;
        //disp_str(FORM15X30, 0, 0, test_char, SCH_COLOR, TSTAT8_BACK_COLOR);

//        if (SSID_Info.IP_Wifi_Status == WIFI_NORMAL) 
//        {
//            disp_icon(26, 26, wificonnect, 210, 0, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
//        }
//         //  
//        else if (SSID_Info.IP_Wifi_Status == WIFI_NO_WIFI || SSID_Info.IP_Wifi_Status == WIFI_NONE)
//        {
//            disp_null_icon(26, 26, 0, 210, 0, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
//        }
//        else
//            disp_icon(26, 26, wifinocnnct, 210, 0, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
				
			if(SSID_Info.IP_Wifi_Status == WIFI_NORMAL)//Show the wifi status in the top right of the screen
			{
				if(SSID_Info.rssi < 70)		
					disp_icon(WIFI_XDOTS, WIFI_YDOTS, wifi_4, WIFI_XPOS,	WIFI_YPOS, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
				else if(SSID_Info.rssi < 80)							
					disp_icon(WIFI_XDOTS, WIFI_YDOTS, wifi_3, WIFI_XPOS,	WIFI_YPOS, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
				else if(SSID_Info.rssi < 90)							
					disp_icon(WIFI_XDOTS, WIFI_YDOTS, wifi_2, WIFI_XPOS,	WIFI_YPOS, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
				else							
					disp_icon(WIFI_XDOTS, WIFI_YDOTS, wifi_1, WIFI_XPOS,	WIFI_YPOS, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
			}
			else	if((SSID_Info.IP_Wifi_Status == WIFI_NO_CONNECT)
				|| (SSID_Info.IP_Wifi_Status == WIFI_SSID_FAIL))
					disp_icon(WIFI_XDOTS, WIFI_YDOTS, wifi_0, WIFI_XPOS,	WIFI_YPOS, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
				// if WIFI_NONE, do not show wifi flag
			else //if((SSID_Info.IP_Wifi_Status == WIFI_NO_WIFI)
				disp_icon(WIFI_XDOTS, WIFI_YDOTS, wifi_none, WIFI_XPOS,	WIFI_YPOS, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
			
						
			// show TX,RX
			
			if(flagLED_uart0_tx > 0)
			{
				if(count_tx++ % 2 == 0)
					disp_icon(LINK_XDOTS, LINK_YDOTS, cmnct_send, 	LINK_TX_XPOS,	LINK_YPOS, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
				else
					disp_null_icon(LINK_XDOTS, LINK_YDOTS, 0, LINK_TX_XPOS,LINK_YPOS,TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
			}
			else
			{
				count_tx = 0;
				disp_null_icon(LINK_XDOTS, LINK_YDOTS, 0, LINK_TX_XPOS,LINK_YPOS,TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);//(26, 26, cmnct_icon, 	0,	0, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
			}
			
			if(flagLED_uart0_rx > 0)
			{
				// if TX on, then RX off
				if(count_tx % 2 == 1)
					count_rx = 0;
				if(count_rx++ % 2 == 1)
					disp_icon(LINK_XDOTS, LINK_YDOTS, cmnct_rcv, 	LINK_RX_XPOS,	LINK_YPOS, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
				else
					disp_null_icon(LINK_XDOTS, LINK_YDOTS, 0, LINK_RX_XPOS,LINK_YPOS,TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
			}
			else
			{
				count_rx = 0;
				disp_null_icon(LINK_XDOTS, LINK_YDOTS, 0, LINK_RX_XPOS,LINK_YPOS,TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);//(26, 26, cmnct_icon, 	0,	0, TSTAT8_CH_COLOR, TSTAT8_BACK_COLOR);
			}
			
			if(flagLED_uart0_tx > 0)
				flagLED_uart0_tx = 0;
			if(flagLED_uart0_rx > 0)
				flagLED_uart0_rx = 0;
				
}


extern uint8_t item_to_adjust;
uint8_t check_msv_data_len(uint8_t index)
{
	char j;
	char len;
	len = 0;
	
	for(j = 0; j < STR_MSV_MULTIPLE_COUNT;j++)
	{
		if(msv_data[index][j].status != 0)
		{
			len++;
		}
		else
		{
			return len;
		}
	}
	return len;
}


void MenuIdle_keycope(uint16 key_value)
{
    uint8 i;
    uint8 temp_value = 0;
    uint8 base = page_var_base();
	switch(key_value /*& KEY_SPEED_MASK*/)
	{
		case 0:
			break;
		case KEY_UP_MASK: 
			count_left_key = 0;
			if((disp_index >= 1) && (disp_index <= 3))
			{
				if ((vars[base + disp_index - 1].range >= 101) && (vars[base + disp_index - 1].range <= 103))  // 101 102 103 	MSV range
				{
					char len;
					len = check_msv_data_len(vars[base + disp_index - 1].range - 101);
					for (i = 0; i < len; i++)
					{
						if (vars[base + disp_index - 1].value / 1000 == msv_data[vars[base + disp_index - 1].range - 101][i].msv_value)
						{
							temp_value = i;
							break;
						}
					}

					for (i = temp_value; i < 7; i++)
					{
						if(strlen(msv_data[vars[base + disp_index - 1].range - 101][i + 1].msv_name) != 0
							&& msv_data[vars[base + disp_index - 1].range - 101][i + 1].msv_name[0] != 0xff)
						{
							vars[base + disp_index - 1].value = msv_data[vars[base + disp_index - 1].range - 101][i + 1].msv_value * 1000;
							break;
						}
					}
				}
				else
				{
					if(vars[base + disp_index - 1].digital_analog == 0)
					{
						if(vars[base + disp_index - 1].control == 0)
							vars[base + disp_index - 1].control = 1;
						else
							vars[base + disp_index - 1].control = 0;
					}
					else
					{
						if(vars[base + disp_index - 1].value < 999 * 1000)
								vars[base + disp_index - 1].value = vars[base + disp_index - 1].value + 1000;
							else
								vars[base + disp_index - 1].value = 0;
					}
				}
			}
			else // disp_index == 4
			{
				digital_top_area_changed = 1;
				if(digital_top_area_type == IN)
				{
					inputs[digital_top_area_num].control = ((inputs[digital_top_area_num].control) == 0) ? 1 : 0;
				}
				else if(digital_top_area_type == VAR)
				{
					vars[digital_top_area_num].control = ((vars[digital_top_area_num].control) == 0) ? 1 : 0;
				}
				else if(digital_top_area_type == OUT)
				{
					outputs[digital_top_area_num].control = ((outputs[digital_top_area_num].control) == 0) ? 1 : 0;
					if(outputs[digital_top_area_num].control) 					
						set_output_raw(digital_top_area_num,1000);
					else 
						set_output_raw(digital_top_area_num,0);	
				}
			}

			write_page_en[VAR] = 1;
			ChangeFlash = 1;
			break;
		case KEY_SPEED_10 | KEY_UP_MASK:	
			count_left_key = 0;
			if((disp_index >= 1) && (disp_index <= 3))
			{
				if ((vars[base + disp_index - 1].range >= 101) && (vars[base + disp_index - 1].range <= 103))  // 101 102 103 	MSV range
				{					
					char len;
					len = check_msv_data_len(vars[base + disp_index - 1].range - 101);
					for (i = 0; i < len; i++)
					{
						if (vars[base + disp_index - 1].value / 1000 == msv_data[vars[base + disp_index - 1].range - 101][i].msv_value)
						{
							temp_value = i;
							break;
						}
					}

					for (i = temp_value; i < 7; i++)
					{
						if(strlen(msv_data[vars[base + disp_index - 1].range - 101][i + 1].msv_name) != 0
							&& msv_data[vars[base + disp_index - 1].range - 101][i + 1].msv_name[0] != 0xff)
						{
							vars[base + disp_index - 1].value = msv_data[vars[base + disp_index - 1].range - 101][i + 1].msv_value * 1000;
							break;
						}
					}
				}
				else
				{
					if(vars[base + disp_index - 1].digital_analog == 0)
					{
						if(vars[base + disp_index - 1].control == 0)
							vars[base + disp_index - 1].control = 1;
						else
							vars[base + disp_index - 1].control = 0;
					}
					else
					{
					if(vars[base + disp_index - 1].value < 999 * 1000)
							vars[base + disp_index - 1].value = vars[base + disp_index - 1].value + 10000;
						else
							vars[base + disp_index - 1].value = 0;
					}
				}
			}
			else // disp_index == 4
			{
				digital_top_area_changed = 1;
				if(digital_top_area_type == IN)
				{
					inputs[digital_top_area_num].control = ((inputs[digital_top_area_num].control) == 0) ? 1 : 0;
				}
				else if(digital_top_area_type == VAR)
				{
					vars[digital_top_area_num].control = ((vars[digital_top_area_num].control) == 0) ? 1 : 0;
				}
				else if(digital_top_area_type == OUT)
				{
					outputs[digital_top_area_num].control = ((outputs[digital_top_area_num].control) == 0) ? 1 : 0;
					if(outputs[digital_top_area_num].control) 					
						set_output_raw(digital_top_area_num,1000);
					else 
						set_output_raw(digital_top_area_num,0);	
				}
			}

			write_page_en[VAR] = 1;
			ChangeFlash = 1;
			break;

		case KEY_DOWN_MASK:
			count_left_key = 0;			
			if((disp_index >= 1) && (disp_index <= 3))
			{
				if ((vars[base + disp_index - 1].range >= 101) && (vars[base + disp_index - 1].range <= 103))  // 101 102 103 	MSV range
				{
					//if(vars[base + disp_index - 1].range == 101)  //if the range is multi-state, adjust the multi-state value;
					{
						// check the lenght of msv_data
						char len;
						len = check_msv_data_len(vars[base + disp_index - 1].range - 101);
							for (i = 0; i < len; i++)
							{
									if (vars[base + disp_index - 1].value / 1000 == msv_data[vars[base + disp_index - 1].range - 101][i].msv_value)
									{
											temp_value = i;
											break;
									}
							}

							for (i = temp_value; i > 0; i--)
							{
									if (strlen(msv_data[vars[base + disp_index - 1].range - 101][i - 1].msv_name) != 0)
									{
											vars[base + disp_index - 1].value = msv_data[vars[base + disp_index - 1].range - 101][i - 1].msv_value * 1000;
											break;
									}
							}
					}
//					else
//					{
//						if(vars[base + disp_index - 1].value > 1000)
//							vars[base + disp_index - 1].value = vars[base + disp_index - 1].value - 1000;
//						else
//							vars[base + disp_index - 1].value = STR_MSV_MULTIPLE_COUNT * 1000;
//					}
				}
				else
				{
					if(vars[base + disp_index - 1].digital_analog == 0)
					{
						if(vars[base + disp_index - 1].control == 0)
							vars[base + disp_index - 1].control = 1;
						else
							vars[base + disp_index - 1].control = 0;
					}
					else
					{
//					if(vars[base + disp_index - 1].value > 1000)
							vars[base + disp_index - 1].value = vars[base + disp_index - 1].value - 1000;
//						else
//							vars[base + disp_index - 1].value = 99 * 1000;
					}
				}
			}
			else // disp_index == 4
			{
				digital_top_area_changed = 1;
				if(digital_top_area_type == IN)
				{
					inputs[digital_top_area_num].control = ((inputs[digital_top_area_num].control) == 0) ? 1 : 0;
				}
				else if(digital_top_area_type == VAR)
				{
					vars[digital_top_area_num].control = ((vars[digital_top_area_num].control) == 0) ? 1 : 0;
				}
				else if(digital_top_area_type == OUT)
				{
					outputs[digital_top_area_num].control = ((outputs[digital_top_area_num].control) == 0) ? 1 : 0;
					if(outputs[digital_top_area_num].control) 					
						set_output_raw(digital_top_area_num,1000);
					else 
						set_output_raw(digital_top_area_num,0);	
				}
			}
		
			write_page_en[VAR] = 1;
			ChangeFlash = 1;
			break;		
		case KEY_SPEED_10 | KEY_DOWN_MASK: 
			count_left_key = 0;			
			if((disp_index >= 1) && (disp_index <= 3))
			{
				if ((vars[base + disp_index - 1].range >= 101) && (vars[base + disp_index - 1].range <= 103))  // 101 102 103 	MSV range
				{
					char len;
					len = check_msv_data_len(vars[base + disp_index - 1].range - 101);
					for (i = 0; i < len; i++)
					{
							if (vars[base + disp_index - 1].value / 1000 == msv_data[vars[base + disp_index - 1].range - 101][i].msv_value)
							{
									temp_value = i;
									break;
							}
					}

					for (i = temp_value; i > 0; i--)
					{
							if (strlen(msv_data[vars[base + disp_index - 1].range - 101][i - 1].msv_name) != 0)
							{
									vars[base + disp_index - 1].value = msv_data[vars[base + disp_index - 1].range - 101][i - 1].msv_value * 1000;
									break;
							}
					}
				}
				else
				{
					if(vars[base + disp_index - 1].digital_analog == 0)
					{
						if(vars[base + disp_index - 1].control == 0)
							vars[base + disp_index - 1].control = 1;
						else
							vars[base + disp_index - 1].control = 0;
					}
					else
					{
						vars[base + disp_index - 1].value = vars[base + disp_index - 1].value - 10000;
					}
				}
			}
			else // disp_index == 4
			{
				digital_top_area_changed = 1;
				if(digital_top_area_type == IN)
				{
					inputs[digital_top_area_num].control = ((inputs[digital_top_area_num].control) == 0) ? 1 : 0;
				}
				else if(digital_top_area_type == VAR)
				{
					vars[digital_top_area_num].control = ((vars[digital_top_area_num].control) == 0) ? 1 : 0;
				}
				else if(digital_top_area_type == OUT)
				{
					outputs[digital_top_area_num].control = ((outputs[digital_top_area_num].control) == 0) ? 1 : 0;
					if(outputs[digital_top_area_num].control) 					
						set_output_raw(digital_top_area_num,1000);
					else 
						set_output_raw(digital_top_area_num,0);	
				}
			}
		
			write_page_en[VAR] = 1;
			ChangeFlash = 1;
			break;
		
		case KEY_LEFT_MASK:
			// change SETP, FAN , SYS
			if(flag_digital_top_area == 1)
			{
				if(disp_index < 4) disp_index++;
				else 
					disp_index = 1;
			}
			else
			{
				if(disp_index < 3) disp_index++;
				else 
					disp_index = 1;
			}
			flag_left_key = 1;
			count_left_key = 0;
			break;
		case KEY_RIGHT_MASK:
			// next page of VARs. This key did nothing on the idle screen before.
			// Holding it repeats, the same way holding LEFT already walks the rows.
			page_index++;
			show_page_rows();	// wraps back to page 0 past the last one
			break;
		case KEY_LEFT_RIGHT_MASK:
			update_menu_state(MenuMain);
			break;
		default:
			break;
	}
}



