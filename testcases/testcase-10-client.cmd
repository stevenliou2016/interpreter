# Test of server and client
server -p 7000 -d /home/steven/temp
sleep
client -p 7000 -f download_file1
client -p 7000 -f download_file2
client -p 7000 -f download_file3
client -p 7000 -d download_dir
server -s
quit
