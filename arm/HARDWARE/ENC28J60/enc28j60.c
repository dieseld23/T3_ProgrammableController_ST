#include <stdio.h>
#include "spi.h"
#include "delay.h"
#include "timerx.h"
#include "enc28j60.h"	 
#include "product.h"

static u8 ENC28J60BANK;
static u32 NextPacketPtr;

u8 flag_reintial_tcpip;
extern U16_T count_reintial_tcpip;
u8 count_tcp_hwErr_rx;
u8 count_tcp_hwErr_tx;
void tcpip_intial(void);
void QuickSoftReset(void);
//Reset the ENC28J60
//Includes SPI initialisation, IO initialisation and so on
void ENC28J60_Reset(void)
{
	GPIO_InitTypeDef GPIO_InitStructure;
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB | RCC_APB2Periph_GPIOC | RCC_APB2Periph_GPIOE, ENABLE);
	
	// RST E6
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOE, &GPIO_InitStructure);
	
	// CS B12
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_12;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOB, &GPIO_InitStructure);
	
	// INT C4
	GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
	GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
	GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOC, &GPIO_InitStructure);
	
	
//	RCC->APB2ENR |= 1 << 3;     //Enable the PORTB clock, INT=PA0, RST=PA1, CS=PA4 
//	
//	GPIOB->CRL &= 0XFFF0FF00; 
//	GPIOB->CRL |= 0X00030038;
//	
//	GPIOB->CRL &= 0XFFF0FF00; 
//	GPIOB->CRL |= 0X00030038;						//PA1,PA4 push-pull output, PA0 input 	    
//	GPIOB->ODR |= (1 << 0) | (1 << 1) | (1 << 4);	//PA1,PA4,PA0 pull-up
	
//	//PC4, PB7 and PB12 are driven high here to stop other SPI devices interfering.
//	//Because they share a single SPI port. 
//	GPIOB->CRH &= 0XFFF0FFFF; 
//	GPIOB->CRH |= 0X00030000;	//PB12 push-pull 	    
//	GPIOB->ODR |= 1 << 12;     	//PB12 pull-up
//	GPIOB->CRL &= 0X0FFFFFFF; 
//	GPIOB->CRL |= 0X30000000;	//PB7 push-pull 	    
//	GPIOB->ODR |= 1 << 7;     	//PB7 pull-up
//	GPIOB->CRL &= 0X0FFFFFFF; 
//	GPIOB->CRL |= 0X30000000;	//PB7 push-pull 	    
//	GPIOB->ODR |= 1 << 7;     	//PB7 pull-up
//	RCC->APB2ENR |= 1 << 4;     //Enable the PORTC clock, other SPI devices CS=PC4
//	GPIOC->CRL &= 0XFFF0FFFF; 
//	GPIOC->CRL |= 0X00030000;	//PC4 push-pull 	    
//	GPIOC->ODR |= 1 << 4;     	//PC4 pull-up
	
	
//	ENC28J60_RST = 0;
	GPIO_ResetBits(GPIOE, GPIO_Pin_6);
	delay_ms(10);
//	ENC28J60_RST = 1;
	GPIO_SetBits(GPIOE, GPIO_Pin_6);	
	delay_ms(10);

//	ENC28J60_RST = 0;
	GPIO_ResetBits(GPIOE, GPIO_Pin_6);
	delay_ms(10);
//	ENC28J60_RST = 1;
	GPIO_SetBits(GPIOE, GPIO_Pin_6);	
	delay_ms(10);

//	ENC28J60_RST = 0;
	GPIO_ResetBits(GPIOE, GPIO_Pin_6);
	delay_ms(10);
//	ENC28J60_RST = 1;
	GPIO_SetBits(GPIOE, GPIO_Pin_6);	
	delay_ms(10);
	
	GPIO_ResetBits(GPIOE, GPIO_Pin_6);
	delay_ms(10);
	GPIO_SetBits(GPIOE, GPIO_Pin_6);	
	delay_ms(10);
}

