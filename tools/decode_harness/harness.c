/* Run program rows through bacnet/private/decode.c's exec_program on a PC.
 *
 *   harness ROWS.bin SEED SCANS LAYOUT [FIRST]
 *
 * Built by run.py as 32-bit x86, whose pointers, long and byte order match the
 * Cortex-M3. ROWS.bin is a run of 2000-byte program rows. Each row is run for
 * SCANS scans, with every point read answered from a generator seeded by SEED
 * and the row's index, and reported on one line of stdout:
 *
 *   index  slot  returns  trace  alarms  outside  crashed
 *
 *   trace    hash of every point write, alarm text and return value, and the
 *            row's bytes after the last scan
 *   alarms   bit n set if generate_program_alarm(n, ...) was called
 *   outside  1 if a byte outside the row changed (LAYOUT 0 only)
 *   crashed  1 on an access violation, which is also reported on stderr as
 *            "fault INDEX read" or "fault INDEX write"
 *
 * LAYOUT 0 runs the row in prg_code[], inside an arena with 64 KB of guard bytes
 * either side, and compares the whole arena afterwards. LAYOUT 1 and 2 run it
 * from a page of its own, pressed against an inaccessible page after it (1) or
 * before it (2), so that any access past that end faults.
 *
 * FIRST skips rows before that index; run.py uses it to carry on after a row
 * that takes the process down. */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "host.h"

ARENA_HOST arena;
static ARENA_HOST before;
Str_program_point programs[MAX_PRGS];
U8_T *prog;
S32_T stack[20];
S32_T *index_stack;
char message[94];
S8_T alarm_flag;
S32_T cond;
S32_T *pn;
S8_T *time_buf;
S32_T op1, op2, value;
U8_T Station_NUM = 1;
S8_T ind_alarm_panel;
U8_T alarm_panel[5];
U8_T alarm_at_all;
U8_T new_alarm_flag;
Str_array_point arrays[MAX_ARRAYS];
long *arrays_address[MAX_ARRAYS];
U8_T remote_panel_num;
STR_MODBUS_HOST Modbus;
u32 uip_timer;
Str_in_point inputs[MAX_INS];
U8_T current_online[32];
REMOTE_PANEL_HOST remote_panel_db[32];
U8_T panel_number;
S32_T v;
Str_controller_point controllers[MAX_CONS];
WR_DAY_HOST wr_times[8][9];
Alarm_point_host alarms[MAX_ALARMS];
UN_Time Rtc;
extern U32_T miliseclast_cur;
S16_T exec_program(S16_T current_prg, U8_T *prog_code);

static unsigned int rng;
static unsigned int hash;
static int alarm_types;

static unsigned int next(void)
{
    rng = rng * 1103515245u + 12345u;
    return rng >> 8;
}

static void mix(unsigned int x)
{
    int k;

    for(k = 0; k < 4; k++)
    {
        hash ^= (x >> (8 * k)) & 0xFF;
        hash *= 16777619u;
    }
}

/* ---- what decode.c calls ------------------------------------------------- */
S16_T swap_word(S16_T dat) { return dat; }
S32_T swap_double(S32_T dat) { return dat; }
float test_match_custom(uint8_t range, S16_T raw) { return 0; }

void get_point_value(Point *point, S32_T *val)
{
    unsigned int r = next();

    *val = (r & 1) ? ((r & 2) ? 1000 : 0) : (S32_T)(r % 200001) - 100000;
}

void get_net_point_value(Point_Net *point, S32_T *val, int a, int b)
{
    get_point_value(0, val);
}

void put_point_value(Point *point, S32_T *val, int aux, int prog_op)
{
    mix(0x100 + point->number);
    mix(point->point_type);
    mix(*val);
}

void put_net_point_value(Point_Net *point, S32_T *val, int aux, int prog_op, int x)
{
    mix(0x200 + point->number);
    mix(*val);
}

