#!/bin/sh

# some usefull things (thanks to oz_paulb from mazda3revolution.com)

get_cmu_sw_version()
{
	_ver=$(/bin/grep "^JCI_SW_VER=" /jci/version.ini | /bin/sed 's/^.*_\([^_]*\)\"$/\1/')
	_patch=$(/bin/grep "^JCI_SW_VER_PATCH=" /jci/version.ini | /bin/sed 's/^.*\"\([^\"]*\)\"$/\1/')
	_flavor=$(/bin/grep "^JCI_SW_FLAVOR=" /jci/version.ini | /bin/sed 's/^.*_\([^_]*\)\"$/\1/')

	if [ ! -z "${_flavor}" ]; then
		echo "${_ver}${_patch}-${_flavor}"
	else
		echo "${_ver}${_patch}"
	fi
}

MYDIR=$(dirname $(readlink -f $0))
CMU_SW_VER=$(get_cmu_sw_version)
if [ -f "${MYDIR}/installer_log.txt" ]; then
    #save old logs
    logidx=1
    while [ -f "${MYDIR}/installer_log_${logidx}.txt" ]; do
        logidx=$(($logidx+1))
    done
    mv "${MYDIR}/installer_log.txt" "${MYDIR}/installer_log_${logidx}.txt"
fi

log_message()
{
	printf "$*" >> "${MYDIR}/installer_log.txt"
	/bin/fsync "${MYDIR}/installer_log.txt"
}

#============================= DIALOG FUNCTIONS

show_message() # $1 - title, $2 - message
{
	killall jci-dialog
	log_message "= POPUP INFO: $*\n"
	/jci/tools/jci-dialog --info --title="$1" --text="$2" --no-cancel
}

show_confirmation() # $1 - title, $2 - message
{
    killall jci-dialog
    log_message "= POPUP CONFIRM: $*\n"
    /jci/tools/jci-dialog --confirm --title="$1" --text="$2" --no-cancel
    return $?
}

show_error() # $1 - title, $2 - message
{
    killall jci-dialog
    log_message "= POPUP ERROR: $*\n"
    /jci/tools/jci-dialog --error --title="$1" --text="$2" --ok-label='OK' --no-cancel
}

show_question() # $1 - title, $2 - message, $3 - ok label, $4 - cancel label
{
	killall jci-dialog
	log_message "= POPUP: $*\n"
	/jci/tools/jci-dialog --question --title="$1" --text="$2" --ok-label="$3" --cancel-label="$4"
	return $?
}

#============================= INSTALLATION HELPERS

check_mount_point()
{
    if [ -f "${MYDIR}/installer_log.txt" ]; then
        log_message "OK\n"
    else
        log_message "FAILED!\n"
        show_error "ERROR!" "Mount point not found, have to reboot again."
        sleep 1
        reboot
        exit
    fi
}

disable_watchdog_and_remount()
{
    echo 1 > /sys/class/gpio/Watchdog\ Disable/value && mount -o rw,remount /
    if [ $? -eq 0 ]; then
        log_message "SUCCESS\n"
    else
        log_message "FAILED\n"
        show_error "ERROR!" "Could not disable watchdog or remount filesystem. Rebooting."
        sleep 1
        reboot
        exit
    fi
}

