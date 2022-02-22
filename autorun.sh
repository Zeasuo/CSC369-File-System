rm -rf runs/*
rm -rf results/*

#--- First, copy out the images ---

# Copy
cp images/emptydisk.img runs/case1-cp.img
cp images/emptydisk.img runs/case2-cp-large.img
cp images/removed.img runs/case3-cp-rm-dir.img

# Mkdir
cp images/emptydisk.img runs/case4-mkdir.img
cp images/onedirectory.img runs/case5-mkdir-2.img

# Link
cp images/twolevel.img runs/case6-ln-hard.img
cp images/twolevel.img runs/case7-ln-soft.img

# Remove
cp images/manyfiles.img runs/case8-rm.img
cp images/manyfiles.img runs/case9-rm-2.img
cp images/manyfiles.img runs/case10-rm-3.img
cp images/largefile.img runs/case11-rm-large.img

# Concurrency
cp images/emptydisk.img runs/case12-concurrent-cp.img
cp images/emptydisk.img runs/case13-concurrent-ln-hl.img
cp images/emptydisk.img runs/case14-concurrent-ln-sl.img
cp images/emptydisk.img runs/case15-concurrent-rm.img
cp images/emptydisk.img runs/case16-concurrent-mkdir.img
cp images/emptydisk.img runs/case17-concurrent-all-one-time.img
cp images/emptydisk.img runs/case18-concurrent-all-five-times.img

#--- Now, do the test cases ---

# Copy
echo "Copy Test 1"
wrappers/ext2_cp runs/case1-cp.img files/oneblock.txt /file.txt
echo "Copy Test 2"
wrappers/ext2_cp runs/case2-cp-large.img files/largefile.txt /big.txt
echo "Copy Test 3"
wrappers/ext2_cp runs/case3-cp-rm-dir.img files/oneblock.txt /level1/file

# Mkdir
echo "Mkdir Test 4"
wrappers/ext2_mkdir runs/case4-mkdir.img /level1/
echo "Mkdir Test 5"
wrappers/ext2_mkdir runs/case5-mkdir-2.img /level1/level2

# Remove
echo "Remove Test 8"
wrappers/ext2_rm runs/case8-rm.img /c.txt
echo "Remove Test 9"
wrappers/ext2_rm runs/case9-rm-2.img /level1/d.txt
echo "Remove Test 10"
wrappers/ext2_rm runs/case10-rm-3.img /level1/e.txt
echo "Remove Test 11"
wrappers/ext2_rm runs/case11-rm-large.img /largefile.txt

# Concurrency
echo "Concurrency Test 12"
wrappers/ext2_concurrent_cp runs/case12-concurrent-cp.img
echo "Concurrency Test 15"
wrappers/ext2_concurrent_rm runs/case15-concurrent-rm.img
echo "Concurrency Test 16"
wrappers/ext2_concurrent_mkdir runs/case16-concurrent-mkdir.img
echo "Concurrency Test 17"
wrappers/ext2_concurrent_all_one_time runs/case17-concurrent-all-one-time.img
echo "Concurrency Test 18"
wrappers/ext2_concurrent_all_five_times runs/case18-concurrent-all-five-times.img

# --- Now do the dumps ---
the_files="$(ls runs)"
for the_file in $the_files
do
	g=$(basename $the_file)
	./out/img/ext2_dump runs/$g > results/$g.txt
done


