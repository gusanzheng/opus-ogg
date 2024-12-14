g++ -g -std=c++11 -shared -o libopus_ogg.so interface.cpp opus_ogg.cpp encoder.cpp decoder.cpp -fPIC -L ./lib -lopus -logg
go build main.go