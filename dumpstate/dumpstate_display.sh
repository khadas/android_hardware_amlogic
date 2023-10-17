echo "################################Start dumpstate display#################################"

function echo_command () {
    if [ -e $2 ]; then
        echo $1 > $2
        echo ""
    fi
}

function dump_node () {
    if [ -e $1 ]; then
        echo "dump" $1
        cat $1
        echo ""
    fi
}

echo "################################Dump DV/HDR information ###############################"
dump_node /sys/class/amhdmitx/amhdmitx0/dv_cap
dump_node /sys/class/amhdmitx/amhdmitx0/hdr_cap
dump_node /sys/class/amhdmitx/amhdmitx0/attr
dump_node /sys/class/amdolby_vision/dv_mode
dump_node /sys/class/amvecm/hdr_dbg
dump_node /sys/module/aml_media/parameters/hdr_policy
dump_node /sys/module/aml_media/parameters/force_output
dump_node /sys/class/amhdmitx/amhdmitx0/hdmi_hdr_status
dump_node /sys/module/aml_media/parameters/dolby_vision_policy
dump_node /sys/module/aml_media/parameters/dolby_vision_ll_policy
dump_node /sys/class/amdolby_vision/src_format
dump_node /sys/module/aml_media/parameters/dolby_vision_enable

echo "################################Dump DI information ###################################"
#mount -t debugfs none /sys/kernel/debug/
dump_node /sys/kernel/debug/di1/state
dump_node /sys/kernel/debug/di2/state
dump_node /sys/kernel/debug/di3/state
dump_node /sys/kernel/debug/di1/list_ndis
dump_node /sys/kernel/debug/di2/list_ndis
dump_node /sys/kernel/debug/di3/list_ndis
dump_node /sys/kernel/debug/di_top/policy
dump_node /sys/kernel/debug/di1/rvfm_out
dump_node /sys/kernel/debug/di2/rvfm_out
dump_node /sys/kernel/debug/di3/rvfm_out
dump_node /sys/kernel/debug/di1/rvfm_in
dump_node /sys/kernel/debug/di2/rvfm_in
dump_node /sys/kernel/debug/di3/rvfm_in

echo "################################Dump VPP information ##################################"
dump_node /sys/class/vfm/map
dump_node /sys/class/video/frame_width
dump_node /sys/class/video/frame_height
dump_node /sys/class/video/axis
dump_node /sys/class/video/crop
dump_node /sys/class/video/video_state

echo "################################Dump vdin information #################################"
dump_node /sys/class/hdmirx/hdmirx0/info
echo_command "state" "/sys/class/hdmirx/hdmirx0/debug"
echo_command "state2" "/sys/class/hdmirx/hdmirx0/debug"
echo_command "state" "/sys/class/vdin/vdin0/attr"
echo_command "fps" "/sys/class/vdin/vdin0/attr"
echo_command "counter" "/sys/class/vdin/vdin0/attr"
echo_command "histgram" "/sys/class/vdin/vdin0/attr"
echo_command "state" "/sys/class/vdin/vdin0/sct_attr"
echo_command "state" "/sys/class/vdin/vdin1/attr"
dump_node /sys/class/vdin/vdin0/vf_log
dump_node /sys/class/vdin/vdin1/vf_log
echo_command "echo start" "/sys/class/vdin/vdin0/vf_log"
echo_command "echo start" "/sys/class/vdin/vdin1/vf_log"
sleep 1;
echo_command "echo print" "/sys/class/vdin/vdin0/vf_log"
echo_command "echo print" "/sys/class/vdin/vdin1/vf_log"

echo "################################Dump FRC information ##################################"
echo_command "status" "/sys/class/frc/debug"

echo "################################Dump pipeline info   ##################################"
dump_node /sys/class/video/video_global_output
dump_node /sys/class/video/disable_video
dump_node /sys/module/aml_media/parameters/video_mute_on
dump_node /sys/module/aml_media/parameters/new_frame_count
dump_node /sys/class/video/hold_video
dump_node /sys/class/video/path_select
dump_node /sys/class/video/video_state
echo_command "dump all" "/sys/class/vfm/map"
echo_command "rv 1dfb" "/sys/class/amvecm/reg"
echo_command "rv 1de1" "/sys/class/amvecm/reg"
echo_command "rv 1de2" "/sys/class/amvecm/reg"
dump_node /sys/class/v4lvideo/dec_count
dump_node /sys/class/v4lvideo/dq_count
dump_node /sys/class/v4lvideo/get_count
dump_node /sys/class/v4lvideo/link_fd_count
dump_node /sys/class/v4lvideo/link_put_fd_count
dump_node /sys/class/v4lvideo/open_fd_count
dump_node /sys/class/v4lvideo/put_count
dump_node /sys/class/v4lvideo/q_count
dump_node /sys/class/v4lvideo/release_fd_count
dump_node /sys/class/v4lvideo/total_get_count
dump_node /sys/class/v4lvideo/total_put_count
dump_node /sys/class/v4lvideo/total_release_count
dump_node /sys/class/vdec/dump_decoder_state
echo "################################End dumpstate display#################################"
