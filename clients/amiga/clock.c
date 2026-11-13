/* Isolated from BSD socket headers, whose timeval conflicts with timer.device. */
#include <stdint.h>
#include <exec/types.h>
#include <devices/timer.h>
#include <proto/exec.h>
#include <proto/timer.h>
struct Device *TimerBase;
static struct MsgPort *port;
static struct timerequest *request;
int amiga_clock_init(void){
 port=CreateMsgPort();if(!port)return 0;
 request=(struct timerequest*)CreateIORequest(port,sizeof(*request));if(!request)return 0;
 if(OpenDevice(TIMERNAME,UNIT_MICROHZ,(struct IORequest*)request,0))return 0;
 TimerBase=(struct Device*)request->tr_node.io_Device;return 1;
}
void amiga_clock_close(void){if(TimerBase)CloseDevice((struct IORequest*)request);TimerBase=0;if(request)DeleteIORequest((struct IORequest*)request);if(port)DeleteMsgPort(port);request=0;port=0;}
uint64_t amiga_now_ms(void){struct EClockVal value;ULONG frequency=ReadEClock(&value);return frequency?(((uint64_t)value.ev_hi<<32)|value.ev_lo)*1000/frequency:0;}
