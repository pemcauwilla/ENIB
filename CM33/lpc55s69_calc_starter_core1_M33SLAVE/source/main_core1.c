/*
 * 2023-2055 (C) E. Boucharé
 */

#include "pin_mux.h"
#include "board.h"

#include "fsl_gpio.h"
#include "fsl_common.h"
#include "fsl_power.h"

/*******************************************************************************
 * Mailbox and events
 ******************************************************************************/
#define EVT_MASK						(0xFF<<24)

#define EVT_NONE						0
#define EVT_CORE_UP						(1U<<24)
#define EVT_RETVAL						(2U<<24)

/* mailbox communication between cores */
void mb_init(void) {
//  Init already done by core0
	MAILBOX->MBOXIRQ[0].IRQCLR = 0xFFFFFFFF;
	NVIC_SetPriority(MAILBOX_IRQn, 2);
	NVIC_EnableIRQ(MAILBOX_IRQn);
}

/* pop events from CPU0 */
uint32_t mb_pop_evt(void) {
	uint32_t evt = MAILBOX->MBOXIRQ[0].IRQ;
	MAILBOX->MBOXIRQ[0].IRQCLR = evt;
	return evt;
}

/* send event to CPU0, wait if there is already a pending event, unless force is set */
bool mb_push_evt(uint32_t evt, bool force) {
	if (MAILBOX->MBOXIRQ[1].IRQ && !force) {
		return false;
	}
	MAILBOX->MBOXIRQ[1].IRQSET = evt;
	return true;
}

#ifdef LCD_CORE1
// CHANGE THIS ACCORDING TO THE LCD.H FILE PATH IN CORE 1 PROJECT
#include "/home/paulo/Documents/MCUXpresso_25.6.136/workspace/lpc55s69_calc_starter_core1_M33SLAVE/lcd/lcd.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

typedef enum {
	L_SOLID,
	L_DOTTED,
	L_DASHED,
	L_COMB,
	L_ARROW,
	L_BAR,
	L_FBAR,
	L_STEP,
	L_FSTEP,
	L_NONE
} line_t;

typedef enum {
	M_CROSS,
	M_PLUS,
	M_DOT,
	M_STAR,
	M_CIRCLE,
	M_FCIRCLE,
	M_SQUARE,
	M_FSQUARE,
	M_DIAMOND,
	M_FDIAMOND,
	M_ARROW,
	M_TRIANGLE,
	M_FTRIANGLE,
	M_NONE
} marker_t;

#define MAXPOINTS	2048

line_t ltype = L_SOLID;
marker_t mtype = M_NONE;
int msize = 6;

/*******************************************************************************
 * Mailbox and events
 ******************************************************************************/
#define EVT DRAWPOINT					(3U<<24)
#define EVT_DRAWLINE					(4U<<24)
#define EVT_DRAWRECT					(5U<<24)
#define EVT_DRAWRNDRECT					(6U<<24)
#define EVT_DRAWCIRCLE					(7U<<24)
#define EVT_DRAWELLISPSE				(8U<<24)
#define EVT_DRAWLINES					(9U<<24)
#define EVT_DRAWSEGMENTS				(10U<<24)
#define EVT_FILLRECT					(11U<<24)
#define EVT_CLIP						(12U<<24)
#define EVT_UNCLIP						(13U<<24)
#define EVT_DRAWPATH					(14U<<24)

#define EVT_FORECOLOR					(15U<<24)
#define EVT_BACKCOLOR					(16U<<24)
#define EVT_SETFONT						(17U<<24)

#define EVT_SETALIGN					(18U<<24)

#define EVT_DRAWSTRING					(19U<<24)
#define EVT_SETDIR						(20U<<24)

#define EVT_GETBUFFER					(30U<<24)

/* event queue */
#define MAX_EVT_DATA				5
typedef struct _Event {
	uint32_t event;
	uint32_t data[MAX_EVT_DATA];
} Event;

/* event queue handling */
#define MAX_EVTS					20

volatile Event evq[MAX_EVTS];
volatile int evq_rd = 0, evq_wr = 0;

bool next_event(Event *evt) {
	if (evq_rd == evq_wr)
		return false;

	*evt = evq[evq_rd];
	evq_rd = (evq_rd + 1) % MAX_EVTS;
	return true;
}

void MAILBOX_IRQHandler(void) {
	if (((evq_wr + 1) % MAX_EVTS) != evq_rd) {
		evq[evq_wr].event = mb_pop_evt();
		evq_wr = (evq_wr + 1) % MAX_EVTS;
	}
}

/*******************************************************************************
 * shared buffer
 ******************************************************************************/
extern SPoint __start_noinit_shmem[];
SPoint *shdata = __start_noinit_shmem;

/*******************************************************************************
 * HELPER FUNCTIONS
 ******************************************************************************/
void handle_drawpath(uint32_t eventdata);
/*******************************************************************************
 * main
 ******************************************************************************/
