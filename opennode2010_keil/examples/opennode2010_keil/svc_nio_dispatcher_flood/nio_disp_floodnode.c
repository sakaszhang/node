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
 
/*******************************************************************************
 * node.c
 * general node in the network
 *
 * @attention
 * Usage:
 *  Before you use this program, you should change the node address macro LOCAL_ADDRESS.
 * Every node in the network should have unique address. 
 * 
 * @author zhangwei on 20061106
 *	- first created
 * @modified by zhangwei on 20090802
 *	- revision. compile passed.
 * @modified by Shi-Miaojing and Yan-Shixing (TongJi University) on 20090802
 *	- testing...
 * @modified by ShiMiaojing 
 *  - rehearse the whole text logic and some details that  may not be compatible 
 * 	with our recent our modification in other files  
 *  - compiled ok.
 * 	- test ok good performance on NOvember 17 2009 
 ******************************************************************************/

#ifdef CONFIG_DEBUG   
    #define GDEBUG
#endif 

#define CONFIG_NIOACCEPTOR_RXQUE_CAPACITY 1
#define CONFIG_NIOACCEPTOR_TXQUE_CAPACITY 1

#include "apl_foundation.h"
#include "openwsn/hal/hal_configall.h"
#include "openwsn/hal/hal_mcu.h"
#include <stdlib.h>
#include <string.h>
#include "openwsn/hal/hal_foundation.h"
#include "openwsn/hal/hal_cpu.h"
#include "openwsn/hal/hal_led.h"
#include "openwsn/hal/hal_assert.h"
#include "openwsn/hal/hal_uart.h"
#include "openwsn/hal/hal_cc2520.h"
#include "openwsn/hal/hal_debugio.h"
#include "openwsn/hal/hal_timer.h"
#include "openwsn/rtl/rtl_frame.h"
#include "openwsn/svc/svc_nio_mac.h"
#include "openwsn/svc/svc_nio_acceptor.h"
#include "openwsn/svc/svc_nio_flood.h"
#include "openwsn/svc/svc_nio_dispatcher.h"

//#define CONFIG_TEST_LISTENER  
#define CONFIG_TEST_ADDRESSRECOGNITION

//since I think there is no need to calling for an ACK 
//in mac_bradocast ACK requeest bit has been cleared. 
//#define CONFIG_TEST_ACK
 
#define PANID						0x0001
#define LOCAL_ADDRESS				0x0003
#define REMOTE_ADDRESS				0xffff
#define DEFAULT_CHANNEL				11

#define MAX_IEEE802FRAME154_SIZE    128

#define CONFIG_NIOACCEPTOR_RXQUE_CAPACITY 1
#define CONFIG_NIOACCEPTOR_TXQUE_CAPACITY 1

#define NAC_SIZE NIOACCEPTOR_HOPESIZE(CONFIG_NIOACCEPTOR_RXQUE_CAPACITY,CONFIG_NIOACCEPTOR_TXQUE_CAPACITY)

static TiFrameRxTxInterface         m_rxtx;
static char                         m_nacmem[NAC_SIZE];
static TiNioMac						m_mac;
static TiTimerAdapter               m_timer2;
static char                         m_rxbufmem[FRAME_HOPESIZE(MAX_IEEE802FRAME154_SIZE)];
static char                         m_macbufmem[FRAME_HOPESIZE(MAX_IEEE802FRAME154_SIZE)];
static TiFloodNetwork			    m_net;
static TiCc2520Adapter              m_cc;
static TiNodeBase					m_nodebase;
static TiNioNetLayerDispatcher      m_disp;
static TiUartAdapter 				m_uart;


#ifdef CONFIG_TEST_LISTENER
static void _aloha_listener( void * ccptr, TiEvent * e );
#endif

static void floodnode(void);

/******************************************************************************
 * attention: 
 *	The simple node needn't to process the frames to be routed, because the flood
 * module will deal with them. 
 *****************************************************************************/


int main(void)
{
	floodnode();
}

