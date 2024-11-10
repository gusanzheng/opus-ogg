# g++ -std=c++11 -g -o main main.cpp encoder.cpp decoder.cpp -I ./ -I /usr/local/include/opus -lopus -logg

# export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:/usr/local/lib
g++ -g -std=c++11 -shared -o libopus_ogg.so interface.cpp opus_ogg.cpp encoder.cpp decoder.cpp -fPIC -L ./lib -lopus -logg
go build main.go