int main(void) {
	/* Init board hardware.*/
	lcd_init();
	lcd_switch_to(LCD_DPY);
	/* Initialize mailbox, send EVT_CORE_UP to notify core 0 that core 1 is up
	 * and ready to work.
	 */
	mb_init();
	mb_push_evt(EVT_CORE_UP, true);

	Event ev;
	short x0, x1, y0, y1;
	int n;
	DC dc;

	lcd_get_default_DC(&dc);
	for (;;) {
		while (!next_event(&ev)) {
		}
		switch (ev.event & EVT_MASK) {
		case EVT_FORECOLOR:
			dc.fcolor = (Color) (ev.event & 0x0000FFFF);
			mb_push_evt(EVT_RETVAL, true);
			break;
		case EVT_DRAWLINES:
			uint32_t offset = (ev.event >> 11) & 0x1FFF;
			n = ev.event & 0x7FF;

			SPoint *p = &shdata[offset];

			x0 = p[0].x;
			y0 = p[0].y;
			for (int i = 1; i < n; i++) {
				x1 = p[i].x;
				y1 = p[i].y;
				lcd_line(x0, y0, x1, y1, dc.fcolor);
				x0 = x1;
				y0 = y1;

			}
			mb_push_evt(EVT_RETVAL, true);
			break;
		case EVT_DRAWSEGMENTS: {
			uint32_t offset = (ev.event >> 11) & 0x1FFF;
			int n = ev.event & 0x7FF;

			SPoint *p = &shdata[offset];

			Color c = (Color) p[2 * n].x;
			for (int i = 0; i < n; i++) {
				x0 = p[2 * i].x;
				y0 = p[2 * i].y;
				x1 = p[2 * i + 1].x;
				y1 = p[2 * i + 1].y;

				lcd_line(x0, y0, x1, y1, c);
			}
			mb_push_evt(EVT_RETVAL, true);
			break;
		}
		case EVT_DRAWPATH: {
			uint32_t offset = (ev.event >> 11) & 0x1FFF;
			int n = ev.event & 0x7FF;

			SPoint *p = &shdata[offset];

			line_t ltype = (line_t) p[n].x;
			marker_t mtype = (marker_t) p[n].y;
			int msize = p[n + 1].x;
			int pyorg = p[n + 1].y;
			int pxmin = p[n + 2].x;
			int pxmax = p[n + 2].y;
			int pymin = p[n + 3].x;
			int pymax = p[n + 3].y;
			Color c = (Color) p[n + 4].x;

			switch (ltype) {
			case L_SOLID:
			case L_DOTTED:
			case L_DASHED:
				x0 = p[0].x;
				y0 = p[0].y;
				for (int i = 1; i < n; i++) {
					x1 = p[i].x;
					y1 = p[i].y;
					lcd_line(x0, y0, x1, y1, c);
					x0 = x1;
					y0 = y1;
				}
				break;
			case L_BAR:
				for (int i = 0; i < n - 1; i++) {
					if (p[i].y >= pyorg) {
						lcd_draw_rect(p[i].x, pyorg, p[i + 1].x - p[i].x + 1,
								p[i].y - pyorg + 1, c);
					} else {
						lcd_draw_rect(p[i].x, p[i].y, p[i + 1].x - p[i].x + 1,
								pyorg - p[i].y + 1, c);
					}
				}
				break;
			case L_COMB:
				for (int i = 0; i < n; i++) {
					lcd_line(p[i].x, pyorg, p[i].x, p[i].y, c);
				}
				break;
			case L_NONE:
			default:
				break;
			}

			if (mtype != M_NONE) {
				for (int i = 0; i < n; i++) {
					if (p[i].x > pxmin && p[i].x < pxmax && p[i].y > pymin
							&& p[i].y < pymax) {
						switch (mtype) {
						case M_CROSS:
							lcd_draw_line(p[i].x - msize / 2,
									p[i].y - msize / 2, p[i].x + msize / 2,
									p[i].y + msize / 2, c);
							lcd_draw_line(p[i].x - msize / 2,
									p[i].y + msize / 2, p[i].x + msize / 2,
									p[i].y - msize / 2, c);
							break;
						case M_PLUS: {
							lcd_draw_line(p[i].x - msize / 2, p[i].y,
									p[i].x + msize / 2, p[i].y, c);
							lcd_draw_line(p[i].x, p[i].y + msize / 2,
									p[i].x, p[i].y - msize / 2, c);
							break;
						}
						case M_STAR: {
							short d1 = msize / 2 * 239 / 338;// d->msize/2*0.707
							short d2 = (msize / 2 + 1) * 239 / 338;	// d->msize/2*0.707
							lcd_draw_line(p[i].x - msize / 2, p[i].y,
									p[i].x + msize / 2, p[i].y, c);
							lcd_draw_line(p[i].x, p[i].y + msize / 2,
									p[i].x, p[i].y + msize / 2, c);
							lcd_draw_line(p[i].x - d1, p[i].y - d1,
									p[i].x + d2, p[i].y + d2, c);
							lcd_draw_line(p[i].x - d1, p[i].y + d1,
									p[i].x + d2, p[i].y - d2, c);
							break;
						}
						case M_SQUARE:
							lcd_draw_rect(p[i].x - msize / 2,
									p[i].y - msize / 2, msize, msize, c);
							break;
						case M_FSQUARE:
							lcd_fill_rect(p[i].x - msize / 2,
									p[i].y - msize / 2, msize, msize, c);
							break;
						case M_DOT:
						case M_CIRCLE:
						case M_FCIRCLE:
							lcd_draw_circle(p[i].x, p[i].y, msize / 2,
									c);
							break;
						case M_DIAMOND:
						case M_FDIAMOND:
							lcd_draw_line(p[i].x - msize / 2, p[i].y,
									p[i].x, p[i].y - msize / 2, c);
							lcd_draw_line(p[i].x, p[i].y - msize / 2,
									p[i].x + msize / 2, p[i].y, c);
							lcd_draw_line(p[i].x + msize / 2, p[i].y,
									p[i].x, p[i].y + msize / 2, c);
							lcd_draw_line(p[i].x, p[i].y + msize / 2,
									p[i].x - msize / 2, p[i].y, c);
							break;
						case M_ARROW:
							lcd_draw_line(p[i].x, p[i].y,
									p[i].x + msize * 13 / 38,
									p[i].y - msize, c);
							lcd_draw_line(p[i].x + msize * 13 / 38,
									p[i].y - msize,
									p[i].x - msize * 13 / 38,
									p[i].y - msize, c);
							lcd_draw_line(p[i].x - msize * 13 / 38,
									p[i].y - msize, p[i].x,
									p[i].y, c);
							break;
						case M_TRIANGLE:
						case M_FTRIANGLE:
							lcd_draw_line(p[i].x, p[i].y + msize / 2,
									p[i].x + msize / 2,
									p[i].y - msize / 2, c);
							lcd_draw_line(p[i].x + msize / 2,
									p[i].y - msize / 2,
									p[i].x - msize / 2,
									p[i].y - msize / 2, c);
							lcd_draw_line(p[i].x - msize / 2,
									p[i].y - msize / 2, p[i].x,
									p[i].y + msize / 2, c);
							break;
						default:
							break;
						}
					}
				}
			}
			mb_push_evt(EVT_RETVAL, true);
			break;
		}
		case EVT_FILLRECT: {
			uint32_t offset = (ev.event >> 11) & 0x1FFF;
			SPoint *p = &shdata[offset];
			lcd_fill_rect((uint16_t) p[0].x, (uint16_t) p[0].y,
					(uint16_t) p[1].x, (uint16_t) p[1].y, (Color) p[2].x);
			mb_push_evt(EVT_RETVAL, false);
			break;
		}
		case EVT_DRAWRECT: {
			uint32_t offset = (ev.event >> 11) & 0x1FFF;
			SPoint *p = &shdata[offset];
			lcd_draw_rect((uint16_t) p[0].x, (uint16_t) p[0].y,
					(uint16_t) p[1].x, (uint16_t) p[1].y, (Color) p[2].x);
			mb_push_evt(EVT_RETVAL, false);
			break;
		}
		case EVT_CLIP: {
			uint32_t offset = (ev.event >> 11) & 0x1FFF;
			SPoint *p = &shdata[offset];
			lcd_clip(p[0].x, p[0].y, p[1].x, p[1].y);
			mb_push_evt(EVT_RETVAL, false);
			break;
		}
		case EVT_UNCLIP: {
			lcd_unclip();
			mb_push_evt(EVT_RETVAL, false);
			break;
		}
		case EVT_SETALIGN: {
			uint32_t offset = (ev.event >> 11) & 0x1FFF;
			uint32_t alignment = (uint32_t) ((shdata[offset].y << 16)
					| (uint16_t) shdata[offset].x);
			lcd_set_alignment(&dc, alignment);
			mb_push_evt(EVT_RETVAL, false);
			break;
		}
		case EVT_SETDIR: {
			uint32_t offset = (ev.event >> 11) & 0x1FFF;
			uint32_t dir = (uint32_t) ((shdata[offset].y << 16)
					| (uint16_t) shdata[offset].x);
			lcd_set_direction(&dc, dir);
			mb_push_evt(EVT_RETVAL, false);
			break;
		}
		case EVT_SETFONT: {
			uint32_t offset = (ev.event >> 11) & 0x1FFF;
			uint8_t data = ev.event & 0xFF;

			static Font font;
			font.data = &data;
			font.width = shdata[offset].x;
			font.height = shdata[offset].y;

			lcd_set_font(&dc, &font);
			mb_push_evt(EVT_RETVAL, false);
			break;
		}
		case EVT_DRAWSTRING: {
			uint32_t offset = (ev.event >> 11) & 0x1FFF;
			SPoint *p = &shdata[offset];

			char *s = (char*) &p[1];
			lcd_draw_string(&dc, p[0].x, p[0].y, s);
			mb_push_evt(EVT_RETVAL, false);
			break;
		}
		default:
			break;
		}
	}
	return 0;
}
#endif

