echo "************** TESTSET: Running Testcases with READER TEST **************"
gcc my_reader_test.c rw_lock-r-test.c rw_lock.c -o rw_lock-r-test -lpthread

echo "Reader Test with 5 reader and 5 writer"
out=`./rw_lock-r-test 5 5`
echo -e $out

echo "************** TESTSET: Running Testcases with WRITER TEST ************** "
gcc my_writer_test.c rw_lock-w-test.c rw_lock.c -o rw_lock-w-test -lpthread

echo "Writer Test with 5 reader and 5 writer"
out=`./rw_lock-w-test 5 5`
echo -e $out

echo "************** TEST END ************** "
