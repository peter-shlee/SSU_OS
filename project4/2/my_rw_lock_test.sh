echo "************** TESTSET: Running Testcases with READER TEST **************"
gcc my_reader_test.c rw_lock-r-test.c rw_lock.c -o rw_lock-r-test -lpthread

echo "CASE1: Reader Test with 5 reader and 1 writer"
out=`./rw_lock-r-test 5 1`
echo -e $out

echo "CASE2: Reader Test with 5 reader and 3 writer"
out=`./rw_lock-r-test 5 3`
echo -e $out

echo "CASE3: Reader Test with 5 reader and 5 writer"
out=`./rw_lock-r-test 5 5`
echo -e $out

echo "************** TESTSET: Running Testcases with WRITER TEST ************** "
gcc my_writer_test.c rw_lock-w-test.c rw_lock.c -o rw_lock-w-test -lpthread

echo "CASE1: Writer Test with 5 reader and 1 writer"
out=`./rw_lock-w-test 5 1`
echo -e $out


echo "CASE2: Writer Test with 5 reader and 3 writer"
out=`./rw_lock-w-test 5 3`
echo -e $out

echo "CASE3: Writer Test with 5 reader and 5 writer"
out=`./rw_lock-w-test 5 5`
echo -e $out

echo "************** TEST END ************** "
