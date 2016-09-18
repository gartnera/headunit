#!/bin/sh

# some usefull things (thanks to oz_paulb from mazda3revolution.com - your code is awesome! I wish, I could understand everything ...)

get_cmu_sw_version()
{
	_ver=$(/bin/grep "^JCI_SW_VER=" /jci/version.ini | /bin/sed 's/^.*_\([^_]*\)\"$/\1/')
	_patch=$(/bin/grep "^JCI_SW_VER_PATCH=" /jci/version.ini | /bin/sed 's/^.*\"\([^\"]*\)\"$/\1/')
	_flavor=$(/bin/grep "^JCI_SW_FLAVOR=" /jci/version.ini | /bin/sed 's/^.*_\([^_]*\)\"$/\1/')

	if [[ ! -z "${_flavor}" ]]; then
		echo "${_ver}${_patch}-${_flavor}"
	else
		echo "${_ver}${_patch}"
	fi
}

get_cmu_sw_version_only()
{
	_veronly=$(/bin/grep "^JCI_SW_VER=" /jci/version.ini | /bin/sed 's/^.*_\([^_]*\)\"$/\1/')
	echo "${_veronly}"
}

log_message()
{
	echo "$*" 1>2
	echo "$*" >> "${MYDIR}/AIO_log.txt"
	/bin/fsync "${MYDIR}/AIO_log.txt"
}


show_message()
{
	sleep 4
	killall jci-dialog
	log_message "= POPUP: $* "
	/jci/tools/jci-dialog --info --title="MESSAGE" --text="$*" --no-cancel &
}


show_message_OK()
{
	sleep 4
	killall jci-dialog
	log_message "= POPUP: $* "
	/jci/tools/jci-dialog --confirm --title="CONTINUE INSTALLATION?" --text="$*" --ok-label="YES - GO ON" --cancel-label="NO - ABORT"
	if [ $? != 1 ]
		then
			killall jci-dialog
			break
		else
			show_message "INSTALLATION ABORTED! PLEASE UNPLUG USB DRIVE"
			sleep 5
			exit
		fi
}


# disable watchdog and allow write access
echo 1 > /sys/class/gpio/Watchdog\ Disable/value
mount -o rw,remount /


MYDIR=$(dirname $(readlink -f $0))
CMU_SW_VER=$(get_cmu_sw_version)
CMU_VER_ONLY=$(get_cmu_sw_version_only)
rm -f "${MYDIR}/AIO_log.txt"


log_message "=== START LOGGING ... ==="
# log_message "=== CMU_SW_VER = ${CMU_SW_VER} ==="
log_message "=== MYDIR = ${MYDIR} ==="
log_message "=== Watchdog temporary disabeld and write access enabled ==="


# first test, if copy from MZD to sd card is working to test correct mount point
cp /jci/sm/sm.conf ${MYDIR}/config
if [ -e ${MYDIR}/config/sm.conf ]
	then
		log_message "=== Copytest to sd card successful, mount point is OK ==="
		rm -f ${MYDIR}/config/sm.conf
	else
		log_message "=== Copytest to sd card not successful, mount point not found! ==="
		/jci/tools/jci-dialog --title="ERROR!" --text="Mount point not found, have to reboot again" --ok-label='OK' --no-cancel &
		sleep 5
		reboot
		exit
fi


show_message_OK "Version = ${CMU_SW_VER} : To continue installation press OK"


# a window will appear for 4 seconds to show the beginning of installation
show_message "START OF TWEAK INSTALLATION ..."


# disable watchdogs in /jci/sm/sm.conf to avoid boot loops if somthing goes wrong
if [ ! -e /jci/sm/sm.conf.org ]
	then
		cp -a /jci/sm/sm.conf /jci/sm/sm.conf.org
		log_message "=== Backup of /jci/sm/sm.conf to sm.conf.org ==="
	else log_message "=== Backup of /jci/sm.conf.org already there! ==="
fi
sed -i 's/watchdog_enable="true"/watchdog_enable="false"/g' /jci/sm/sm.conf
sed -i 's|args="-u /jci/gui/index.html"|args="-u /jci/gui/index.html --noWatchdogs"|g' /jci/sm/sm.conf
log_message "=== WATCHDOG IN SM.CONF PERMANENTLY DISABLED ==="


