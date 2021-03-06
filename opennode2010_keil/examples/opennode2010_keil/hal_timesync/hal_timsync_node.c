/*******************************************************************************
 * This file is part of OpenWSN, the Open Wireless Sensor Network Platform.
 *
 * Copyright (C) 2005-2010 zhangwei(TongJi University)
 *
 * OpenWSN is a free software; you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 or (at your option) any later version.
 *
 * OpenWSN is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * For non-opensource or commercial applications, please choose commercial license.
 * Refer to OpenWSN site http://code.google.com/p/openwsn/ for more detail.
 *
 * For other questions, you can contact the author through email openwsn#gmail.com
 * or the mailing address: Dr. Wei Zhang, Dept. of Control, Dianxin Hall, TongJi
 * University, 4800 Caoan Road, Shanghai, China. Zip: 201804
 *
 ******************************************************************************/

#define CONFIG_NIOACCEPTOR_RXQUE_CAPACITY 1
#define CONFIG_NIOACCEPTOR_TXQUE_CAPACITY 1

/*******************************************************************************
 * aloha_recv
 * The receiving test program based on ALOHA medium access control. It will try 
 * to receive the frames to itself, and then sent a character to the computer 
 * through UART as a indication. 
 *
 * @state
 *	still in developing. test needed
 *
 * @author Shi-Miaojing on 20090801
 *	- first created
 * @modified by  ShiMiaojing but not tested   
 *  - some wrong with assert.h  so  interupt is wrong.
 * @modified by zhangwei on 20090804
 *	- revisioin. compile passed.
 *modified  by ShMiaojing
 *modified by ShimMiaojing  test ok add cc2420_open and modifeid output_openframe 
 *but about macro define-config_test_listenner may be somewhat wrong but both two way works
 *
 * @modified by zhangwei on 2010520
 *  - upgraded to winavr20090313
 ******************************************************************************/


#define CONFIG_NIOACCEPTOR_RXQUE_CAPACITY 1
#define CONFIG_NIOACCEPTOR_TXQUE_CAPACITY 1
#define MAX_IEEE802FRAME154_SIZE          128
#include "apl_foundation.h"
#include "openwsn/hal/opennode2010/cm3/core/core_cm3.h"
#include "openwsn/hal/hal_mcu.h"
#include "openwsn/hal/hal_cpu.h"
#include "openwsn/hal/hal_configall.h"
#include <stdlib.h>
#include <string.h>
#include "openwsn/hal/hal_foundation.h"
#include "openwsn/rtl/rtl_foundation.h"
#include "openwsn/rtl/rtl_frame.h"
#include "openwsn/rtl/rtl_debugio.h"
#include "openwsn/rtl/rtl_ieee802frame154.h"
#include "openwsn/rtl/rtl_random.h"
#include "openwsn/hal/hal_cpu.h"
#include "openwsn/hal/hal_led.h"
#include "openwsn/hal/hal_assert.h"
#include "openwsn/hal/hal_uart.h"
#include "openwsn/hal/hal_cc2520.h"
#include "openwsn/hal/hal_debugio.h"
#include "openwsn/svc/svc_nio_aloha.h"
#include "openwsn/hal/hal_timesync.h"
#include "openwsn/hal/hal_rtc.h"
#include "openwsn/hal/hal_interrupt.h"


#define CONFIG_DEBUG


#define CONFIG_TEST_LISTENER  
#undef  CONFIG_TEST_LISTENER  

#define CONFIG_TEST_ADDRESSRECOGNITION
#define CONFIG_TEST_ACK

#define CONFIG_ALOHA_PANID			0x0001
#define CONFIG_ALOHA_LOCAL_ADDRESS	0x02
#define CONFIG_ALOHA_REMOTE_ADDRESS	0x00
#define CONFIG_ALOHA_CHANNEL		11

#define BROADCAST_ADDRESS			0xFFFF