modify_cmu_files()
{
    log_message "Changing opera.ini file ... "
    # -- Enable userjs and allow file XMLHttpRequest in /jci/opera/opera_home/opera.ini - backup first - then edit
    if [ ! -f /jci/opera/opera_home/opera.ini.org ]; then
        log_message "backing up file first ... "
        cp -a /jci/opera/opera_home/opera.ini /jci/opera/opera_home/opera.ini.org
    else
        log_message "backup exists ... "
    fi
    sed -i 's/User JavaScript=0/User JavaScript=1/g' /jci/opera/opera_home/opera.ini
    count=$(grep -c "Allow File XMLHttpRequest=" /jci/opera/opera_home/opera.ini)
    if [ ${count} -eq 0 ]; then
        sed -i '/User JavaScript=.*/a Allow File XMLHttpRequest=1' /jci/opera/opera_home/opera.ini
    else
        sed -i 's/Allow File XMLHttpRequest=.*/Allow File XMLHttpRequest=1/g' /jci/opera/opera_home/opera.ini
    fi
    log_message "DONE\n"

    #Rename this since once we turn on userJs we don't want a FPS indicator everywhere
    if [ ! -f /jci/opera/opera_dir/userjs/fps.js.bak ]; then
        log_message "Disabling fps.js ... "
        if mv /jci/opera/opera_dir/userjs/fps.js /jci/opera/opera_dir/userjs/fps.js.bak; then
            log_message "DONE\n"
        else
            log_message "FAILED\n"
        fi
    fi
    log_message "Changing stage_wifi.sh file ... "
    if grep -Fq "# Android Auto start" /jci/scripts/stage_wifi.sh; then
        log_message "autorun AA entry already exists in /jci/scripts/stage_wifi.sh"
        if grep -q "websocketd" /jci/scripts/stage_wifi.sh ; then
            log_message " ... found old websocketd version and replacing to new one ... "
            sed -i 's/.*websocketd.*/headunit-wrapper \&/' /jci/scripts/stage_wifi.sh
            log_message "DONE"
        fi
        log_message "\n"
    else
        #first backup
        if [ ! -f /jci/scripts/stage_wifi.sh.bak ]; then
            log_message "backing up file first ... "
            if cp -a /jci/scripts/stage_wifi.sh /jci/scripts/stage_wifi.sh.bak; then
                echo "# Android Auto start" >> /jci/scripts/stage_wifi.sh
                echo "headunit-wrapper &" >> /jci/scripts/stage_wifi.sh
                echo "check-usb.sh &" >> /jci/scripts/stage_wifi.sh
                log_message "autostart entry added to /jci/scripts/stage_wifi.sh ... DONE\n"
            else
                log_message "backup failed so leaving file as is - there will be no autostart. FAILED\n"
            fi
        else
            log_message "backup exists ... "
            echo "# Android Auto start" >> /jci/scripts/stage_wifi.sh
            echo "headunit-wrapper &" >> /jci/scripts/stage_wifi.sh
            log_message "autostart entry added to /jci/scripts/stage_wifi.sh ... DONE\n"
        fi
    fi
    log_message "Copying BdsConfiguration.xml"
    if grep -Fq "AndroidAuto" /jci/bds/BdsConfiguration.xml; then
      log_message "Android Auto entry already in BdsConfiguration.xml"
    else
      if cp -a /jci/bds/BdsConfiguration.xml /jci/bds/BdsConfiguration.xml.bak; then
        cp "${MYDIR}/config/jci/bds/BdsConfiguration.xml" /jci/bds/BdsConfiguration.xml
      else
        log_message "Backing up BdsConfiguration.xml failed. No changes made"
      fi
    fi
}

