# Test of server and client
server -p 8000 -d /home/steven/temp
client -p 8000 -f download_file1
client -p 8000 -f download_file2
client -p 8000 -f download_file3
client -p 8000 -d download_dir
server -s
quit
