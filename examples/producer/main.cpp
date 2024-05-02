/*
 _________________________________________________________
|   _          __ _                _              _       |
|  | |        / _| |              | |            | |      |
|  | | ____ _| |_| | ____ _   __  | |_ ___   ___ | |___   |  C++ Kafka tools (producer/consumer)
|  | |/ / _` |  _| |/ / _` | |__| | __/ _ \ / _ \| / __|  |  Version 1.0.z
|  |   < (_| | | |   < (_| |      | || (_) | (_) | \__ \  |  https://github.com/testillano/kafka-tools
|  |_|\_\__,_|_| |_|\_\__,_|       \__\___/ \___/|_|___/  |
|_________________________________________________________|

Licensed under the MIT License <http://opensource.org/licenses/MIT>.
SPDX-License-Identifier: MIT
Copyright (c) 2024 Eduardo Ramos

Permission is hereby  granted, free of charge, to any  person obtaining a copy
of this software and associated  documentation files (the "Software"), to deal
in the Software  without restriction, including without  limitation the rights
to  use, copy,  modify, merge,  publish, distribute,  sublicense, and/or  sell
copies  of  the Software,  and  to  permit persons  to  whom  the Software  is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE  IS PROVIDED "AS  IS", WITHOUT WARRANTY  OF ANY KIND,  EXPRESS OR
IMPLIED,  INCLUDING BUT  NOT  LIMITED TO  THE  WARRANTIES OF  MERCHANTABILITY,
FITNESS FOR  A PARTICULAR PURPOSE AND  NONINFRINGEMENT. IN NO EVENT  SHALL THE
AUTHORS  OR COPYRIGHT  HOLDERS  BE  LIABLE FOR  ANY  CLAIM,  DAMAGES OR  OTHER
LIABILITY, WHETHER IN AN ACTION OF  CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE  OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

// C
#include <libgen.h> // basename

// Standard
#include <iostream>
#include <iomanip>
#include <string>
#include <unistd.h>
#include <chrono>
#include <thread>
#include <mutex>
#include <fstream>
#include <vector>

#include <ert/tracing/Logger.hpp>

#include <cppkafka/cppkafka.h>


std::mutex SequenceMutex;
unsigned int Sequence{};


const char* progname;

void producer_thread(int thread_id, const std::string &brokers, const std::string &topic, const std::string &message, unsigned int maxMessages, int workerDelayMs) {
    cppkafka::Configuration config = {
        { "metadata.broker.list", brokers }
    };

    cppkafka::Producer producer(config);

    while (true) {
        int sequence_value = -1;

        {
            std::lock_guard<std::mutex> lock(SequenceMutex);
            if (Sequence < maxMessages) {
                sequence_value = Sequence;
                Sequence++;
            }
        }

        if (sequence_value == -1) {
            break;
        }

        std::string msg = message + std::to_string(sequence_value);
        try {
            producer.produce(cppkafka::MessageBuilder(topic).partition(-1).payload(msg));
            producer.flush();
            LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Message sent successfully on thread %d: %s", thread_id, msg.c_str()), ERT_FILE_LOCATION));
        }
        catch (const std::exception& ex) {
            ert::tracing::Logger::error(ex.what(), ERT_FILE_LOCATION);
            return;
        }

        std::this_thread::sleep_for(std::chrono::milliseconds(workerDelayMs));
    }
}

void usage(const char *progname) {
    std::cout << "Usage:   " << progname << " [-h|--help] [--brokers <brokers>] [--topic <topic>] [--message <message>] [--max-messages <value>] [--workers <value>] [--worker-delay-ms <value>] [--debug]\n\n"
              << "         brokers:         defaults to 'localhost:9092'\n"
              << "         topic:           defaults to 'test'\n"
              << "         message:         defaults to 'hello-world'\n"
              << "         max-messages:    defaults to '1'\n"
              << "         workers:         defaults to '1'\n"
              << "         worker-delay-ms: defaults to '1000'\n\n";

    exit(0);
}

int main(int argc, char* argv[]) {

    progname = basename(argv[0]);
    ert::tracing::Logger::initialize(progname);

    LOGINFORMATIONAL(ert::tracing::Logger::informational("Starting ...", ERT_FILE_LOCATION));

    std::string brokers("localhost:9092");
    std::string topic("test");
    std::string message("hello-world");
    unsigned int maxMessages = 1;
    unsigned int workers = 1;
    int workerDelayMs = 1000;

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "-h" || std::string(argv[i]) == "--help") {
            usage(progname);
        }
        if (std::string(argv[i]) == "--brokers") {
            if (i + 1 < argc) {
                brokers = argv[++i];
            }
        }
        if (std::string(argv[i]) == "--topic") {
            if (i + 1 < argc) {
                topic = argv[++i];
            }
        }
        if (std::string(argv[i]) == "--message") {
            if (i + 1 < argc) {
                message = argv[++i];
            }
        }
        if (std::string(argv[i]) == "--max-messages") {
            if (i + 1 < argc) {
                maxMessages = std::stoi(argv[++i]);
            }
        }
        else if (std::string(argv[i]) == "--workers") {
            if (i + 1 < argc) {
                workers = std::stoi(argv[++i]);
            }
        }
        else if (std::string(argv[i]) == "--worker-delay-ms") {
            if (i + 1 < argc) {
                workerDelayMs = std::stoi(argv[++i]);
            }
        }
        else if (std::string(argv[i]) == "--debug") {
            ert::tracing::Logger::setLevel("Debug");
            ert::tracing::Logger::verbose();
        }
    }

    // Workers:
    std::vector<std::thread> threads;
    for (int i = 0; i < workers; ++i) {
        threads.emplace_back(producer_thread, i + 1, brokers, topic, message, maxMessages, workerDelayMs);
    }

    // Join threads:
    for (auto &thread : threads) {
        thread.join();
    }

    return 0;
}

