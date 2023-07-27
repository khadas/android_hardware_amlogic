set -x
echo "################Start dumpstate display################"
echo "dump DV information"
cat /sys/class/amhdmitx/amhdmitx0/dv_cap
cat /sys/class/amhdmitx/amhdmitx0/hdr_cap
echo "cat /sys/class/amhdmitx/amhdmitx0/attr"
cat /sys/class/amhdmitx/amhdmitx0/attr
echo "cat /sys/class/amdolby_vision/dv_mode"
cat /sys/class/amdolby_vision/dv_mode
echo "cat /sys/class/amvecm/hdr_dbg"
cat /sys/class/amvecm/hdr_dbg
echo "cat /sys/module/aml_media/parameters/hdr_policy"
cat /sys/module/aml_media/parameters/hdr_policy
echo "cat /sys/module/aml_media/parameters/force_output"
cat /sys/module/aml_media/parameters/force_output
echo "cat /sys/class/amhdmitx/amhdmitx0/hdmi_hdr_status"
cat /sys/class/amhdmitx/amhdmitx0/hdmi_hdr_status
echo "cat /sys/module/aml_media/parameters/dolby_vision_policy"
cat /sys/module/aml_media/parameters/dolby_vision_policy
echo "cat /sys/module/aml_media/parameters/dolby_vision_ll_policy"
cat /sys/module/aml_media/parameters/dolby_vision_ll_policy
echo "cat /sys/class/amdolby_vision/src_format"
cat /sys/class/amdolby_vision/src_format
echo "cat /sys/module/aml_media/parameters/dolby_vision_enable"
cat /sys/module/aml_media/parameters/dolby_vision_enable

echo "dump DI information"
#mount -t debugfs none /sys/kernel/debug/
echo "/cat sys/kernel/debug/di*/state"
cat /sys/kernel/debug/di1/state
cat /sys/kernel/debug/di2/state
cat /sys/kernel/debug/di3/state
echo "cat /sys/kernel/debug/di*/list_ndis"
cat /sys/kernel/debug/di1/list_ndis
cat /sys/kernel/debug/di2/list_ndis
cat /sys/kernel/debug/di3/list_ndis
echo "cat di_top policy"
cat /sys/kernel/debug/di_top/policy
echo "cat /sys/kernel/debug/di*/rvfm_out"
cat /sys/kernel/debug/di1/rvfm_out
cat /sys/kernel/debug/di2/rvfm_out
cat /sys/kernel/debug/di3/rvfm_out
echo "/sys/kernel/debug/di*/rvfm_in"
cat /sys/kernel/debug/di1/rvfm_in
cat /sys/kernel/debug/di2/rvfm_in
cat /sys/kernel/debug/di3/rvfm_in

echo "dump VPP information"
echo "cat /sys/class/vfm/map"
cat /sys/class/vfm/map
echo "/sys/class/video/frame_width"
cat /sys/class/video/frame_width
echo "/sys/class/video/frame_height"
cat /sys/class/video/frame_height
echo "cat /sys/class/video/axis"
cat /sys/class/video/axis
echo "cat /sys/class/video/crop"
cat /sys/class/video/crop
echo "cat /sys/class/video/video_state"
cat /sys/class/video/video_state

echo "dump vdin information"
echo "cat /sys/class/hdmirx/hdmirx0/info"
cat /sys/class/hdmirx/hdmirx0/info
echo "echo state > /sys/class/hdmirx/hdmirx0/debug"
echo state > /sys/class/hdmirx/hdmirx0/debug
echo "echo state2 > /sys/class/hdmirx/hdmirx0/debug"
echo state2 > /sys/class/hdmirx/hdmirx0/debug
echo "echo state > /sys/class/vdin/vdin0/attr"
echo state > /sys/class/vdin/vdin0/attr
echo "echo fps > /sys/class/vdin/vdin0/attr"
echo fps > /sys/class/vdin/vdin0/attr
echo "echo counter > /sys/class/vdin/vdin0/attr"
echo counter > /sys/class/vdin/vdin0/attr
echo "echo histgram > /sys/class/vdin/vdin0/attr"
echo histgram > /sys/class/vdin/vdin0/attr
echo "echo state > /sys/class/vdin/vdin0/sct_attr"
echo state > /sys/class/vdin/vdin0/sct_attr
echo "echo state > /sys/class/vdin/vdin1/attr"
echo state > /sys/class/vdin/vdin1/attr
echo "cat /sys/class/vdin/vdin0/vf_log"
cat /sys/class/vdin/vdin0/vf_log
echo "cat /sys/class/vdin/vdin1/vf_log"
cat /sys/class/vdin/vdin1/vf_log
echo "echo start > /sys/class/vdin/vdin0/vf_log"
echo start > /sys/class/vdin/vdin0/vf_log
echo "echo start > /sys/class/vdin/vdin1/vf_log"
echo start > /sys/class/vdin/vdin1/vf_log
sleep 1;
echo "echo print > /sys/class/vdin/vdin0/vf_log"
echo print > /sys/class/vdin/vdin0/vf_log
echo "echo print > /sys/class/vdin/vdin1/vf_log"
echo print > /sys/class/vdin/vdin1/vf_log

echo "dump FRC information"
echo status > /sys/class/frc/debug

echo "dump video_pipeline information"
echo "cat /sys/class/video/video_global_output"
cat /sys/class/video/video_global_output
echo "cat /sys/class/video/disable_video"
cat /sys/class/video/disable_video
echo "cat /sys/module/aml_media/parameters/video_mute_on"
cat /sys/module/aml_media/parameters/video_mute_on
echo "cat /sys/module/aml_media/parameters/new_frame_count"
cat /sys/module/aml_media/parameters/new_frame_count
echo "cat /sys/class/video/hold_video"
cat /sys/class/video/hold_video
echo "cat /sys/class/video/path_select"
cat /sys/class/video/path_select
echo "cat /sys/class/video/video_state"
cat /sys/class/video/video_state
echo "echo dump all > /sys/class/vfm/map"
echo dump all > /sys/class/vfm/map
echo "echo rv 1dfb > /sys/class/amvecm/reg"
echo rv 1dfb > /sys/class/amvecm/reg
echo "echo rv 1de1 > /sys/class/amvecm/reg"
echo rv 1de1 > /sys/class/amvecm/reg
echo "echo rv 1de2 > /sys/class/amvecm/reg"
echo rv 1de2 > /sys/class/amvecm/reg
cat /sys/class/v4lvideo/dec_count
cat /sys/class/v4lvideo/dq_count
cat /sys/class/v4lvideo/get_count
cat /sys/class/v4lvideo/link_fd_count
cat /sys/class/v4lvideo/link_put_fd_count
cat /sys/class/v4lvideo/open_fd_count
cat /sys/class/v4lvideo/put_count
cat /sys/class/v4lvideo/q_count
cat /sys/class/v4lvideo/release_fd_count
cat /sys/class/v4lvideo/total_get_count
cat /sys/class/v4lvideo/total_put_count
cat /sys/class/v4lvideo/total_release_count
echo "cat /sys/class/vdec/dump_decoder_state"
cat /sys/class/vdec/dump_decoder_state
echo "################End dumpstate display################"
