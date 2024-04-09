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
#include <fstream>
#include <vector>

#include <ert/tracing/Logger.hpp>

#include <cppkafka/cppkafka.h>


class MyKafkaProducer {
public:
    MyKafkaProducer(const std::string& kafkaServer, const std::string& topic)
        : server_(kafkaServer), topic_(topic) {
        config_.set("metadata.broker.list", server_);
        producer_ = std::make_shared<cppkafka::Producer>(config_);
    }

    void sendData(const std::string& content) {
        producer_->produce(cppkafka::MessageBuilder(topic_).payload(content));
        producer_->flush();
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Production on topic '%s': %s", topic_.c_str(), content.c_str()), ERT_FILE_LOCATION));
    }

private:
    std::string server_;
    std::string topic_;
    cppkafka::Configuration config_;
    std::shared_ptr<cppkafka::Producer> producer_;
};


const char* progname;

int main(int argc, char* argv[]) {

    progname = basename(argv[0]);
    ert::tracing::Logger::initialize(progname);
    ert::tracing::Logger::setLevel("Debug");

    if (argc < 5) {
        std::cerr << "Usage: " << argv[0] << " <brokers (i.e. localhost:9092)> <topic> <file> [--delay-ms <value>] [--verbose]\n";
        return 1;
    }

    LOGINFORMATIONAL(ert::tracing::Logger::informational("Starting ...", ERT_FILE_LOCATION));

    std::string brokers(argv[1]);
    std::string topic(argv[2]);
    std::string filepath(argv[3]);

    std::ifstream file(filepath);
    if (!file.is_open()) {
        ert::tracing::Logger::error(ert::tracing::Logger::asString("Cannot open provided file '%s'", filepath.c_str()), ERT_FILE_LOCATION);
        return 1;
    }

    std::stringstream buffer;
    buffer << file.rdbuf();
    file.close();
    std::string content = buffer.str();

    int delayMs = 0;
    for (int i = 4; i < argc; ++i) {
        if (std::string(argv[i]) == "--delay-ms") {
            if (i + 1 < argc) {
                delayMs = std::stoi(argv[++i]);
            }
        } else if (std::string(argv[i]) == "--verbose") {
            ert::tracing::Logger::verbose();
        }
    }

    if (delayMs > 0) {
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("Delay before producing: %d ms", delayMs), ERT_FILE_LOCATION));
        std::this_thread::sleep_for(std::chrono::milliseconds(delayMs));
        LOGDEBUG(ert::tracing::Logger::debug("Delay completed !", ERT_FILE_LOCATION));
    }

    try {
        MyKafkaProducer producer(brokers, topic);
        producer.sendData(content);
        LOGDEBUG(ert::tracing::Logger::debug("Message sent successfully !", ERT_FILE_LOCATION));
    } catch (const std::exception& ex) {
        ert::tracing::Logger::error(ex.what(), ERT_FILE_LOCATION);
        return 1;
    }

    return 0;
}

