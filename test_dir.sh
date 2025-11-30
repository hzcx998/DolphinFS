./compile.sh
./gendisk.sh
./dolpfs mkfs
./dolpfs test_dir
rm dolpfs
rm disk.vhd