# -- Enable userjs and allow file XMLHttpRequest in /jci/opera/opera_home/opera.ini - backup first - then edit
if [ ! -e /jci/opera/opera_home/opera.ini.org ]
	then
		cp -a /jci/opera/opera_home/opera.ini /jci/opera/opera_home/opera.ini.org
		log_message "=== Backup of /jci/opera/opera_home/opera.ini to opera.ini.org ==="
	else log_message "=== Backup of /jci/opera/opera_home/opera.ini.org already there! ==="
fi
sed -i 's/User JavaScript=0/User JavaScript=1/g' /jci/opera/opera_home/opera.ini
count=$(grep -c "Allow File XMLHttpRequest=" /jci/opera/opera_home/opera.ini)
if [ "$count" = "0" ]
	then
		sed -i '/User JavaScript=.*/a Allow File XMLHttpRequest=1' /jci/opera/opera_home/opera.ini
	else
		sed -i 's/Allow File XMLHttpRequest=.*/Allow File XMLHttpRequest=1/g' /jci/opera/opera_home/opera.ini
fi
log_message "=== ENABLED USERJS AND ALLOWED FILE XMLHTTPREQUEST IN /JCI/OPERA/OPERA_HOME/OPERA.INI  ==="


# Install Android Auto Headunit App
show_message "INSTALL ANDROID AUTO HEADUNIT APP ..."
cp -a ${MYDIR}/config/androidauto/data_persist/dev/* /tmp/mnt/data_persist/dev/
cp -a ${MYDIR}/config/androidauto/jci/gui/apps/_androidauto /jci/gui/apps/
cp -a ${MYDIR}/config/androidauto/jci/opera/opera_dir/userjs/additionalApps.* /jci/opera/opera_dir/userjs/
mv /jci/opera/opera_dir/userjs/fps.js /jci/opera/opera_dir/userjs/fps.js.bak
cp -a ${MYDIR}/config/androidauto/usr/lib/gstreamer-0.10/libgsth264parse.so /usr/lib/gstreamer-0.10
cp -a ${MYDIR}/config/androidauto/usr/lib/gstreamer-0.10/libgstalsa.so /usr/lib/gstreamer-0.10

cp -a ${MYDIR}/config/androidauto/jci/sm/* /jci/sm/
log_message "=== Copied Android Auto Headunit App files ==="
chmod 755 /tmp/mnt/data_persist/dev/bin/websocketd
chmod 755 /tmp/mnt/data_persist/dev/bin/headunit
chmod 755 /tmp/mnt/data_persist/dev/bin/input_filter

#once you are sure you are not in a boot loop, make this 2, putting it in data_persist doesn't work since it's not writable normally
show_message "Setting /tmp/mnt/data/enable_input_filter to 1. If everything works reinstall with 2"
echo "1" > /tmp/mnt/data/enable_input_filter
#echo "2" > /tmp/mnt/data/enable_input_filter
chmod 755 /tmp/mnt/data/enable_input_filter

#add androidauto.js to stage_wifi
if [ -e "/jci/scripts/stage_wifi.sh" ]
	then
		if grep -Fq "# Android Auto start" /jci/scripts/stage_wifi.sh
			then
				echo "exist"
				log_message "=== Modifications already done to /jci/scripts/stage_wifi.sh ==="
			else
				#first backup
				cp -a /jci/scripts/stage_wifi.sh /jci/scripts/stage_wifi.sh.org3
				log_message "=== Backup of /jci/scripts/stage_wifi.sh to stage_wifi.sh.org3==="
				echo "# Android Auto start" >> /jci/scripts/stage_wifi.sh
				echo "websocketd --port=9999 sh &" >> /jci/scripts/stage_wifi.sh
				log_message "=== Modifications added to /jci/scripts/stage_wifi.sh ==="
			break
		fi
	fi
log_message "=== END INSTALLATION OF ANDROID AUTO HEADUNIT APP ==="


# a window will appear for asking to reboot automatically
sleep 4
killall jci-dialog
sleep 3
/jci/tools/jci-dialog --confirm --title="SELECTED ALL-IN-ONE TWEAKS APPLIED" --text="Click OK to reboot the system"
		if [ $? != 1 ]
		then
			reboot
			exit
		fi
sleep 10
killall jci-dialog
