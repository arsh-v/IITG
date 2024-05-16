rm -rf sender
rm -rf receiver
rm -rf tracker
rm -rf rec
rm -rf sen
rm -rf tra
rm -rf test


g++ sender.cpp -o sender
g++ receiver.cpp -o receiver $(pkg-config --libs --cflags gtk+-3.0)
g++ tracker.cpp -o tracker
g++ receiver.cpp -o rec $(pkg-config --libs --cflags gtk+-3.0)
g++ sender.cpp -o sen
g++ tracker.cpp -o tra
g++ testing_code.cpp -o test