/*****************************************************************************/
/* BroadVoice(R)32 (BV32) Floating-Point ANSI-C Source Code                  */
/* Revision Date: October 5, 2012                                            */
/* Version 1.2                                                               */
/*****************************************************************************/

/*****************************************************************************
  bitpack.h: BV32 bit packing routines

  $Log$
******************************************************************************/

#ifndef BITPACK_H
#define BITPACK_H

void BV32_BitPack(UWord8 * PackedStream, struct BV32_Bit_Stream * BitStruct);
void BV32_BitUnPack(UWord8 * PackedStream, struct BV32_Bit_Stream * BitStruct);

#endif