int generatealarm(char *mes, int prg, int panel, int type, char a, char b, char *c, char d)
{
    size_t k;

    mix(0x300);
    for(k = 0; k < 94 && mes[k]; k++)
    {
        mix((unsigned char)mes[k]);
    }
    return 0;
}

void generate_program_alarm(U8_T type, U8_T prg)
{
    mix(0x400 + type);
    alarm_types |= 1 << type;
}

void dalarmrestore(char *mes, int prg, int panel)
{
    mix(0x500);
}

/* ---- running a row ------------------------------------------------------- */
static int fault_write;

static int filter(EXCEPTION_POINTERS *e)
{
    if(e->ExceptionRecord->ExceptionCode != EXCEPTION_ACCESS_VIOLATION)
    {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    fault_write = e->ExceptionRecord->ExceptionInformation[0] != 0;
    return EXCEPTION_EXECUTE_HANDLER;
}

static int run(int index, int slot, U8_T *row, int scans, int *rets)
{
    int s, r;

    __try
    {
        for(s = 0; s < scans; s++)
        {
            miliseclast_cur = 1000;
            r = exec_program((S16_T)slot, row);
            mix((unsigned int)r);
            *rets = *rets * 3 + (r + 1);
        }
    }
    __except(filter(GetExceptionInformation()))
    {
        fprintf(stderr, "fault %d %s\n", index, fault_write ? "write" : "read");
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    static U8_T bytes[2000];
    FILE *f;
    unsigned int seed;
    int scans, layout, first = 0, index = 0;
    U8_T *page_row = NULL;

    if(argc < 5)
    {
        fprintf(stderr, "harness ROWS.bin SEED SCANS LAYOUT [FIRST]\n");
        return 2;
    }
    f = fopen(argv[1], "rb");
    if(f == NULL)
    {
        return 2;
    }
    seed = (unsigned int)strtoul(argv[2], NULL, 0);
    scans = atoi(argv[3]);
    layout = atoi(argv[4]);
    if(argc > 5)
    {
        first = atoi(argv[5]);
    }
    setvbuf(stdout, NULL, _IONBF, 0);

    if(layout != 0)
    {
        DWORD was;
        U8_T *pages = (U8_T *)VirtualAlloc(NULL, 3 * 4096, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);

        VirtualProtect(pages, 4096, PAGE_NOACCESS, &was);
        VirtualProtect(pages + 2 * 4096, 4096, PAGE_NOACCESS, &was);
        page_row = (layout == 1) ? pages + 2 * 4096 - 2000 : pages + 4096;
    }

    for(; fread(bytes, 1, sizeof(bytes), f) == sizeof(bytes); index++)
    {
        int slot, crashed, outside = 0, rets = 0;
        U8_T *row;
        size_t k;

        if(index < first)
        {
            continue;
        }
        rng = seed ^ (unsigned int)(index * 2654435761u);
        slot = (index % 3 == 0) ? MAX_PRGS - 1 : (int)(next() % MAX_PRGS);   /* the last row often */
        memset(&arena, 0xAA, sizeof(arena));
        memset(programs, 0, sizeof(programs));
        row = page_row ? page_row : prg_code[slot];
        memcpy(row, bytes, sizeof(bytes));
        memcpy(&before, &arena, sizeof(arena));
        hash = 2166136261u;
        alarm_types = 0;

        crashed = run(index, slot, row, scans, &rets);

        if(page_row == NULL)
        {
            U8_T *a = (U8_T *)&arena;
            U8_T *b = (U8_T *)&before;

            for(k = 0; k < sizeof(arena) && !outside; k++)
            {
                if((a + k < row || a + k >= row + 2000) && a[k] != b[k])
                {
                    outside = 1;
                }
            }
        }
        for(k = 0; k < 2000; k++)
        {
            mix(row[k]);
        }
        printf("%d %d %d %08x %x %d %d\n", index, slot, rets, hash, alarm_types, outside, crashed);
    }
    return 0;
}
