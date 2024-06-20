#!/system/bin/sh

#/vendor/bin/provision_devid_demo #The 0x43 key is not burned, so you need to execute this command first.

#echo "sdcard mounting. Waiting..."
#sleep 20  # Pause for 20 seconds and wait for the sdcard to be mounted successfully.

#echo "sdcard mounted. Exiting..."

/vendor/bin/rkp_factory_extraction_tool > /sdcard/csrs.json --output_format build+csr
