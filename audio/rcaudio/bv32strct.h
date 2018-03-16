/*****************************************************************************/
/* BroadVoice(R)32 (BV32) Floating-Point ANSI-C Source Code                  */
/* Revision Date: October 5, 2012                                            */
/* Version 1.2                                                               */
/*****************************************************************************/


/*****************************************************************************
  bv32strct.h : BV32 data structures

  $Log$
******************************************************************************/

#ifndef  BV32STRCT_H
#define  BV32STRCT_H

struct BV32_Decoder_State {
Float	stsym[LPCO];
Float	ltsym[LTMOFF];
Float	lsppm[LPCO*LSPPORDER];
Float	lgpm[LGPORDER];
Float	lsplast[LPCO];
Float	dezfm[PFO];
Float	depfm[PFO];
short	cfecount;
UWord32 idum;
Float scplcg;
Float per;
Float E;
Float atplc[LPCO+1];
short	pp_last;
Float	prevlg[2];
Float	lgq_last;
Float	bq_last[3];
Float lmax;           /* level-adaptation */
Float lmin;
Float lmean;
Float x1;
Float level;
short nclglim;
short lctimer;
};

struct BV32_Encoder_State {
Float	x[XOFF];
Float	xwd[XDOFF];		/* memory of DECF:1 decimated version of xw() */
Float	dq[XOFF];		/* quantized short-term pred error */
Float	dfm[DFO];		/* decimated xwd() filter memory */
Float	stpem[LPCO];		/* ST Pred. Error filter memory, low-band */
Float	stwpm[LPCO];		/* ST Weighting all-Pole Memory, low-band */
Float	stnfm[LPCO];		/* ST Noise Feedback filter Memory, Lowband */
Float	stsym[LPCO];		/* ST SYnthesis filter Memory, Lowband	*/
Float	ltsym[MAXPP1+FRSZ];	/* long-term synthesis filter memory */
Float	ltnfm[MAXPP1+FRSZ];	/* long-term noise feedback filter memory */
Float	lsppm[LPCO*LSPPORDER];	/* LSP Predictor Memory */
Float	allast[LPCO+1];
Float	lsplast[LPCO];
Float	lgpm[LGPORDER];
Float	hpfzm[HPO];
Float	hpfpm[HPO];
Float	prevlg[2];
Float	lmax;			/* level-adaptation */
Float	lmin;
Float	lmean;
Float	x1;
Float	level;
int cpplast;		/* pitch period pf the previous frame */
};

struct BV32_Bit_Stream {
short   lspidx[3];
short   ppidx;      /* 9 bit */
short   bqidx;
short   gidx[2];
short   qvidx[NVPSF];
};

#endif /* BV32STRCT_H */

