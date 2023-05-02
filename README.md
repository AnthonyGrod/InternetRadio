## InternetRadio
This program simulates an internet radio using UDP connection. 

## How does it work?
Audio data streamed via stdin into sikradio-sender is sent via UDP to sikradio-receiver, which outputs the audio data on stdout.
sikradio-receiver uses a cyclical buffer to which one thread is writing into from stdin and the second thread is writing from to stdout.
Additionally, the program writes onto cerr numbers of missing UDP packets

## How to run it?
An example usage with Makefile:
```
make
./sikradio-receiver -a 10.10.11.12 | play -t raw -c 2 -r 44100 -b 16 -e signed-integer --buffer 32768 -
sox -S "music.mp3" -r 44100 -b 16 -e signed-integer -c 2 -t raw - | pv -q -L \$((44100\*4)) | ./sikradio-sender -a 10.10.11.12 -n "My Radio"
```
Using CMake:
```
mkdir build
cd build
cmake ..
./sikradio-receiver -a 10.10.11.12 | play -t raw -c 2 -r 44100 -b 16 -e signed-integer --buffer 32768 -
sox -S "music.mp3" -r 44100 -b 16 -e signed-integer -c 2 -t raw - | pv -q -L \$((44100\*4)) | ./sikradio-sender -a 10.10.11.12 -n "My Radio"
```
