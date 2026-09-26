/* Stand-ins for the firmware headers bacnet/private/decode.c includes, so that it
 * builds unchanged as 32-bit x86. The other headers in this directory include
 * only this one; basic.h is the real one, copied next to decode.c by run.py.
 *
 * Types follow arm/USER/types.h. Sizes decode.c depends on follow the firmware:
 * Point and Point_Net (IO_control/ud_str.h), message[] (94 bytes), the table
 * sizes below. The structures decode.c only reaches through a field or two are
 * cut down to those fields, and the harness defines the globals. */
#ifndef HOST_H
#define HOST_H

#include <string.h>

#define far
#define Byte  unsigned char
#define S8_T  signed char
#define S16_T signed short int
#define S32_T signed int
#define U8_T  unsigned char
#define U16_T unsigned short int
#define U32_T unsigned int
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef signed short int16_t;

#define LOW_BYTE(word)  (U8_T)((word) & 0x00FF)
#define HIGH_BYTE(word) (U8_T)(((word) & 0xFF00) >> 8)

/* the T3-OEM build */
#define BAC_PRIVATE    1
#define ARM_MINI       0
#define ARM_CM5        0
#define ARM_TSTAT_WIFI 1

#define MAX_PRGS               16
#define MAX_ARRAYS             16
#define MAX_ALARMS             16
#define MAX_INS                64
#define MAX_CONS               16
#define MAX_SCHEDULES_PER_WEEK 9
#define ARRAY                  11
#define VAR                    2
#define MB_COIL_REG            20
#define VIRTUAL_ALARM          0
#define table1                 20
#define FLOAT_TYPE_ABCD        1
#define FLOAT_TYPE_CDAB        2
#define FLOAT_TYPE_BADC        3
#define FLOAT_TYPE_DCBA        4

typedef union { unsigned long ldata; float fdata; } FloatLongType;
typedef struct { U8_T number; U8_T point_type; } Point;
typedef struct { U8_T number; U8_T point_type; U8_T panel; U8_T sub_id; U8_T network_number; } Point_Net;
typedef struct { U8_T b[6]; } Point_Bacnet;
typedef union { U8_T all[16]; struct { U8_T sec, min, hour, day, week, mon, year; U16_T day_of_year; } Clk; } UN_Time;
typedef struct { U16_T bytes; U8_T on_off; U16_T real_byte; U32_T costtime; } Str_program_point;
typedef struct { S16_T length; } Str_array_point;
typedef struct { U8_T x; } Str_in_point;
typedef union { Str_in_point *pin; void *p; } Str_points_ptr;
typedef struct { U8_T auto_manual, proportional, prop_high, rate, reset; } Str_controller_point;
typedef struct { U8_T panel; U8_T sub_id; } REMOTE_PANEL_HOST;
typedef struct { U8_T minutes, hours; } WR_TIME_HOST;
typedef struct { WR_TIME_HOST time[MAX_SCHEDULES_PER_WEEK]; } WR_DAY_HOST;
typedef struct { U8_T alarm, acknowledged; } Alarm_point_host;
typedef struct { U8_T icon_config; } STR_MODBUS_HOST;

/* prg_code[] lives inside an arena with guard bytes either side, so that the
 * harness can see a write that leaves a row. sizeof(prg_code[0]) is still 2000. */
#define GUARD (64 * 1024)
typedef struct { U8_T lo[GUARD]; U8_T rows[MAX_PRGS][2000]; U8_T hi[GUARD]; } ARENA_HOST;
extern ARENA_HOST arena;
#define prg_code (arena.rows)

extern Str_program_point programs[MAX_PRGS];
extern U8_T *prog;
extern S32_T stack[20];
extern S32_T *index_stack;
extern char message[94];
extern S8_T alarm_flag;
extern S32_T cond;
extern S32_T *pn;
extern S8_T *time_buf;
extern S32_T op1, op2, value;
extern U8_T Station_NUM;
extern S8_T ind_alarm_panel;
extern U8_T alarm_panel[5];
extern U8_T alarm_at_all;
extern U8_T new_alarm_flag;
extern Str_array_point arrays[MAX_ARRAYS];
extern long *arrays_address[MAX_ARRAYS];
extern U8_T remote_panel_num;
extern STR_MODBUS_HOST Modbus;
extern u32 uip_timer;
extern Str_in_point inputs[MAX_INS];
extern U8_T current_online[32];
extern REMOTE_PANEL_HOST remote_panel_db[32];
extern U8_T panel_number;
extern S32_T v;
extern Str_controller_point controllers[MAX_CONS];
extern WR_DAY_HOST wr_times[8][9];
extern Alarm_point_host alarms[MAX_ALARMS];

S16_T swap_word(S16_T dat);
S32_T swap_double(S32_T dat);
void get_point_value(Point *point, S32_T *val);
void put_point_value(Point *point, S32_T *val, int aux, int prog_op);
void get_net_point_value(Point_Net *point, S32_T *val, int a, int b);
void put_net_point_value(Point_Net *point, S32_T *val, int aux, int prog_op, int x);
int generatealarm(char *mes, int prg, int panel, int type, char alarmatall, char indalarmpanel, char *alarmpanel, char printalarm);
void generate_program_alarm(U8_T type, U8_T prg);
void dalarmrestore(char *mes, int prg, int panel);

#endif
