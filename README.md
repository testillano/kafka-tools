# C++ Kafka Tools

[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](https://opensource.org/licenses/MIT)
[![Documentation](https://codedocs.xyz/testillano/kafka-tools.svg)](https://codedocs.xyz/testillano/kafka-tools/index.html)
[![Ask Me Anything !](https://img.shields.io/badge/Ask%20me-anything-1abc9c.svg)](https://github.com/testillano)
[![Maintenance](https://img.shields.io/badge/Maintained%3F-yes-green.svg)](https://github.com/testillano/kafkatools/graphs/commit-activity)
[![Main project workflow](https://github.com/testillano/kafka-tools/actions/workflows/ci.yml/badge.svg)](https://github.com/testillano/kafka-tools/actions/workflows/ci.yml)
[![Docker Pulls](https://img.shields.io/docker/pulls/testillano/kafkatools.svg)](https://github.com/testillano/kafka-tools/pkgs/container/kafkatools)

This project is based on @mfontanini cppkafka library (https://github.com/mfontanini/cppkafka) and so on, based on @confluentinc librdkafka library (https://github.com/confluentinc/librdkafka).

## Project image

This image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$ docker pull ghcr.io/testillano/kafkatools:<tag>
```

You could also build it using the script `./build.sh` located at project root:


```bash
$ ./build.sh --project-image
```

This image is built with `./Dockerfile`.

## Usage

To run compilation over this image, just run with `docker`. The `entrypoint` (check it at `./deps/build.sh`) will fall back from `cmake` (looking for `CMakeLists.txt` file at project root, i.e. mounted on working directory `/code` to generate makefiles) to `make`, in order to build your source code. There are two available environment variables used by the builder script of this image: `BUILD_TYPE` (for `cmake`) and `MAKE_PROCS` (for `make`):

```bash
$ envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
         ghcr.io/testillano/kafkatools:<tag>
```

## Build project with docker

### Builder image

This image is already available at `github container registry` and `docker hub` for every repository `tag`, and also for master as `latest`:

```bash
$ docker pull ghcr.io/testillano/kafkatools_builder:<tag>
```

You could also build it using the script `./build.sh` located at project root:


```bash
$ ./build.sh --builder-image
```

This image is built with `./Dockerfile.build`.

### Usage

Builder image is used to build the project library. To run compilation over this image, again, just run with `docker`:

```bash
$ envs="-e MAKE_PROCS=$(grep processor /proc/cpuinfo -c) -e BUILD_TYPE=Release"
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
         ghcr.io/testillano/kafkatools_builder:<tag>
```

You could generate documentation passing extra arguments to the [entry point](https://github.com/testillano/kafkatools/blob/master/deps/build.sh) behind:

```bash
$ docker run --rm -it -u $(id -u):$(id -g) ${envs} -v ${PWD}:/code -w /code \
         ghcr.io/testillano/kafkatools_builder::<tag>-build "" doc
```

You could also build the library using the script `./build.sh` located at project root:


```bash
$ ./build.sh --project
```

## Build project natively

This is a cmake-based building library, so you may install cmake:

```bash
$ sudo apt-get install cmake
```

And then generate the makefiles from project root directory:

```bash
$ cmake .
```

You could specify type of build, 'Debug' or 'Release', for example:

```bash
$ cmake -DCMAKE_BUILD_TYPE=Debug .
$ cmake -DCMAKE_BUILD_TYPE=Release .
```

You could also change the compilers used:

```bash
$ cmake -DCMAKE_CXX_COMPILER=/usr/bin/g++     -DCMAKE_C_COMPILER=/usr/bin/gcc
```
or

```bash
$ cmake -DCMAKE_CXX_COMPILER=/usr/bin/clang++ -DCMAKE_C_COMPILER=/usr/bin/clang
```

### Build

```bash
$ make
```

### Clean

```bash
$ make clean
```

### Documentation

```bash
$ make doc
```

```bash
$ cd docs/doxygen
$ tree -L 1
     .
     ├── Doxyfile
     ├── html
     ├── latex
     └── man
```

### Install

```bash
$ sudo make install
```

Optionally you could specify another prefix for installation:

```bash
$ cmake -DMY_OWN_INSTALL_PREFIX=$HOME/mytools/kafkatools
$ make install
```

### Uninstall

```bash
$ cat install_manifest.txt | sudo xargs rm
```

## Apache Kafka native installation

### Download

Download and extract latest version from https://kafka.apache.org/downloads, for example:

```bash
$ wget https://downloads.apache.org/kafka/3.7.0/kafka_2.13-3.7.0.tgz
$ tar -xzf kafka_2.13-3.7.0.tgz
$ cd kafka_2.13-3.7.0/
```

### Start

Install `JRE` requirement:

```bash
$ sudo apt update
$ sudo apt install default-jre
```

Then, start `zookeeper` and `kafka server`:

```bash
$ bin/zookeeper-server-start.sh config/zookeeper.properties # terminal 1
$ bin/kafka-server-start.sh config/server.properties # terminal 2
```

### Test

Create a test a topic:

```bash
$ bin/kafka-topics.sh --create --topic test --bootstrap-server localhost:9092 --replication-factor 1 --partitions 1
$ bin/kafka-console-producer.sh --topic test --bootstrap-server localhost:9092 # terminal 1
$ bin/kafka-console-consumer.sh --topic test --bootstrap-server localhost:9092 --from-beginning # terminal 2
```

## Examples

### Kafka-producer

You could also test kafka installation using this simple producer, just using docker image, for example:

```bash
$ docker run --rm -it --network=host --entrypoint "/opt/kafka-producer" ghcr.io/testillano/kafkatools:latest --help
```

You could omit entry point, as that simple producer is the default for project image.

### Udp-server-kafka-producer

This is an advanced kafka producer which triggers actively kafka messages for every UDP reception. You can use netcat in bash, to generate UDP messages easily:

```
$ echo -n "<message here>" | nc -u -q0 -w1 -U /tmp/udp.sock
```

But, you could also use the `h2agent` project UDP client generator (https://github.com/testillano/h2agent/tree/master?tab=readme-ov-file#execution-of-udp-client-utility) which allows to drive UDP traffic load with specific rate and ramp up time.

Powerful parsing capabilities allow to create any kind of message dynamically using patterns for message configured. This, together with UDP client generator will enable any kind of kafka production needs.

```bash
$ docker run --rm -it --network=host --entrypoint "/opt/udp-server-kafka-producer" ghcr.io/testillano/kafkatools:latest --help
```

It is recommended to read this guide to work with unix sockets and docker containers: https://github.com/testillano/h2agent/tree/master?tab=readme-ov-file#working-with-unix-sockets-and-docker-containers. There, `udp-server-h2client` is the functional equivalent to this `udp-server-kafka-producer`.

In the following example, we will produce 1000 messages per second during about 10 seconds, with the sequence as message content for topic 'test':

```bash
$ docker volume create --name=socketVolume
$ docker run --rm -it --network=host -v socketVolume:/tmp --entrypoint /opt/udp-server-kafka-producer ghcr.io/testillano/kafkatools:latest -k /tmp/udp.sock --message @{udp} # terminal 1
$ docker run --rm -it -v socketVolume:/tmp --entrypoint /opt/udp-client ghcr.io/testillano/h2agent:latest -k /tmp/udp.sock --final 10000  --pattern "@{seq}" --eps 1000 # terminal 2
```

## Contributing

Please, execute `astyle` formatting (using [frankwolf image](https://hub.docker.com/r/frankwolf/astyle)) before any pull request:

```bash
$ sources=$(find . -name "*.hpp" -o -name "*.cpp")
$ docker run -i --rm -v $PWD:/data frankwolf/astyle ${sources}
```