//Read an ENC28J60 register (with opcode) 
//op: opcode
//addr: register address / parameter
//Return: the data read
u8 ENC28J60_Read_Op(u8 op, u8 addr)
{
	u8 dat = 0;	 
	//ENC28J60_CS = 0;
//	printf("ENC28J60_Read_start");
	GPIO_ResetBits(GPIOB, GPIO_Pin_12);
	dat = op | (addr & ADDR_MASK);
	SPI2_ReadWriteByte(dat);
	dat = SPI2_ReadWriteByte(0xFF);
	
	//When reading a MAC/MII register only the second read returns the correct data - see page 29 of the manual
 	if(addr & 0x80)
		dat = SPI2_ReadWriteByte(0xFF);
	
	//ENC28J60_CS = 1;
	GPIO_SetBits(GPIOB, GPIO_Pin_12);
	return dat;
}

//Read an ENC28J60 register (with opcode) 
//op: opcode
//addr: register address
//data: parameter
void ENC28J60_Write_Op(u8 op, u8 addr, u8 _data)
{
	u8 dat = 0;	    
	//ENC28J60_CS = 0;
	GPIO_ResetBits(GPIOB, GPIO_Pin_12);
	dat = op | (addr & ADDR_MASK);
	SPI2_ReadWriteByte(dat);	  
	SPI2_ReadWriteByte(_data);
	//ENC28J60_CS = 1;
	GPIO_SetBits(GPIOB, GPIO_Pin_12);
}

//Read data from the ENC28J60 receive buffer
//len: number of bytes to read
//data: output buffer (a terminator is appended automatically)
void ENC28J60_Read_Buf(u32 len, u8* _data)
{
	//ENC28J60_CS = 0;
	GPIO_ResetBits(GPIOB, GPIO_Pin_12);
	SPI2_ReadWriteByte(ENC28J60_READ_BUF_MEM);
	while(len)
	{
		len--;			  
		*_data = (u8)SPI2_ReadWriteByte(0);
		_data++;
	}
	*_data = '\0';
	//ENC28J60_CS = 1;
	GPIO_SetBits(GPIOB, GPIO_Pin_12);
}

//Write data into the ENC28J60 transmit buffer
//len: number of bytes to write
//data: data buffer 
extern u16 Test[50];
void ENC28J60_Write_Buf(u32 len, u8* _data)
{
	//ENC28J60_CS = 0;
    GPIO_ResetBits(GPIOB, GPIO_Pin_12);	
	SPI2_ReadWriteByte(ENC28J60_WRITE_BUF_MEM);		 
	while(len)
	{
		len--;
		SPI2_ReadWriteByte(*_data);
		_data++;
	}
	//ENC28J60_CS = 1;
	GPIO_SetBits(GPIOB, GPIO_Pin_12);
}

//Select the ENC28J60 register bank
//ban: the bank to select
void ENC28J60_Set_Bank(u8 bank)
{								    
	if((bank & BANK_MASK) != ENC28J60BANK)	//Only switch when it differs from the current bank
	{				  
		ENC28J60_Write_Op(ENC28J60_BIT_FIELD_CLR, ECON1, (ECON1_BSEL1 | ECON1_BSEL0));
		ENC28J60_Write_Op(ENC28J60_BIT_FIELD_SET, ECON1, (bank & BANK_MASK) >> 5);
		ENC28J60BANK = (bank & BANK_MASK);
	}
}

//Read the given ENC28J60 register 
//addr: register address
//Return: the data read
u8 ENC28J60_Read(u8 addr)
{		
	ENC28J60_Set_Bank(addr);	//Select the bank	
	return ENC28J60_Read_Op(ENC28J60_READ_CTRL_REG, addr);
}

//Write data to the given ENC28J60 register
//addr: register address
//data: the data to write		 
void ENC28J60_Write(u8 addr, u8 _data)
{					  
	ENC28J60_Set_Bank(addr);		 
	ENC28J60_Write_Op(ENC28J60_WRITE_CTRL_REG, addr, _data);
}

//Write data to an ENC28J60 PHY register
//addr: register address
//data: the data to write		 
void ENC28J60_PHY_Write(u8 addr, u32 _data)
{
	u16 retry = 0;
	ENC28J60_Write(MIREGADR, addr);		//Set the PHY register address
	ENC28J60_Write(MIWRL, _data);		//Write data
	ENC28J60_Write(MIWRH, _data >> 8);		   
	while((ENC28J60_Read(MISTAT) & MISTAT_BUSY) && (retry < 0XFFF))
		retry++;						//Wait for the PHY write to finish		  
}

