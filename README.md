# SC3020-CZ4031-Project-1
Group 11 Project 1 DBMS Storage and Indexing

## Installation
The code needs to compiled and run from the `code` directory. The `data.tsv` file must be present in the current working directory where the executable is run from (included in the `code` folder). We recommend using gcc to compile and run the code.  
```sh
cd code
g++ main.cc disk.cc -o DB
./DB
```

## Getting g++

### Windows
MinGW-g64 via [MYSYS2](https://www.msys2.org/)

### MacOS
g++ is included in the Xcode command line tools.
```
xcode-select --install
```

### Linux
If g++ isn't installed already, on a Debian-based system (such as Ubuntu), it can be installed with
```
sudo apt-get update
sudo apt-get install build-essential
```
