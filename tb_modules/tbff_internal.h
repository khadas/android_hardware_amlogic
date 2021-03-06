/*
 *
 * Copyright (c) 2015 Amlogic, Inc. All rights reserved.
 *
 * This source code is subject to the terms and conditions defined in the
 * file 'LICENSE' which is part of this source code package.
 *
 * tbff_internal related tb_module defines
 *
 */

#ifndef TBFF_INTERNAL_H_
#define TBFF_INTERNAL_H_

struct _tbff_stats {
	int buf_row;
	int buf_col;

	/* set the registers */
	int reg_polar5_v_mute;
	int reg_polar5_h_mute;
	int reg_polar5_ro_reset;
	int reg_polar5_mot_thrd;
	int reg_polar5_edge_rat;
	int reg_polar5_ratio;
	int reg_polar5_ofset;

	/* read-only for tff/bff decision */
	int ro_polar5_numofpix;
	int ro_polar5_f4_m2;
	int ro_polar5_f4_p2;
	int ro_polar5_f6_m2;
	int ro_polar5_f6_p2;
	int ro_polar5_f2_m2;
	int ro_polar5_f2_p2;
	int ro_polar5_f4_i5;
	int ro_polar5_f4_i3;
};

extern void tbff_stats_inital(struct _tbff_stats *pReg, int irow, int icol);
extern void get_tbff_stats(unsigned long *in, struct _tbff_stats *pReg);
extern void tbff_fwalg_inital(int init_mode);
extern int tbff_fwalg(struct _tbff_stats *pReg, int fld_id, int is_tff, int frm, int skip_flg, int print_flg);
extern int get_tbff_majority_flg(void);
extern const char *get_version_info(void);

#endif
