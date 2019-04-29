#!/bin/bash

workdir=/opt/torrent
log=torrent.log
conf=config.conf

function startTorrent() {

    status
    if [ $res -eq 0 ]
    then
        echo Torrent already running, pid $pid
    else
        ulimit -c unlimited
        if [ -d $workdir ]
        then
           cd $workdir

           if [ ! -f $key ]
           then
               echo "No key file $key found, exiting."
               exit 2
           elif [ ! -f $conf ]
           then
               echo "No config file $conf found, exiting."
                          exit 2
           fi
           echo Staring torrent
           ./torrent_node $conf > $log &
        else
           echo "workdir $workdir doesn't exists"
           exit 2;
        fi
    fi


}


function stopTorrent() {


    status
    if [ $res -ne 0 ]
       then
               echo Torrent not running
    else

        echo Stopping torrent, pid $pid
        kill $pid
        sleep 2
        status

        if [ $res -eq 0 ]
        then
            echo Stop failed. Torrent still alive, pid $pid. Please check manually
        fi

    fi
}

function status() {

    pid=`pidof torrent_node`
    res=$?
}


case "$1" in
start)
    startTorrent
    ;;

stop)
    stopTorrent
    ;;

restart)
    stopTorrent
    startTorrent
    ;;

status)
     status

    if [ $res -ne 0 ]
       then
               echo -e "Torrent \e[31mnot running\e[0m"
       else
               echo -e "Torrent is \e[32mrunning\e[0m. Pid $pid"
       fi
     ;;

 *)
     echo "Usage: $0 start|stop|status|restart"
     exit 1
    ;;
esac