//Initialise the ENC28J60
//macaddr: the MAC address
//Return: 0 = initialisation succeeded;
//       1 = initialisation failed;
u8 ENC28J60_Init(u8* macaddr)
{		
	u16 retry = 0;
//	u8 temp ;	
	ENC28J60_Reset();
	
	while(!(ENC28J60_Read(ESTAT) & ESTAT_CLKRDY) && (retry < 500))			//Wait for the clock to settle
	{
		ENC28J60_Write_Op(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);		//Software reset, done inside the loop to improve the odds of a successful init.
		retry++;
//		printf("retry=%u\n\r",retry);
		delay_ms(10);
	}
	
//	while(retry < 500)
//	{
//		temp = ENC28J60_Read(ESTAT);
//		//printf("temp=%u retry=%u\n\r",temp,retry);
//		if(temp == ESTAT_CLKRDY)
//		{
//			break;
//		}
//		else
//		{
//			ENC28J60_Write_Op(ENC28J60_SOFT_RESET, 0, ENC28J60_SOFT_RESET);		//software reset, inside the loop to improve the odds of a successful init.
//			retry++;
//			delay_ms(10);
//		}
//	
//	}
	
	
	if(retry >= 500)
	{
//		printf("ENC28J60 initilise failed...\n");
		return 1;	//ENC28J60 initialisation failed
	}
	
	// do bank 0 stuff
	// initialize receive buffer
	// 16-bit transfers,must write low byte first
	// set receive buffer start address	   set the receive buffer address, 8K bytes
	NextPacketPtr = RXSTART_INIT;
	// Rx start
	//The receive buffer is a hardware-managed circular FIFO.
	//The register pairs ERXSTH:ERXSTL and ERXNDH:ERXNDL act as
	//pointers defining the buffer's size and its position in memory.
	//The bytes pointed to by ERXST and ERXND are both inside the FIFO.
	//As bytes are received from the Ethernet interface they are written
	//sequentially into the receive buffer. Once the location pointed to by ERXND
	//has been written, the hardware automatically writes the next byte to the
	//location pointed to by ERXST, so the receive hardware never writes outside
	//the FIFO.
	//Set the receive start byte
	ENC28J60_Write(ERXSTL, RXSTART_INIT & 0xFF);	
	ENC28J60_Write(ERXSTH, RXSTART_INIT >> 8);	  
	//The ERXWRPTH:ERXWRPTL registers define where in the FIFO the hardware
	//writes the bytes it receives. The pointer is read-only and is updated
	//automatically by the hardware once a packet has been received. The pointer can
	//Used to work out how much room is left in the FIFO, 8K-1500. 
	//Set the receive read pointer byte
	ENC28J60_Write(ERXRDPTL, RXSTART_INIT & 0xFF);
	ENC28J60_Write(ERXRDPTH, RXSTART_INIT >> 8);
	//Set the receive end byte
	ENC28J60_Write(ERXNDL, RXSTOP_INIT & 0xFF);
	ENC28J60_Write(ERXNDH, RXSTOP_INIT >> 8);
	//Set the transmit start byte
	ENC28J60_Write(ETXSTL, TXSTART_INIT & 0xFF);
	ENC28J60_Write(ETXSTH, TXSTART_INIT >> 8);
	//Set the transmit end byte
	ENC28J60_Write(ETXNDL, TXSTOP_INIT & 0xFF);
	ENC28J60_Write(ETXNDH, TXSTOP_INIT >> 8);
	// do bank 1 stuff,packet filter:
	// For broadcast packets we allow only ARP packtets
	// All other packets should be unicast only for our mac (MAADR)
	//
	// The pattern to match on is therefore
	// Type     ETH.DST
	// ARP      BROADCAST
	// 06 08 -- ff ff ff ff ff ff -> ip checksum for theses bytes=f7f9
	// in binary these poitions are:11 0000 0011 1111
	// This is hex 303F->EPMM0=0x3f,EPMM1=0x30
	//Receive filter
	//UCEN: unicast filter enable bit
	//When ANDOR = 1:
	//1 = packets whose destination does not match the local MAC address are discarded
	//0 = filter disabled
	//When ANDOR = 0:
	//1 = packets whose destination matches the local MAC address are accepted
	//0 = filter disabled
	//CRCEN: post-filter CRC check enable bit
	//1 = every packet with an invalid CRC is discarded
	//0 = the CRC is not checked
	//PMEN: pattern match filter enable bit
	//When ANDOR = 1:
	//1 = a packet must meet the pattern match conditions or it is discarded
	//0 = filter disabled
	//When ANDOR = 0:
	//1 = packets meeting the pattern match conditions are accepted
	//0 = filter disabled
	ENC28J60_Write(ERXFCON, ERXFCON_UCEN | ERXFCON_CRCEN | ERXFCON_BCEN);//ERXFCON_PMEN);
	ENC28J60_Write(EPMM0, 0x3f);
	ENC28J60_Write(EPMM1, 0x30);
	ENC28J60_Write(EPMCSL, 0xf9);
	ENC28J60_Write(EPMCSH, 0xf7);
	// do bank 2 stuff
	// enable MAC receive
	//bit 0 MARXEN: MAC receive enable bit
	//1 = the MAC may receive packets
	//0 = packet reception is disabled
	//bit 3 TXPAUS: pause control frame transmit enable bit
	//1 = the MAC may send pause control frames (flow control in full duplex)
	//0 = pause frame transmission is disabled
	//bit 2 RXPAUS: pause control frame receive enable bit
	//1 = transmission stops when a pause control frame is received (normal operation)
	//0 = received pause control frames are ignored
	ENC28J60_Write(MACON1, MACON1_MARXEN | MACON1_TXPAUS | MACON1_RXPAUS);
	// bring MAC out of reset
	//Clear the MARST bit in MACON2 to bring the MAC out of reset.
	ENC28J60_Write(MACON2, 0x00);
	// enable automatic padding to 60bytes and CRC operations
	//bit 7-5 PADCFG2:PADCFG0: automatic pad and CRC configuration bits
	//111 = pad every short frame to 64 bytes with zeros and append a valid CRC
	//110 = do not pad short frames automatically
	//101 = the MAC detects VLAN frames with a type field of 8100h and pads them to 64 bytes. If it is
	//not a VLAN frame it is padded to 60 bytes. A valid CRC is appended after padding
	//100 = do not pad short frames automatically
	//011 = pad every short frame to 64 bytes with zeros and append a valid CRC
	//010 = do not pad short frames automatically
	//001 = pad every short frame to 60 bytes with zeros and append a valid CRC
	//000 = do not pad short frames automatically
	//bit 4 TXCRCEN: transmit CRC enable bit
	//1 = whatever PADCFG says, the MAC appends a valid CRC to the end of the frame. If PADCFG calls for
	//a valid CRC to be appended, TXCRCEN must be set to 1.
	//0 = the MAC appends no CRC. The last 4 bytes are checked and an invalid CRC is reported in the transmit status vector.
	//bit 0 FULDPX: MAC full duplex enable bit
	//1 = the MAC runs in full duplex. PHCON1.PDPXMD must be set to 1.
	//0 = the MAC runs in half duplex. PHCON1.PDPXMD must be cleared.
	ENC28J60_Write_Op(ENC28J60_BIT_FIELD_SET, MACON3, MACON3_PADCFG0 | MACON3_TXCRCEN | MACON3_FRMLNEN | MACON3_FULDPX);
	// set inter-frame gap (non-back-to-back)
	//Configure the low byte of the non-back-to-back inter-packet gap register,
	//MAIPGL. Most applications program this register with 12h.
	//In half duplex the high byte of the non-back-to-back inter-packet gap
	//register, MAIPGH, should also be programmed. Most applications use 0Ch
	//for this register.
	ENC28J60_Write(MAIPGL, 0x12);
	ENC28J60_Write(MAIPGH, 0x0C);
	// set inter-frame gap (back-to-back)
	//Configure the back-to-back inter-packet gap register MABBIPG. Most
	//applications program it with 15h in full duplex and 12h in half
	//duplex.
	ENC28J60_Write(MABBIPG, 0x15);
	// Set the maximum packet size which the controller will accept
	// Do not send packets longer than MAX_FRAMELEN:
	// maximum frame length, 1500
	ENC28J60_Write(MAMXFLL, MAX_FRAMELEN & 0xFF);	
	ENC28J60_Write(MAMXFLH, MAX_FRAMELEN >> 8);
	// do bank 3 stuff
	// write MAC address
	// NOTE: MAC address in ENC28J60 is byte-backward
	//Set the MAC address
	ENC28J60_Write(MAADR5, macaddr[0]);	
	ENC28J60_Write(MAADR4, macaddr[1]);
	ENC28J60_Write(MAADR3, macaddr[2]);
	ENC28J60_Write(MAADR2, macaddr[3]);
	ENC28J60_Write(MAADR1, macaddr[4]);
	ENC28J60_Write(MAADR0, macaddr[5]);
	//Configure the PHY for full duplex; LEDB sources current
	ENC28J60_PHY_Write(PHCON1, PHCON1_PDPXMD);	 
	// no loopback of transmitted frames	 disable loopback
	//HDLDIS: PHY half duplex loopback disable bit
	//When PHCON1.PDPXMD = 1 or PHCON1.PLOOPBK = 1:
	//this bit is ignored.
	//When PHCON1.PDPXMD = 0 and PHCON1.PLOOPBK = 0:
	//1 = data to send goes out over the twisted pair interface only
	//0 = data to send is looped back to the MAC as well as going out over the twisted pair interface
	ENC28J60_PHY_Write(PHCON2, PHCON2_HDLDIS);
	// switch to bank 0
	//The ECON1 register
	//Register 3-1 shows ECON1, which controls the main functions of the
	//ENC28J60. It holds the receive enable, transmit request, DMA control
	//and bank select bits.	   
	ENC28J60_Set_Bank(ECON1);
	// enable interrutps
	//EIE: Ethernet interrupt enable register
	//bit 7 INTIE: global INT interrupt enable bit
	//1 = interrupt events may drive the INT pin
	//0 = all INT pin activity is disabled (the pin is always driven high)
	//bit 6 PKTIE: receive packet pending interrupt enable bit
	//1 = the receive packet pending interrupt is enabled
	//0 = the receive packet pending interrupt is disabled
	ENC28J60_Write_Op(ENC28J60_BIT_FIELD_SET, EIE,EIE_INTIE | EIE_PKTIE);
	// enable packet reception
	//bit 2 RXEN: receive enable bit
	//1 = packets passing the current filters are written to the receive buffer
	//0 = every received packet is ignored
	
	ENC28J60_Write_Op(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_RXEN);
	if(ENC28J60_Read(MAADR5) == macaddr[0])
		return 0;
	else
		return 1;
//	temp  = ENC28J60_Read(MAADR5);
////	printf("%u,%u",macaddr[0], temp);
//	if(temp == macaddr[0])
//		return 0;	//initialisation succeeded
//	else
//		return 1; 	  
}

