HOST=$1

make tcp-server

scp tmp/tcp-server/tmp/server/kserverd root@$HOST:/tmp/kserverd
scp middleware/kserver.conf root@$HOST:/tmp/kserver.conf

ssh -t -t root@$HOST << EOF
pkill -SIGINT kserverd
cp /tmp/kserverd /usr/local/tcp-server/kserverd
cp /tmp/kserver.conf /usr/local/tcp-server/kserver.conf
/usr/local/tcp-server/kserverd -c /usr/local/tcp-server/kserver.conf
rm /tmp/kserverd
exit
EOF
