#!/bin/bash
# cleanup of DAQ

PROCESSES=" euLog euRun euCliProducer euCliCollector"
for thisprocess in $PROCESSES
do
    echo "Checking if $thisprocess is running... "
    pid=`pgrep -f $thisprocess`
    for i in $pid
    do
     if [ $i ]
     then
        echo "Killing $thisprocess with pid: $pid"
        killall  $thisprocess
        kill -9 $pid
     fi
    done
done


RPCPORT=44000
export RUNCONTROLIP=127.0.0.1
export currentdate=`date +%Y%m%d`
eudaq_bin="./bin/"

# Start Run Control
${eudaq_bin}/euRun &
sleep 2

# Start Logger
${eudaq_bin}/euLog -r tcp://${RUNCONTROLIP} |tee -a ../../../data/logs/logcollector_${currentdate} &
sleep 1

${eudaq_bin}/euCliCollector -n DirectSaveDataCollector -t my_dc -r tcp://${RUNCONTROLIP}:${RPCPORT} |tee -a ../../../data/logs/my_dc_${currentdate}.log&
#sleep 1

# Start SiWECAL Producer
${eudaq_bin}/euCliProducer -n SiWECALProducer -t SiWECALProducer -r tcp://${RUNCONTROLIP}:${RPCPORT} |tee -a ../../../data/logs/siwecal_${currentdate}.log & 
#sleep 1

# Start AHCAL Producer
${eudaq_bin}/euCliProducer -n AHCALProducer -t AHCALProducer -r tcp://${RUNCONTROLIP}:${RPCPORT} |tee -a ../../../data/logs/ahcal_${currentdate}.log  & 
#sleep 1

${eudaq_bin}/euCliProducer -n DesyTableProducer -t DesyTableProducer -r tcp://${RUNCONTROLIP}:${RPCPORT} |tee -a ../../../data/logs/table_${currentdate}.log &


# OnlineMonitor
#xterm -T "Online Monitor" -e 'StdEventMonitor -t StdEventMonitor -r tcp://${RUNCONTROLIP}:${RPCPORT}' &