//Read EREVID
u8 ENC28J60_Get_EREVID(void)
{
	//EREVID also holds version information. It is a read-only control
	//register holding a 5-bit identifier that gives the revision number of
	//the particular silicon
	return ENC28J60_Read(EREVID);
}


//#include "uip.h"
//Send a packet to the network through the ENC28J60
//len: the packet size
//packet: the packet
u8 ENC28J60_Packet_Send(u32 len, u8* packet)
{
	//Set the transmit buffer write pointer to the start
	ENC28J60_Write(EWRPTL, TXSTART_INIT & 0xFF);
	ENC28J60_Write(EWRPTH, TXSTART_INIT >> 8);
	//Set the TXND pointer to match the given packet size	   
	ENC28J60_Write(ETXNDL, (TXSTART_INIT + len) & 0xFF);
	ENC28J60_Write(ETXNDH, (TXSTART_INIT + len) >> 8);
	//Write the per-packet control byte (0x00 means use the MACON3 settings) 
	ENC28J60_Write_Op(ENC28J60_WRITE_BUF_MEM, 0, 0x00);
	//Copy the packet into the transmit buffer
	//printf("len:%d\r\n", len);	//watch the transmitted length
 	ENC28J60_Write_Buf(len, packet);
 	//Send the data to the network
	ENC28J60_Write_Op(ENC28J60_BIT_FIELD_SET, ECON1, ECON1_TXRTS);
	//Reset for the transmit logic problem. See Rev. B4 Silicon Errata point 12.
	if((ENC28J60_Read(EIR) & EIR_TXERIF))
	{
		ENC28J60_Write_Op(ENC28J60_BIT_FIELD_CLR, ECON1, ECON1_TXRTS);
		// added by chelsea
	  // hardware error	
		count_tcp_hwErr_tx++;
		if(count_tcp_hwErr_tx > 20) // reboot
			QuickSoftReset();
#if (ARM_MINI || ARM_CM5)
			//flag_reintial_tcpip = 1;
			//count_reintial_tcpip = 2;
		//Test[27]++;
		tcpip_intial();
#endif
		return 0;
		
	}
	else {
		count_tcp_hwErr_tx = 0;
	// change full duplex -> half duplex

	}
//	if((ENC28J60_Read(EIR)&EIR_TXERIF))
//		ENC28J60_Write_Op(ENC28J60_BIT_FIELD_CLR,ECON1,ECON1_TXRST); 
	return 1;
}