revert_cmu_files()
{
    log_message "Reverting opera.ini file ... "
    reverted=0
    # -- Revert /jci/opera/opera_home/opera.ini from backup
    if [ -f /jci/opera/opera_home/opera.ini.org ]; then
        log_message "from backup ... "
        if mv -a /jci/opera/opera_home/opera.ini.org /jci/opera/opera_home/opera.ini; then
            log_message "DONE\n"
            reverted=1
        else
            log_message "FAILED ... trying the same "
        fi
    fi
    if [ ${reverted} -eq 0 ]; then
        log_message "by reverting changes ... "
        sed -i 's/User JavaScript=1/User JavaScript=0/g' /jci/opera/opera_home/opera.ini &&
            sed -i 'Allow File XMLHttpRequest=1/d' /jci/opera/opera_home/opera.ini
        if [ $? == 0 ]; then
            log_message "DONE\n"
        else
            log_message "FAILED\n"
        fi
    fi
    #Revert fps since once we try to match original state of JCI
    if [ -f /jci/opera/opera_dir/userjs/fps.js.bak ]; then
        log_message "Reverting fps.js from backup ... "
        if mv /jci/opera/opera_dir/userjs/fps.js.bak /jci/opera/opera_dir/userjs/fps.js; then
            log_message "DONE\n"
        else
            log_message "FAILED\n"
        fi
    fi
    if grep -Fq "# Android Auto start" /jci/scripts/stage_wifi.sh; then
        log_message "Reverting stage_wifi.sh ... "
        reverted=0
        if [ -f "/jci/scripts/stage_wifi.sh.bak" ]; then
            log_message " from backup ... "
            if mv /jci/scripts/stage_wifi.sh.bak /jci/scripts/stage_wifi.sh; then
                log_message "DONE\n"
                reverted=1
            else
                log_message "FAILED ... trying the same "
            fi
        fi
        if [ ${reverted} -eq 0 ]; then
            log_message "by reverting changes ... "
            sed -i '/# Android Auto start/d' /jci/scripts/stage_wifi.sh &&
            sed -i '/headunit-wrapper/d' /jci/scripts/stage_wifi.sh &&
            sed -i '/check-usb.sh/d' /jci/scripts/stage_wifi.sh
            if [ $? == 0 ]; then
                log_message "DONE\n"
            else
                log_message "FAILED\n"
            fi
        fi
    fi
    if grep -Fq "AndroidAuto" /jci/bds/BdsConfiguration.xml; then
      log_message "Reverting BdsConfiguration.xml"
      if cp -a /jci/bds/BdsConfiguration.xml.bak /jci/bds/BdsConfiguration.xml; then
        log_message "BdsConfiguration.xml reverted"
      else
        log_message "Failed to revert BdsConfiguration.xml"
      fi
    fi
}

