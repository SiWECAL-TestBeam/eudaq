#!/bin/bash

if [ -f KILLRUN.local ]
then
    sh KILLRUN.local
else
    sh KILLRUN.sh
fi

export currentdate=`date +%Y%m%d`
export RCPORT=44000
export HOSTIP=192.168.1.10
#################  Run control ###################
#xterm -sb -sl 1000000 -T "Runcontrol" -e 'INSTALL/bin/euRun -n RunControl ; read '&
xterm -r -sb -sl 100000 -T "Runcontrol" -e 'INSTALL/bin/euRun -n AhcalRunControl |tee -a data/logs/runcontrol_${currentdate}.log; read '&
sleep 2
#################  Log collector #################
xterm -r -sb -sl 1000 -geometry 160x30 -T "Logger" -e 'INSTALL/bin/euLog -r tcp://$HOSTIP:$RCPORT|tee --append data/logs/logcollector_${currentdate} ;read' &
#sleep 1
#################  Data collector #################
#known data collectors:  EventnumberSyncDataCollector, DirectSaveDataCollector, TimestampSyncDataCollector, AhcalHodoscopeDataCollector, CaliceDataCollector, CaliceTelDataCollector, CaliceTsDataCollector, CaliceAhcalBifBxidDataCollector
#EventnumberSyncDataCollector
xterm -r -sb -sl 100000 -geometry 80x4 -T "DC1" -e "INSTALL/bin/euCliCollector -n AlpideDataCollector -t dc1 -r tcp://$HOSTIP:$RCPORT | tee -a data/logs/dc1_${currentdate}.log; read" &
#not yet written: 
#xterm -r -sb -sl 100000 -geometry 80x4 -T "Total 5 Collector 1" -e "INSTALL/bin/euCliCollector -n AhcalBifTluAlpieTableDataCollector -t dcall1 -r tcp://$HOSTIP:$RCPORT | tee -a data/logs/dcall1_${currentdate}.log; read" &
xterm -r -sb -sl 100000 -geometry 160x24 -T "BXID col" -e "INSTALL/bin/euCliCollector -n CaliceAhcalBifBxidDataCollector -t bxidColl1 | tee -a data/logs/dcbxid1_${currentdate}.log ; read" &
xterm -r -sb -sl 100000 -geometry 80x4 -T "DS DC2" -e "INSTALL/bin/euCliCollector -n DirectSaveDataCollector -t dc2 -r tcp://$HOSTIP:$RCPORT | tee -a data/logs/dc2_${currentdate}.log; read" &
#xterm -r -sb -sl 100000 -geometry 80x4 -T "DC3" -e "INSTALL/bin/euCliCollector -n TriggerIDSyncDataCollector -t dc3 -r tcp://$HOSTIP:$RCPORT | tee -a data/logs/dc3_${currentdate}.log; read" &


#xterm -r -sb -sl 100000 -T "directsave Data collector 3" -e 'INSTALL/bin/euCliCollector -n DirectSaveDataCollector -t dc3 ; read' &
#xterm -sb -sl 100000 -geometry 160x30 -T "Hodoscope Data collector" -e 'INSTALL/bin/euCliCollector -n AhcalHodoscopeDataCollector -t dc2 | tee -a logs/dc2.log; read' &
#xterm -sb -sl 100000 -geometry 160x30 -T "slcio Hodoscope Data collector" -e 'INSTALL/bin/euCliCollector -n AhcalHodoscopeDataCollector -t dc3 | tee -a logs/dc3.log; read' &
#xterm -sb -sl 100000  -T "Data collector2" -e 'INSTALL/bin/euCliCollector -n DirectSaveDataCollector -t dc2 ; read' &
#sleep 1

#################  Producer #################
#xterm -sb -sl 1000000 -geometry 160x30 -T "Hodoscope" -e 'INSTALL/bin/euCliProducer -n CaliceEasirocProducer -t hodoscope1 -r tcp://$HOSTIP:$RCPOR|tee -a logs/hodoscope1.log && read || read'&
#xterm -sb -sl 1000000 -geometry 160x30 -T "Hodoscope 2" -e 'INSTALL/bin/euCliProducer -n CaliceEasirocProducer -t hodoscope2 -r tcp://$HOSTIP:$RCPOR|tee -a logs/hodoscope2.log && read || read'&
# sleep 1
xterm -r -sb -sl 100000 -geometry 160x30 -T "AHCAL" -e 'INSTALL/bin/euCliProducer -n AHCALProducer -t AHCAL1 -r tcp://$HOSTIP:$RCPORT |tee -a data/logs/ahcal_${currentdate}.log ; read'&
xterm -r -sb -sl 10000 -geometry 160x30 -T "DESY table" -e 'INSTALL/bin/euCliProducer -n DesyTableProducer -t desytable1 -r tcp://$HOSTIP:$RCPORT |tee -a data/logs/desytable_${currentdate}.log ;read'&
xterm -r -sb -sl 100000 -geometry 160x24 -T "BIF" -e "INSTALL/bin/euCliProducer -n caliceahcalbifProducer -t BIF1 -r tcp://$HOSTIP:$RCPORT |tee -a data/logs/bif_${currentdate}.log ; read"&
xterm -r -sb -sl 100000 -geometry 160x24 -T "TLU" -e "INSTALL/bin/euCliProducer -n AidaTluProducer -t aida_tlu -r tcp://$HOSTIP:$RCPORT |tee -a data/logs/tlu_${currentdate}.log ; read"&
xterm -r -sb -sl 100000 -geometry 160x24 -T "ALPIDE" -e "INSTALL/bin/euCliProducer -n AltelProducer -t altel -r tcp://$HOSTIP:$RCPORT |tee -a data/logs/alpide_${currentdate}.log ; read"&

#################  Online Monitor #################
echo "starting online monitor"
#xterm -sb -sl 100000  -T "Online monitor" -e 'INSTALL/bin/StdEventMonitor -c conf/onlinemonitor.conf --monitor_name StdEventMonitor --reset ; read' &
xterm -sb -sl 100000  -T "Online monitor" -e 'INSTALL/bin/StdEventMonitor -c conf/onlinemonitor.conf --monitor_name StdEventMonitor --reset --root -r tcp://$HOSTIP:$RCPORT ; read' &
echo "online monitor started"