//Fetch one packet from the network
//maxlen: the largest packet that may be received
//packet: the packet buffer
//Return: the length of the packet received, in bytes									  
u32 ENC28J60_Packet_Receive(u32 maxlen, u8* packet)
{
	u32 rxstat;
	u32 len;

	if(ENC28J60_Read(EPKTCNT) == 0)
		return 0;  //Has a packet arrived?	
	//Set the receive buffer read pointer
	ENC28J60_Write(ERDPTL, (NextPacketPtr));
	ENC28J60_Write(ERDPTH, (NextPacketPtr) >> 8);	   
	// read the pointer to the next packet
	NextPacketPtr = ENC28J60_Read_Op(ENC28J60_READ_BUF_MEM, 0);
	NextPacketPtr |= ENC28J60_Read_Op(ENC28J60_READ_BUF_MEM, 0) << 8;
	//Read the packet length
	len = ENC28J60_Read_Op(ENC28J60_READ_BUF_MEM, 0);
	len |= ENC28J60_Read_Op(ENC28J60_READ_BUF_MEM, 0) << 8;
 	len -= 4; //Drop the CRC from the count
	//Read the receive status
	rxstat = ENC28J60_Read_Op(ENC28J60_READ_BUF_MEM, 0);
	rxstat |= ENC28J60_Read_Op(ENC28J60_READ_BUF_MEM, 0) << 8;
	//Clamp the receive length	
	if (len > maxlen - 1)
	{
		// hardware error	
		count_tcp_hwErr_rx++;	
		if(count_tcp_hwErr_rx > 20) // reboot
			QuickSoftReset();
#if (ARM_MINI || ARM_CM5)
			//flag_reintial_tcpip = 1;
			//count_reintial_tcpip = 5;
		tcpip_intial();
#endif
		
		len = maxlen - 1;
		return 0;
	}
	//Check for CRC and symbol errors
	// ERXFCON.CRCEN is at its default, so normally there is no need to check.
	if((rxstat & 0x80) == 0)
	{
		// hardware error	
		count_tcp_hwErr_rx++;	
		if(count_tcp_hwErr_rx > 20) // reboot
			QuickSoftReset();
#if (ARM_MINI || ARM_CM5)
//			flag_reintial_tcpip = 1;
//			count_reintial_tcpip = 5;
		Test[29]++;tcpip_intial();
#endif
		len = 0;	//Invalid
		return 0;
	}
	else
	{	
		count_tcp_hwErr_rx = 0;
		ENC28J60_Read_Buf(len, packet);//Copy the packet out of the receive buffer	    
	}
	//Move the RX read pointer to the start of the next received packet 
	//and free the memory we have just read
	ENC28J60_Write(ERXRDPTL, (NextPacketPtr));
	ENC28J60_Write(ERXRDPTH, (NextPacketPtr) >> 8);
	//Decrement the packet counter to show this packet has been taken 
 	ENC28J60_Write_Op(ENC28J60_BIT_FIELD_SET, ECON2, ECON2_PKTDEC);
	return(len);
}