copy_aa_binaries()
{
    # Install or update Android Auto Headunit App - we can copy files for sure.
    log_message "Copying AA files ... "
    cp -a ${MYDIR}/config/androidauto/data_persist/dev/* /tmp/mnt/data_persist/dev/ || "headunit binaries failed ... "
    cp -a ${MYDIR}/config/androidauto/jci/gui/apps/_androidauto /jci/gui/apps/ || "_androidauto failed ... "
    cp -a ${MYDIR}/config/androidauto/jci/opera/opera_dir/userjs/additionalApps.* /jci/opera/opera_dir/userjs/ || "additionalApps.* failed ... "
    log_message "DONE\n"

    log_message "Changing permissions ... "
    chmod 755 /tmp/mnt/data_persist/dev/bin/headunit || "for headunit failed ... "
    chmod 755 /tmp/mnt/data_persist/dev/bin/headunit-wrapper || "for headunit-wrapper failed ... "
    chmod 755 /tmp/mnt/data_persist/dev/bin/headunit.json || "for headunit.json failed ... "
    chmod 755 /tmp/mnt/data_persist/dev/bin/check-usb.sh  || "for check-usb.sh  failed ... "
    log_message "DONE\n"
}

remove_aa_binaries()
{
    # Remove Android Auto Headunit App
    log_message "Removing AA files ... "
    rm /tmp/mnt/data_persist/dev/bin/headunit || log_message "headunit failed ... "
    rm /tmp/mnt/data_persist/dev/bin/headunit-wrapper || log_message "headunit-wrapper failed ... "
    rm /tmp/mnt/data_persist/dev/bin/headunit.json || log_message "headunit.json failed ... "
    rm /tmp/mnt/data_persist/dev/bin/check-usb.sh  || log_message "check-usb.sh failed ... "
    rm -rf /tmp/mnt/data_persist/dev/bin/headunit_libs || log_message "headunit_libs failed ... "
    rm -rf /jci/gui/apps/_androidauto || log_message "_androidauto failed ... "
    rm /jci/opera/opera_dir/userjs/additionalApps.* || "additionalApps.* failed ... "
    log_message "DONE\n"
}

test_run()
{
    #make sure it can run. this will hopefully generate a more useful log for debugging that is easy to get if not
    log_message "\n\nRunning smoke test\n\n"
    /tmp/mnt/data_persist/dev/bin/headunit-wrapper test
    cat /tmp/mnt/data/headunit.log >> "${MYDIR}/installer_log.txt"
    if grep -Fq "###TESTMODE_OK###" /tmp/mnt/data/headunit.log; then
        log_message "\nTest looks good.\n"
    else
        log_message "\nTest went wrong.\n"
        show_message "TEST RUN FAILED" "Headunit binary launch failed. Check the log."
    fi
}

#============================= INSTALLATION STARTS HERE

log_message "Installer started.\n"

log_message "MYDIR = ${MYDIR}\n"
log_message "CMU version = ${CMU_SW_VER}\n"

# check software version first
echo ${CMU_SW_VER} | /bin/sed "/^5[569]\..*/Q 1"
if [ $? -ne 1 ]; then
    log_message "Script aborted due to CMU version mismatch."
    show_message "Aborted" "This version of CMU is not supported. Please update first."
    exit
fi

# first test, if copy from MZD to sd card is working to test correct mount point
log_message "Check mount point ... "
check_mount_point

# ask if proceed with installation
show_question "AA INSTALL SCRIPT" "Welcome to Android Auto installation script. Would you like to proceed?" "Proceed" "Abort"
if [ $? -ne 0 ]; then
    log_message "Installation aborted.\n"
    show_message "Aborted" "Script aborted. Please remove the USB drive. There is no need to reboot."
    exit
fi

# disable watchdog and allow write access
log_message "Disabling watchdog and remounting for write access ... "
disable_watchdog_and_remount

installed=0
remove=0
# check whether we have AA already installed
# check for headunit-wrapper to make sure the installed version is new one
log_message "Check whether Android Auto is installed ... "
if [ -f /tmp/mnt/data_persist/dev/bin/headunit-wrapper ]; then
    installed=1
    log_message "YES\n"
    show_question "CHOOSE ACTION" "You have Android Auto already installed. Would you like to update or remove it?" "UPDATE" "REMOVE"
    remove=$?
    if [ ${remove} -eq 1 ]; then
        log_message "Removing Android Auto.\n"
    else
        log_message "Updating Android Auto.\n"
    fi
    log_message "Killing running headunit processes ... "
    killall -q -9 headunit && log_message "KILLED\n" || log_message "FAILED! No 'headunit' process found or could not kill it.\n"
else
    log_message "NO\n"
    log_message "Installing Android Auto.\n"
fi

if [ ${installed} -eq 0 ]; then    # this is an installation path - installed=false
    show_message "INSTALLING" "Android Auto is installing ..."

    modify_cmu_files
    copy_aa_binaries
    test_run

    log_message "Installation complete!\n"
    show_confirmation "DONE" "Android Auto has been installed. System will reboot now. Remember to remove USB drive."
elif [ ${remove} -eq 1 ]; then
    # this is a removing path - installed=true, remove=true
    show_message "UNINSTALLING" "Android Auto is uninstalling ..."

    revert_cmu_files
    remove_aa_binaries

    log_message "Uninstall complete!\n"
    show_confirmation "DONE" "Uninstall complete. System will reboot now. Remember to remove USB drive."
else
    # this is an update path - installed=true, remove=false
    show_message "UPDATING" "Android Auto is updating ..."

    # still modify cmu_files in case they were updated
    modify_cmu_files
    # remove all files and copy once again in case there were some orphaned files in CMU
    remove_aa_binaries
    copy_aa_binaries
    test_run

    log_message "Update complete!\n"
    show_confirmation "DONE" "Update complete. System will reboot now. Remember to remove USB drive."
fi

sleep 3
reboot
exit
