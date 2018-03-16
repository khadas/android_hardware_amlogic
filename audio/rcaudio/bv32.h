/*****************************************************************************/
/* BroadVoice(R)32 (BV32) Floating-Point ANSI-C Source Code                  */
/* Revision Date: October 5, 2012                                            */
/* Version 1.2                                                               */
/*****************************************************************************/


/*****************************************************************************
  bv32.h :

  $Log$
******************************************************************************/

extern void Reset_BV32_Coder(
struct BV32_Encoder_State *cs);

extern void BV32_Encode(
struct BV32_Bit_Stream *bs,
struct BV32_Encoder_State *cs,
short  *inx);

extern void Reset_BV32_Decoder(
struct BV32_Decoder_State *ds);

extern void BV32_Decode(
struct BV32_Bit_Stream     *bs,
struct BV32_Decoder_State  *ds,
short	*out);

extern void BV32_PLC(
struct  BV32_Decoder_State   *ds,
short	*out);