#define NAC_SIZE NIOACCEPTOR_HOPESIZE(CONFIG_NIOACCEPTOR_RXQUE_CAPACITY,CONFIG_NIOACCEPTOR_TXQUE_CAPACITY)

#define VTM_RESOLUTION                          5


#define NAC_SIZE NIOACCEPTOR_HOPESIZE(CONFIG_NIOACCEPTOR_RXQUE_CAPACITY,CONFIG_NIOACCEPTOR_TXQUE_CAPACITY)
//static TiCc2520Adapter		                    m_cc;//todo 用2520.c中的全局变量
static TiFrameRxTxInterface                     m_rxtx;
static char                                     m_nacmem[NAC_SIZE];
static TiAloha                                  m_aloha;
static TiTimerAdapter                           m_timer2;
static char                                     m_rxbuf[FRAME_HOPESIZE(MAX_IEEE802FRAME154_SIZE)];
static char                                     m_mactxbuf[FRAME_HOPESIZE(MAX_IEEE802FRAME154_SIZE)];
TiRtcAdapter                                    m_rtc;
TiTimeSyncAdapter                               m_syn;
TiCc2520Adapter                                 m_cc;

static TiUartAdapter		m_uart;




#ifdef CONFIG_TEST_LISTENER
static void _aloha_listener( void * ccptr, TiEvent * e );
#endif

static void recvnode(void);
//void RTC_IRQHandler(void);
static void _rtc_handler(void * object, TiEvent * e);

int main(void)
{
	recvnode();
}

void recvnode(void)
{
    TiCc2520Adapter * cc;
    TiFrameRxTxInterface * rxtx;
    TiNioAcceptor        * nac;
    TiAloha * mac;
    TiTimerAdapter   *timer2;
    TiFrame * rxbuf;
    TiFrame * mactxbuf;
    char * pc;
    uint8 len;
    TiRtcAdapter * rtc;
    TiTimeSyncAdapter *syn;
		TiUartAdapter * uart;


    uint8 i, seqid=0, option;

    //__disable_irq();
    led_open();
    led_on( LED_ALL );
    hal_delayms( 500 );
    led_off( LED_ALL );

    cc = cc2520_construct( (char *)(&m_cc), sizeof(TiCc2520Adapter) );
    nac = nac_construct( &m_nacmem[0], NAC_SIZE );//todo
    mac = aloha_construct( (char *)(&m_aloha), sizeof(TiAloha) );
    timer2= timer_construct(( char *)(&m_timer2),sizeof(TiTimerAdapter));

   	uart = uart_construct( (void *)&m_uart, sizeof(TiUartAdapter) );
    uart = uart_open( uart,0, 9600, 8, 1, 0 );


    cc2520_open(cc, 0, NULL, NULL, 0x00 );
    rxtx = cc2520_interface( cc, &m_rxtx );

    hal_assert( rxtx != NULL );

    timer2 = timer_open( timer2, 2, NULL, NULL, 0x00 ); 

    timer_setinterval( timer2,1000,7999);


    nac_open( nac, rxtx, CONFIG_NIOACCEPTOR_RXQUE_CAPACITY, CONFIG_NIOACCEPTOR_TXQUE_CAPACITY);

    mactxbuf = frame_open( (char*)(&m_mactxbuf), FRAME_HOPESIZE(MAX_IEEE802FRAME154_SIZE), 0, 0, 0 );

    aloha_open( mac,rxtx,nac, CONFIG_ALOHA_CHANNEL, CONFIG_ALOHA_PANID, 
        CONFIG_ALOHA_LOCAL_ADDRESS, timer2 , NULL, NULL, 0x00 );
    mac->txbuf = mactxbuf;
 
	cc2520_setchannel( cc, CONFIG_ALOHA_CHANNEL );
	cc2520_rxon( cc );							            // enable RX mode
	cc2520_setpanid( cc, CONFIG_ALOHA_PANID );					// network identifier, seems no use in sniffer mode
	cc2520_setshortaddress( cc, CONFIG_ALOHA_LOCAL_ADDRESS );	// in network address, seems no use in sniffer mode
	cc2520_enable_autoack( cc );
 
    rxbuf = frame_open( (char*)(&m_rxbuf), FRAME_HOPESIZE(MAX_IEEE802FRAME154_SIZE), 3, 20, 0 );

    rtc = rtc_construct( (void *)(&m_rtc),sizeof(m_rtc));
    rtc = rtc_open(rtc,NULL,NULL,1,1);
    rtc_setprscaler( rtc,327);//rtc_setprscaler( rtc,32767);
    syn =  hal_tsync_open( &m_syn,rtc);
    nac_set_timesync_adapter( nac, syn);
    hal_attachhandler(  INTNUM_RTC, _rtc_handler,rtc );
    rtc_start( rtc);

    //dbc_putchar(0x11);
	uart_putchar( uart, 0x11);

    #ifdef CONFIG_TEST_ACK
    //fcf = OPF_DEF_FRAMECONTROL_DATA_ACK; 
	#else
    //fcf = OPF_DEF_FRAMECONTROL_DATA_NOACK; 
	#endif



	/* Wait for listener action. The listener function will be called by the TiCc2420Adapter
	 * object when a frame arrives */
	#ifdef CONFIG_TEST_LISTENER	
	while (1) {}
	#endif
    
	/* Query the TiCc2420Adapter object if there's no listener */
	#ifndef CONFIG_TEST_LISTENER
	while(1) 
	{	
		char * ptr;//todo for testing
        frame_reset( rxbuf, 3, 20, 0 );
		len = aloha_recv( mac, rxbuf, 0x00 );        
		if (len > 0)
		{   
            //frame_moveouter( rxbuf );
            pc = frame_startptr( rxbuf);
            for ( i=0;i< frame_length( rxbuf);i++)
            {
				uart_putchar( uart, pc[i]);
            }
          // frame_moveinner( rxbuf );
        }

		aloha_evolve(mac, NULL );
	}
	#endif

    frame_close( rxbuf );
    aloha_close( mac );
    cc2520_close( cc );
}
/*
void RTC_IRQHandler(void)
{
    if (RTC_GetITStatus(RTC_IT_SEC) != RESET)
    {
        /* Clear the RTC Second interrupt */