void floodnode(void)
{
    TiUartAdapter * uart;
    TiCc2520Adapter * cc;
	TiFrameRxTxInterface * rxtx;
	TiNioAcceptor * nac;
	TiTimerAdapter   *timer2;
    TiNioMac * mac;
	TiFloodNetwork * net;
	TiNodeBase * nodebase;
    TiNioNetLayerDispatcher * disp;
	TiFrame * frame;
    TiFrame * mactxbuf;
	char * msg = "welcome to flood node...";
	uint8  len;
	char * pc;
    uint8 i;
    uint16 remoteaddr;

    /***************************************************************************
	 * Device Startup and Initialization 
     **************************************************************************/
	 
	led_open(LED_RED);
	led_on(LED_RED);
	hal_delayms( 1000 );
	led_off( LED_RED );

    uart = uart_construct((void *)(&m_uart), sizeof(m_uart));
    uart = uart_open(uart, 0, 9600, 8, 1, 0);

    /***************************************************************************
	 * Flood Protocol Startup
     **************************************************************************/

	cc = cc2520_construct( (void *)(&m_cc), sizeof(TiCc2520Adapter) );
	nac = nac_construct( &m_nacmem[0], NAC_SIZE );
    mac = mac_construct( (char *)(&m_mac), sizeof(TiNioMac) );
    timer2 = timer_construct( (char *)(&m_timer2),sizeof(TiTimerAdapter) );
    disp = nio_dispa_construct( (void *)( &m_disp), sizeof( m_disp) );
	net = flood_construct( (void *)(&m_net), sizeof(TiFloodNetwork) );
    nodebase = nbase_construct( (void *)&m_nodebase,sizeof(m_nodebase));

	// open the transceiver driver. we use TiCc2420Adapter in this example.
	cc2520_open( cc, 0, 0x00 );
	rxtx = cc2520_interface( cc, &m_rxtx );
	hal_assert( rxtx != NULL );
	nac = nac_open( nac, rxtx, CONFIG_NIOACCEPTOR_RXQUE_CAPACITY, CONFIG_NIOACCEPTOR_TXQUE_CAPACITY);
	hal_assert( nac != NULL ); 
	
	// open the medium access control protocol for sending and receiving
	// should use a 16bit or 32bit timer for the MAC. 8 bit timer will lead to wrong
	// delay results.
	timer2 = timer_open( timer2, 2, NULL, NULL, 0x00 ); 

    
	#ifdef CONFIG_TEST_LISTENER
	mac = mac_open( mac, rxtx, nac, DEFAULT_CHANNEL, PANID, LOCAL_ADDRESS, timer2, _aloha_listener, NULL, 0x00);
	#else
	mac = mac_open( mac, rxtx, nac, DEFAULT_CHANNEL, PANID, LOCAL_ADDRESS, timer2, 0x00);
	#endif

    net = flood_open( net, disp, NULL, NULL, PANID, LOCAL_ADDRESS );
    nodebase = nbase_open( nodebase,PANID,LOCAL_ADDRESS,DEFAULT_CHANNEL);
	disp = nio_dispa_open( disp,nodebase,mac,NULL );

    nio_dispa_register(disp, FLOOD_PROTOCAL_IDENTIFIER, net, flood_rxhandler, flood_txhandler,flood_evolve);

	//todo 
	cc2520_setchannel( cc, DEFAULT_CHANNEL );
	cc2520_rxon( cc );							    // enable RX mode
	cc2520_setpanid( cc, PANID );					// network identifier, seems no use in sniffer mode
	cc2520_setshortaddress( cc, LOCAL_ADDRESS );	// in network address, seems no use in sniffer mode
	
	#ifndef CONFIG_TEST_LISTENER
	while(1) 
	{ 
		flood_evolve( net, NULL );

		frame = frame_open( (char*)(&m_rxbufmem), FRAME_HOPESIZE(MAX_IEEE802FRAME154_SIZE), 3, 30, 92 );		
		len = nio_dispa_recv( net->disp, &remoteaddr, frame, 0);

		if (len > 0)
		{ 
		    //frame_skipinner( frame,4,0);//todo 
			pc = frame_startptr( frame);
            //len = len -4;
            for ( i=0;i<len;i++)
            {
				uart_putchar(uart,pc[i]);
            }
			
			if (pc[0])
				led_on( LED_RED );

			else
				led_off( LED_RED );
			//frame_moveouter( frame );
			
		}

        mac_evolve( mac, NULL );
        flood_evolve( net, NULL );
        nio_dispa_evolve(net->disp, NULL);
	}
	#endif
}

#ifdef CONFIG_TEST_LISTENER
void _flood_listener( void * owner, TiEvent * e )
{
	TiFloodNetwork * net = &(m_net);
    TiFrame * f = (TiFrame *)g_rxbufmem;
					  
	dbc_putchar( 0x77);
	led_toggle( LED_RED );
	while (1)
	{
		f = frame_open( (char*)(&m_rxbufmem), FRAME_HOPESIZE(MAX_IEEE802FRAME154_SIZE), 3, 20, 0 );		
		if (flood_recv(net, f, 0x00) > 0)
		{
			pc = frame_startptr( frame);
			if (pc[4])
				led_on( LED_RED );
			else
				led_off( LED_RED );

			frame_moveouter( frame );
			//ieee802frame154_dump( frame);
		}
	}
}
#endif


