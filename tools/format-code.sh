find . -name \*.c -or -name \*.h -or -name \*.cpp -or -name \*.hpp | xargs astyle --style=java -H -Y -s2 -U -k3 -W3 -xC128 -xL -p -o -Q -xp
git diff --check
#astyle --style=java -H -Y -U -k3 -W3 -xC140 -xL -p -U -k3 -xw smogrify
