all: build test

build: mmaptest32 mmaptest64 unaligned64

mmaptest32: mmaptest.c
	@gcc -m32 -msse -Wall mmaptest.c -o mmaptest32

mmaptest64: mmaptest.c
	@gcc -m64 mmaptest.c -Wall -o mmaptest64

unaligned64: unaligned.c
	@gcc -m64 unaligned.c -Wall -o unaligned64

clean:
	@rm -f mmaptest32 mmaptest64 mmaptest32.stdout.tmp mmaptest32.mmt.tmp mmaptest64.stdout.tmp mmaptest64.mmt.tmp unaligned64 unaligned64.stdout.tmp unaligned64.mmt.tmp

test: test32 test64 test_unaligned64

test32: mmaptest32
	@../../coregrind/valgrind --tool=mmt --mmt-trace-file=/dev/zero --log-file=mmaptest32.mmt.tmp ./mmaptest32 >mmaptest32.stdout.tmp || (cat mmaptest32.stdout.tmp && cat mmaptest32.mmt.tmp && false)
	@diff -u mmaptest32.stdout mmaptest32.stdout.tmp
	@cat mmaptest32.mmt.tmp | mmt_bin2dedma | grep -v sys_open | grep "^--" | sed "s/^--[0-9]*-- //" | diff -u mmaptest32.mmt.dedma -

test64: mmaptest64
	@../../coregrind/valgrind --tool=mmt --mmt-trace-file=/dev/zero --log-file=mmaptest64.mmt.tmp ./mmaptest64 >mmaptest64.stdout.tmp || (cat mmaptest64.stdout.tmp && cat mmaptest64.mmt.tmp && false)
	@diff -u mmaptest64.stdout mmaptest64.stdout.tmp
	@cat mmaptest64.mmt.tmp | mmt_bin2dedma | grep -v sys_open | grep "^--" | sed "s/^--[0-9]*-- //" | diff -u mmaptest64.mmt.dedma -

test_unaligned64: unaligned64
	@../../coregrind/valgrind --tool=mmt --mmt-trace-file=/dev/zero --log-file=unaligned64.mmt.tmp ./unaligned64 >unaligned64.stdout.tmp || (cat unaligned64.stdout.tmp && cat unaligned64.mmt.tmp && false)
	@cat unaligned64.mmt.tmp | mmt_bin2dedma | grep -v sys_open | grep "^--" | sed "s/^--[0-9]*-- //" | diff -u unaligned64.mmt.dedma -