/*
        RTC_ClearITPendingBit(RTC_IT_SEC);
        m_rtc.currenttime++;
        led_toggle( LED_RED);
    }
}
*/
static void _rtc_handler(void * object, TiEvent * e)
{
    if (RTC_GetITStatus(RTC_IT_SEC) != RESET)
    {
        /* Clear the RTC Second interrupt */
        RTC_ClearITPendingBit(RTC_IT_SEC);
        m_rtc.currenttime++;
        led_toggle( LED_RED);
    }
}

#ifdef CONFIG_TEST_LISTENER
void _aloha_listener( void * owner, TiEvent * e )
{
	TiAloha * mac = &m_aloha;
    TiFrame * frame = (TiFrame *)m_rxbufmem;
    uint8 len;

	//dbc_putchar( 0xF4 );
	uart_putchar( uart, 0xF4);
	led_toggle( LED_RED );
	while (1)
	{
       	len = aloha_recv( mac, frame, 0x00 );
		if (len > 0)
		{    
            frame_moveouter( frame );
            _output_frame( frame, NULL );
            frame_moveinner( rxbuf );
			led_toggle( LED_RED );

			/* warning: You cannot wait too long in the listener. Because in the 
			 * current version, the listener is still run in interrupt mode. 
			 * you may encounter unexpect error at the application error in real-time
			 * systems. However, the program should still work properly even the 
			 * delay time is an arbitrary value here. No error are allowed in this case. 
			 *
			 * => That's why we cannot use hal_delay() to control the LED to make
			 * it observable for human eye. 
			 */
			// hal_delay( 500 );
			break;
        }
	}
}
#endif


