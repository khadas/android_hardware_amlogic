echo "################Start dumpstate display################"
echo "Video Frame information"
cat /sys/class/video/vframe_states
cat /sys/class/ppmgr/ppmgr_vframe_states
cat /sys/class/ionvideo/vframe_states
cat /sys/class/vfm/map

echo "osd registers informations"
echo dump > /sys/devices/platform/fb/graphics/fb0/debug
cat /sys/devices/platform/fb/graphics/fb0/window_axis
cat /sys/devices/platform/fb/graphics/fb0/scale_width
cat /sys/devices/platform/fb/graphics/fb0/scale_height

echo "dump video information"
echo 1 > /sys/module/amvideo/parameters/debug_flag
cat /sys/class/video/video_state
cat /sys/class/video/frame_width
cat /sys/class/video/frame_height
cat /sys/class/video/axis
cat /sys/class/video/crop
cat /sys/class/video/screen_mode
cat /sys/class/video/frame_rate

echo "dump amvideo information"
cat /sys/module/amvideo/parameters/process_3d_type
cat /sys/module/amvideo/parameters/sharpness1_sr2_ctrl_32d7
cat /sys/module/amvideo/parameters/sharpness1_sr2_ctrl_3280

echo "dump DI information"
echo dumpreg > /sys/class/deinterlace/di0/debug
echo state > /sys/class/deinterlace/di0/debug
cat /sys/module/di/parameters/di_vscale_skip_count_real
cat /sys/class/deinterlace/di0/provider_vframe_status
cat /sys/class/video/video_state
cat /sys/class/video/vframe_states
cat /sys/class/amvecm/dump_reg

echo "dump displaymode information"
cat /sys/class/display/mode
cat /sys/class/amhdmitx/amhdmitx0/disp_mode
cat /sys/class/amhdmitx/amhdmitx0/attr
cat /sys/class/amhdmitx/amhdmitx0/rawedid
cat /sys/class/amhdmitx/amhdmitx0/disp_cap
cat /sys/class/amhdmitx/amhdmitx0/dc_cap
cat /sys/class/amhdmitx/amhdmitx0/aud_cap
cat /sys/class/amhdmitx/amhdmitx0/hdr_cap
cat /sys/class/amhdmitx/amhdmitx0/hdr_cap2
cat /sys/class/amhdmitx/amhdmitx0/dv_cap
cat /sys/class/amhdmitx/amhdmitx0/dv_cap2
cat /sys/class/amhdmitx/amhdmitx0/allm_cap
cat /sys/class/amhdmitx/amhdmitx0/contenttype_cap
cat /sys/class/amhdmitx/amhdmitx0/hpd_state
cat /sys/class/amhdmitx/amhdmitx0/hdcp_lstore
cat /sys/class/amhdmitx/amhdmitx0/hdcp_mode
cat /sys/class/amhdmitx/amhdmitx0/cedst_policy

echo "dump hdmi registers"
cat /sys/kernel/debug/amhdmitx/aud_cts
cat /sys/kernel/debug/amhdmitx/bus_reg
cat /sys/kernel/debug/amhdmitx/hdmi_pkt
cat /sys/kernel/debug/amhdmitx/hdmi_reg
cat /sys/kernel/debug/amhdmitx/hdmi_timing
echo state > /sys/class/hdmirx/hdmirx0/debug
cat /sys/class/hdmirx/hdmirx0/info

echo "dump video panel"
echo dump > /sys/class/lcd/debug

echo "dump backlight information"
cat /sys/class/aml_bl/status
cat /sys/class/aml_bl/pwm

echo "dump vout information"
cat /sys/class/display/vinfo

echo "dump vout information"
echo state >/sys/devices/platform/vdin0/vdin/vdin0/attr
echo state >/sys/devices/platform/vdin1/vdin/vdin1/attr
echo start > /sys/devices/platform/vdin0/vdin/vdin0/vf_log
sleep 2;
echo print > /sys/devices/platform/vdin0/vdin/vdin0/vf_log
echo dump_reg > /sys/devices/platform/vdin0/vdin/vdin0/attr
echo dump_reg > /sys/devices/platform/vdin1/vdin/vdin1/attr

echo "dump tvafe information"
echo D > /sys/class/tvafe/tvafe0/reg

echo "dump amvecm information"
cat /sys/class/amvecm/hdr_dbg
cat /sys/class/amvecm/hdr_reg
cat /sys/module/am_vecm/parameters/cur_csc_type
echo "################End dumpstate display################"
