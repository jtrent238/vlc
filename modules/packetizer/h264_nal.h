/*****************************************************************************
 * Copyright © 2010-2014 VideoLAN
 *
 * Authors: Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef H264_NAL_H
# define H264_NAL_H

# ifdef HAVE_CONFIG_H
#  include "config.h"
# endif

# include <vlc_common.h>

# include "../demux/mpeg/mpeg_parser_helpers.h"

#define PROFILE_H264_BASELINE             66
#define PROFILE_H264_MAIN                 77
#define PROFILE_H264_EXTENDED             88
#define PROFILE_H264_HIGH                 100
#define PROFILE_H264_HIGH_10              110
#define PROFILE_H264_HIGH_422             122
#define PROFILE_H264_HIGH_444             144
#define PROFILE_H264_HIGH_444_PREDICTIVE  244

#define PROFILE_H264_CAVLC_INTRA          44
#define PROFILE_H264_SVC_BASELINE         83
#define PROFILE_H264_SVC_HIGH             86
#define PROFILE_H264_MVC_STEREO_HIGH      128
#define PROFILE_H264_MVC_MULTIVIEW_HIGH   118

#define PROFILE_H264_MFC_HIGH                          134
#define PROFILE_H264_MVC_MULTIVIEW_DEPTH_HIGH          138
#define PROFILE_H264_MVC_ENHANCED_MULTIVIEW_DEPTH_HIGH 139

#define H264_SPS_MAX (32)
#define H264_PPS_MAX (256)

enum h264_nal_unit_type_e
{
    H264_NAL_UNKNOWN = 0,
    H264_NAL_SLICE   = 1,
    H264_NAL_SLICE_DPA   = 2,
    H264_NAL_SLICE_DPB   = 3,
    H264_NAL_SLICE_DPC   = 4,
    H264_NAL_SLICE_IDR   = 5,    /* ref_idc != 0 */
    H264_NAL_SEI         = 6,    /* ref_idc == 0 */
    H264_NAL_SPS         = 7,
    H264_NAL_PPS         = 8,
    H264_NAL_AU_DELIMITER= 9
    /* ref_idc == 0 for 6,9,10,11,12 */
};

/* Defined in H.264 annex D */
enum h264_sei_type_e
{
    H264_SEI_PIC_TIMING = 1,
    H264_SEI_USER_DATA_REGISTERED_ITU_T_T35 = 4,
    H264_SEI_RECOVERY_POINT = 6
};

struct h264_nal_sps
{
    int i_id;
    int i_profile, i_profile_compatibility, i_level;
    int i_width, i_height;
    int i_log2_max_frame_num;
    int b_frame_mbs_only;
    int i_pic_order_cnt_type;
    int i_delta_pic_order_always_zero_flag;
    int i_log2_max_pic_order_cnt_lsb;

    struct {
        bool b_valid;
        int i_sar_num, i_sar_den;
        bool b_timing_info_present_flag;
        uint32_t i_num_units_in_tick;
        uint32_t i_time_scale;
        bool b_fixed_frame_rate;
        bool b_pic_struct_present_flag;
        bool b_cpb_dpb_delays_present_flag;
        uint8_t i_cpb_removal_delay_length_minus1;
        uint8_t i_dpb_output_delay_length_minus1;
    } vui;
};

struct h264_nal_pps
{
    int i_id;
    int i_sps_id;
    int i_pic_order_present_flag;
};

/*
    AnnexB : [\x00] \x00 \x00 \x01 Prefixed NAL
    AVC Sample format : NalLengthSize encoded size prefixed NAL
    avcC: AVCDecoderConfigurationRecord combining SPS & PPS in AVC Sample Format
*/

#define H264_MIN_AVCC_SIZE 7

bool h264_isavcC( const uint8_t *, size_t );

/* Convert AVC Sample format to Annex B in-place */
void h264_AVC_to_AnnexB( uint8_t *p_buf, uint32_t i_len,
                         uint8_t i_nal_length_size );

/* Convert Annex B to AVC Sample format in-place
 * Returns the same p_block or a new p_block if there is not enough room to put
 * the NAL size. In case of error, NULL is returned and p_block is released.
 * */
block_t *h264_AnnexB_to_AVC( block_t *p_block, uint8_t i_nal_length_size );

/* Get the SPS/PPS pointers from an Annex B buffer
 * Returns 0 if a SPS and/or a PPS is found */
int h264_get_spspps( uint8_t *p_buf, size_t i_buf,
                     uint8_t **pp_sps, size_t *p_sps_size,
                     uint8_t **pp_pps, size_t *p_pps_size );

/* Parse a SPS into the struct nal_sps
 * Returns 0 in case of success */
int h264_parse_sps( const uint8_t *p_sps_buf, int i_sps_size,
                    struct h264_nal_sps *p_sps );

/* Parse a PPS into the struct nal_pps
 * Returns 0 in case of success */
int h264_parse_pps( const uint8_t *p_pps_buf, int i_pps_size,
                    struct h264_nal_pps *p_pps );

/* Create a AVCDecoderConfigurationRecord from SPS/PPS
 * Returns a valid block_t on success, must be freed with block_Release */
block_t *h264_AnnexB_NAL_to_avcC( uint8_t i_nal_length_size,
                                  const uint8_t *p_sps_buf,
                                  size_t i_sps_size,
                                  const uint8_t *p_pps_buf,
                                  size_t i_pps_size );

/* Convert AVCDecoderConfigurationRecord SPS/PPS to Annex B format */
uint8_t * h264_avcC_to_AnnexB_NAL( const uint8_t *p_buf, size_t i_buf,
                                   size_t *pi_result, uint8_t *pi_nal_length_size );

/* Get level and Profile */
bool h264_get_profile_level(const es_format_t *p_fmt, size_t *p_profile,
                            size_t *p_level, uint8_t *p_nal_length_size);

#endif /* H264_NAL_H */
