//**************************************************************************
//**
//** NET_MSG.C
//**
//** Buffer overflow checks *ARE NOT* made.
//** The caller must know what it's doing.
//** These don't provide the casual flexibility of the Quake messages
//** but at least they work. :-)
//**
//**************************************************************************

// HEADER FILES ------------------------------------------------------------

#include "de_base.h"
#include "de_network.h"
#if _DEBUG
#include "de_console.h"
#endif

// MACROS ------------------------------------------------------------------

// TYPES -------------------------------------------------------------------

// EXTERNAL FUNCTION PROTOTYPES --------------------------------------------

// PUBLIC FUNCTION PROTOTYPES ----------------------------------------------

// PRIVATE FUNCTION PROTOTYPES ---------------------------------------------

// EXTERNAL DATA DECLARATIONS ----------------------------------------------

// PUBLIC DATA DEFINITIONS -------------------------------------------------

// PRIVATE DATA DEFINITIONS ------------------------------------------------

// CODE --------------------------------------------------------------------

void Msg_Begin(int type)
{
	netBuffer.cursor = netBuffer.msg.data;
	netBuffer.length = 0;
	netBuffer.msg.type = type;
}

void Msg_WriteByte(byte b)
{
	*netBuffer.cursor++ = b;
}

void Msg_WriteShort(short w)
{
	*(short*) netBuffer.cursor = w;
	netBuffer.cursor += 2;
}

//==========================================================================
// Msg_WritePackedShort
//	Only 15 bits can be used for the number because the high bit of the 
//	lower byte is used to determine whether the upper byte follows or not.
//==========================================================================
void Msg_WritePackedShort(short w)
{
	if(w < 0x80) // Can the number be represented with 7 bits?
		Msg_WriteByte(w);
	else
	{
		Msg_WriteByte(0x80 | w);
		Msg_WriteByte(w >> 7); // Highest bit is lost.
	}
}

void Msg_WriteLong(int l)
{
	*(int*) netBuffer.cursor = l;
	netBuffer.cursor += 4;
}

void Msg_Write(void *src, int len)
{
	memcpy(netBuffer.cursor, src, len);
	netBuffer.cursor += len;
}

byte Msg_ReadByte()
{
#ifdef _DEBUG
	if(Msg_Offset() >= netBuffer.length) 
		Con_Error("Packet read overflow!\n");
#endif
	return *netBuffer.cursor++;
}

short Msg_ReadShort()
{
#ifdef _DEBUG
	if(Msg_Offset() >= netBuffer.length) 
		Con_Error("Packet read overflow!\n");
#endif
	netBuffer.cursor += 2;
	return *(short*) (netBuffer.cursor-2);
}

//==========================================================================
// Msg_ReadPackedShort
//	Only 15 bits can be used for the number because the high bit of the 
//	lower byte is used to determine whether the upper byte follows or not.
//==========================================================================
short Msg_ReadPackedShort()
{
	short pack = *netBuffer.cursor++;
	if(pack & 0x80) 
	{
		pack &= ~0x80;
		pack |= (*netBuffer.cursor++) << 7;
	}
	return pack;
}

int Msg_ReadLong()
{
#ifdef _DEBUG
	if(Msg_Offset() >= netBuffer.length) 
		Con_Error("Packet read overflow!\n");
#endif
	netBuffer.cursor += 4;
	return *(int*) (netBuffer.cursor-4);
}

void Msg_Read(void *dest, int len)
{
#ifdef _DEBUG
	if(Msg_Offset() >= netBuffer.length) 
		Con_Error("Packet read overflow!\n");
#endif
	memcpy(dest, netBuffer.cursor, len);
	netBuffer.cursor += len;
}

int Msg_Offset(void)
{
	return netBuffer.cursor - netBuffer.msg.data;
}

void Msg_SetOffset(int offset)
{
	netBuffer.cursor = netBuffer.msg.data + offset;
}

int Msg_MemoryLeft(void)
{
	return NETBUFFER_MAXDATA - (netBuffer.cursor - netBuffer.msg.data);
}

boolean Msg_End(void)
{
	return (netBuffer.cursor-netBuffer.msg.data >= netBuffer.length);
}
