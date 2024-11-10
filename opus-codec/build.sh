g++ -g -std=c++11 -shared -o libopus_codec.so interface.cpp opus_codec.cpp -fPIC -I /usr/local/include/opus -L ./lib -lopus
go build -o main main.go