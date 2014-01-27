# jshon

Jshon is a JSON parser designed for maximum convenience within the shell. Its homepage is [http://kmkeen.com/jshon/](http://kmkeen.com/jshon/).

## Dependencies

Your system will need to provide the [`jansson` library](http://www.digip.org/jansson/).

### Install / Building 

#### Arch/Chakra Linux

    pacman -S jshon

#### Debian/Ubuntu/Linux Mint

These directions assume you have already downloaded the sources and placed yourself in the directory. 

First, install the `jansson` library :

    sudo apt-get install libjansson{4,-dev}

Then build the sources, by running :

    make

### Test

    echo '{"a":1,"b":[true,false,null,"none"],"c":{"d":4,"e":5}}' > sample.json
    ./jshon -t < sample.json
    # output: object
