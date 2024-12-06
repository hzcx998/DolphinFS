./compile.sh
./gendisk.sh
./dolpfs mkfs
./dolpfs put README.md README.md
./dolpfs list /
./dolpfs get README.md README.md2
./dolpfs list /
./dolpfs copy README.md README.md3
./dolpfs list /
./dolpfs delete README.md
./dolpfs list /
./dolpfs mkdir abc
./dolpfs list /
rm README.md2
rm disk.vhd
rm dolpfs