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

// Standard
#include <libgen.h> // basename
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <signal.h>

#include <iostream>
#include <ctime>
#include <iomanip>
#include <string>
#include <map>
#include <thread>
#include <memory>
#include <atomic>
#include <regex>
#include <chrono>
#include <algorithm>
#include <mutex>

#include <cctype> // parse
#include <functional> // parse

#include <ert/tracing/Logger.hpp>

#include <cppkafka/cppkafka.h>
#include <cppkafka/exceptions.h>
#include <boost/asio.hpp>


#define BUFFER_SIZE 256
#define COL1_WIDTH 36 // date time and microseconds
#define COL2_WIDTH 16 // sequence
#define COL3_WIDTH 32 // 256 is too much, but we could accept UDP datagrams with that size ...
#define COL4_WIDTH 100 // status codes


const char* progname;

// Results statistics
std::atomic<unsigned int> PRODUCTIONS_OK{};
std::atomic<unsigned int> PRODUCTIONS_ERROR{};
std::mutex Mutex;

// Globals
std::map<std::string, std::string> MessagePatterns{};
int Sockfd{}; // global to allow signal wrapup
std::string UdpSocketPath{}; // global to allow signal wrapup
int HardwareConcurrency = std::thread::hardware_concurrency();
int Workers = 10*HardwareConcurrency; // default

////////////////////////////
// Command line functions //
////////////////////////////

void usage(int rc, const std::string &errorMessage = "")
{
    auto& ss = (rc == 0) ? std::cout : std::cerr;

    ss << "Usage: " << progname << " [options]\n\nOptions:\n\n"

       << "UDP server will trigger one Kafka production for every reception, replacing optionally\n"
       << "certain patterns on message provided. Implemented patterns:\n"
       << '\n'
       << "   @{udp}:      replaced by the whole UDP datagram received.\n"
       << "   @{udp8}:     selects the 8 least significant digits in the UDP datagram, and may\n"
       << "                be used to build valid IPv4 addresses for a given sequence.\n"
       << "   @{udp.<n>}:  UDP datagram received may contain a pipe-separated list of tokens\n"
       << "                and this pattern will be replaced by the nth one.\n"
       << "\n"
       << "To stop the process you can send UDP message 'EOF'.\n"
       << "To print accumulated statistics you can send UDP message 'STATS' or stop/interrupt the process.\n"
       << "To flush kafka producer you can send UDP message 'FLUSH'.\n\n"

       << "-k|--udp-socket-path <value>\n"
       << "  UDP unix socket path.\n\n"

       << "[-w|--workers <value>]\n"
       << "  Number of worker threads to produce messages. By default, 10x times 'hardware\n"
       << "  concurrency' is configured (10*" << HardwareConcurrency << " = " << Workers << "), but you could consider increase even more\n"
       << "  if high I/O is expected (high response times raise busy threads, so context switching\n"
       << "  is not wasted as much as low latencies setups do). We should consider Amdahl law and\n"
       << "  other specific conditions to set the default value, but 10*CPUs is a good approach\n"
       << "  to start with. You may also consider using 'perf' tool to optimize your configuration.\n\n"

       << "[-e|--print-each <value>]\n"
       << "  Print UDP receptions each specific amount (must be positive). Defaults to 1.\n"
       << "  Setting datagrams estimated rate should take 1 second/printout and output\n"
       << "  frequency gives an idea about UDP receptions rhythm.\n\n"

       << "[-l|--log-level <Debug|Informational|Notice|Warning|Error|Critical|Alert|Emergency>]\n"
       << "  Set the logging level; defaults to warning.\n\n"

       << "[-v|--verbose]\n"
       << "  Output log traces on console.\n\n"

       << "[-d|--production-delay-milliseconds <value>]\n"
       << "  Time in seconds to delay before producing the message. Defaults to 0.\n"
       << "  It also supports negative values which turns into random number in\n"
       << "  the range [0,abs(value)].\n\n"

       << "[-m|--message <value>]\n"
       << "  Kafka message. Defaults to 'Hello World'.\n\n"

       << "[-b|--brokers <value>]\n"
       << "  Kafka brokers. Defaults to 'localhost:9092'.\n\n"

       << "[-t|--topic <value>]\n"
       << "  Kafka topic. Defaults to 'test'.\n\n"

       << "[-h|--help]\n"
       << "  This help.\n\n"

       << "Examples: " << '\n'
       << "   " << progname << " --udp-socket-path /tmp/udp.sock --print-each 1000 --message @{udp}" << '\n'

       << '\n'
       << '\n';

    if (rc != 0 && !errorMessage.empty())
    {
        ss << errorMessage << '\n';
    }

    exit(rc);
}

int toNumber(const std::string& value)
{
    int result = 0;

    try
    {
        result = std::stoul(value, nullptr, 10);
    }
    catch (...)
    {
        usage(EXIT_FAILURE, std::string("Error in number conversion for '" + value + "' !"));
    }

    return result;
}

char **cmdOptionExists(char** begin, char** end, const std::string& option, std::string& value)
{
    char** result = std::find(begin, end, option);
    bool exists = (result != end);

    if (exists) {
        if (++result != end)
        {
            value = *result;
        }
    }
    else {
        result = nullptr;
    }

    return result;
}

std::string statsAsString() {
    std::stringstream ss;

    ss << PRODUCTIONS_OK << " OK, " << PRODUCTIONS_ERROR << " errors";

    return ss.str();
}

void wrapup() {
    close(Sockfd);
    unlink(UdpSocketPath.c_str());
    LOGWARNING(ert::tracing::Logger::warning("Stopping logger", ERT_FILE_LOCATION));
    ert::tracing::Logger::terminate();

    // Print status codes statistics:
    std::cout << '\n' << "Restuls: " << statsAsString() << '\n';
}

void sighndl(int signal)
{
    std::cout << "Signal received: " << signal << '\n';
    std::cout << "Wrap-up and exit ..." << '\n';

    // wrap up
    wrapup();

    exit(EXIT_FAILURE);
}

void collectVariablePatterns(const std::string &str, std::map<std::string, std::string> &patterns) {

    static std::regex re("@\\{[^\\{\\}]*\\}", std::regex::optimize); // @{[^{}]*} with curly braces escaped
    // or: R"(@\{[^\{\}]*\})"

    std::string::const_iterator it(str.cbegin());
    std::smatch matches;
    std::string pattern;
    patterns.clear();
    while (std::regex_search(it, str.cend(), matches, re)) {
        it = matches.suffix().first;
        pattern = matches[0];
        patterns[pattern] = pattern.substr(2, pattern.size()-3); // @{foo} -> foo
    }
}

void searchReplaceAll(std::string& str,
                      const std::string& from,
                      const std::string& to)
{
    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("String source to 'search/replace all': %s | from: %s | to: %s", str.c_str(), from.c_str(), to.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );
    std::string::size_type pos = 0u;
    while((pos = str.find(from, pos)) != std::string::npos) {
        str.replace(pos, from.length(), to);
        pos += to.length();
    }

    LOGDEBUG(
        std::string msg = ert::tracing::Logger::asString("String result of 'search/replace all': %s", str.c_str());
        ert::tracing::Logger::debug(msg, ERT_FILE_LOCATION);
    );
}

void replaceVariables(std::string &str, const std::map<std::string, std::string> &patterns, const std::map<std::string,std::string> &vars) {

    if (patterns.empty()) return;
    if (vars.empty()) return;

    std::map<std::string,std::string>::const_iterator it;
    std::unordered_map<std::string,std::string>::const_iterator git;

    for (auto pit = patterns.begin(); pit != patterns.end(); pit++) {

        // local var has priority over a global var with the same name
        if (!vars.empty()) {
            it = vars.find(pit->second);
            if (it != vars.end()) {
                searchReplaceAll(str, pit->first, it->second);
                continue; // all is done
            }
        }
    }
}

// Extract least N significant characters from string:
std::string extractLastNChars(const std::string &input, int n) {
    if(input.size() < n) return input;
    return input.substr(input.size() - n);
}


class Stream {
    std::string data_{}; // UDP

    std::map<std::string, std::string> variables_;

    std::string topic_{};
    std::string message_{};

public:
    Stream(const std::string &data) : data_(data) {;}

    // Set HTTP/2 components
    void setMessage(const std::string &topic, const std::string &message) {
        topic_ = topic;
        message_ = message;

        // Main variable @{udp}
        std::string mainVar = "udp";
        variables_[mainVar] = data_;

        // Reserved variables:
        // @{udp8}
        variables_["udp8"] = extractLastNChars(data_, 8);

        // Variable parts: @{udp.1}, @{udp.2{, etc.
        char delimiter = '|';
        if (data_.find(delimiter) == std::string::npos)
            return;

        std::size_t start = 0;
        std::size_t pos = data_.find(delimiter);
        std::string aux;
        int count = 1;
        while (pos != std::string::npos) {
            aux = mainVar;
            aux += ".";
            aux += std::to_string(count);
            count++;
            variables_[aux] = data_.substr(start, pos - start);
            start = pos + 1;
            pos = data_.find(delimiter, start);
        }
        // add latest
        aux = mainVar;
        aux += ".";
        aux += std::to_string(count);
        variables_[aux] = data_.substr(start);
    }

    std::string getMessage() const { // variables substitution
        std::string result = message_;
        replaceVariables(result, MessagePatterns, variables_);
        return result;
    }

    // Process reception
    void process(cppkafka::Producer &producer) {

        // Produce the message:
        std::unique_lock<std::mutex> lock(Mutex);
        try {
            std::string msg = getMessage();
            producer.produce(cppkafka::MessageBuilder(topic_).partition(-1).payload(msg));
            //producer.flush();
            PRODUCTIONS_OK++;
        }
        catch (const cppkafka::HandleException& ex) {
            ert::tracing::Logger::error(ex.what(), ERT_FILE_LOCATION);
            PRODUCTIONS_ERROR++;
        }
        catch (const cppkafka::QueueException& ex) {
            ert::tracing::Logger::error(ex.what(), ERT_FILE_LOCATION);
            PRODUCTIONS_ERROR++;
        }
        catch (const cppkafka::Exception& ex) {
            ert::tracing::Logger::error(ex.what(), ERT_FILE_LOCATION);
            PRODUCTIONS_ERROR++;
        }

        // Log debug the production, and store results statistics:
        LOGDEBUG(ert::tracing::Logger::debug(ert::tracing::Logger::asString("[process] Data processed: %s", data_.c_str()), ERT_FILE_LOCATION));
    };
};

///////////////////
// MAIN FUNCTION //
///////////////////

int main(int argc, char* argv[])
{
    srand(std::chrono::duration_cast<std::chrono::nanoseconds>(std::chrono::system_clock::now().time_since_epoch()).count());
    progname = basename(argv[0]);

    // Traces
    ert::tracing::Logger::initialize(progname); // initialize logger (before possible myExit() execution):

    // Parse command-line ///////////////////////////////////////////////////////////////////////////////////////
    std::string applicationName = progname;
    int i_printEach = 1; // default
    int millisecondsTimeout = 5000; // default
    int millisecondsProductionDelay = 0; // default
    bool randomProductionDelay = false; // default
    std::string message = "Hello World";
    std::string brokers("localhost:9092");
    std::string topic("test");
    bool verbose = false;


    std::string value;

    if (cmdOptionExists(argv, argv + argc, "-h", value)
            || cmdOptionExists(argv, argv + argc, "--help", value))
    {
        usage(EXIT_SUCCESS);
    }

    if (cmdOptionExists(argv, argv + argc, "-k", value)
            || cmdOptionExists(argv, argv + argc, "--udp-socket-path", value))
    {
        UdpSocketPath = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-w", value)
            || cmdOptionExists(argv, argv + argc, "--workers", value))
    {
        Workers = toNumber(value);
        if (Workers <= 0)
        {
            usage(EXIT_FAILURE, "Invalid '-w|--workers' value. Must be positive.");
        }
    }

    if (cmdOptionExists(argv, argv + argc, "-e", value)
            || cmdOptionExists(argv, argv + argc, "--print-each", value))
    {
        i_printEach = atoi(value.c_str());
        if (i_printEach <= 0) usage(EXIT_FAILURE);
    }

    if (cmdOptionExists(argv, argv + argc, "-l", value)
            || cmdOptionExists(argv, argv + argc, "--log-level", value))
    {
        if (!ert::tracing::Logger::setLevel(value))
        {
            usage(EXIT_FAILURE, "Invalid log level provided !");
        }
    }

    if (cmdOptionExists(argv, argv + argc, "-v", value)
            || cmdOptionExists(argv, argv + argc, "--verbose", value))
    {
        verbose = true;
    }

    if (cmdOptionExists(argv, argv + argc, "-d", value)
            || cmdOptionExists(argv, argv + argc, "--production-delay-milliseconds", value))
    {
        millisecondsProductionDelay = toNumber(value);
        if (millisecondsProductionDelay < 0)
        {
            randomProductionDelay = true;
            millisecondsProductionDelay *= -1;
        }
    }

    if (cmdOptionExists(argv, argv + argc, "-m", value)
            || cmdOptionExists(argv, argv + argc, "--message", value))
    {
        message = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-b", value)
            || cmdOptionExists(argv, argv + argc, "--brokers", value))
    {
        brokers = value;
    }

    if (cmdOptionExists(argv, argv + argc, "-t", value)
            || cmdOptionExists(argv, argv + argc, "--topic", value))
    {
        topic = value;
    }


    /////////////////////////////////////////////////////////////////////////////////////////////////////////////
    std::cout << '\n';

    // Logger verbosity
    ert::tracing::Logger::verbose(verbose);

    if (UdpSocketPath.empty()) usage(EXIT_FAILURE);

    std::cout << "UDP socket path: " << UdpSocketPath << '\n';
    std::cout << "Workers: " << Workers << '\n';
    std::cout << "Log level: " << ert::tracing::Logger::levelAsString(ert::tracing::Logger::getLevel()) << '\n';
    std::cout << "Verbose (stdout): " << (verbose ? "true":"false") << '\n';
    std::cout << "Print each: " << i_printEach << " message(s)\n";

    // Collect variable patterns:
    collectVariablePatterns(message, MessagePatterns);

    // Detect builtin patterns:
    std::map<std::string, std::string> AllPatterns;
    auto iterateMap = [&AllPatterns](const std::map<std::string, std::string>& map) {
        static std::regex builtinPatternRegex("^udp(?:\\.\\d+)?$");
        for (const auto& it : map) {
            const std::string& value = it.second;
            if (std::regex_match(value, builtinPatternRegex)) AllPatterns[value] = std::string("@{") + value + std::string("}");
        }
    };

    iterateMap(MessagePatterns);
    bool builtinPatternsUsed = (!AllPatterns.empty());
    std::string s_builtinPatternsUsed{};
    if (builtinPatternsUsed) for (auto it: AllPatterns) {
            s_builtinPatternsUsed += " " ;
            s_builtinPatternsUsed += it.second;
        }

    std::cout << "Kafka endpoint:" << '\n';
    std::cout << "   Brokers: " << brokers << '\n';
    std::cout << "   Topic:   " << topic << '\n';
    std::cout << "   Message: " << message << '\n';
    std::cout << "   Production delay for messages (ms): ";
    if (randomProductionDelay) {
        std::cout << "random in [0," << millisecondsProductionDelay << "]" << '\n';
    }
    else {
        std::cout << millisecondsProductionDelay << '\n';
    }
    std::cout << "   Builtin patterns used:" << (s_builtinPatternsUsed.empty() ? " not detected":s_builtinPatternsUsed) << '\n';
    std::cout << '\n';

    // Creating UDP server:
    Sockfd = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (Sockfd < 0) {
        perror("Error creating UDP socket !");
        exit(EXIT_FAILURE);
    }

    struct sockaddr_un serverAddr;
    memset(&serverAddr, 0, sizeof(struct sockaddr_un));
    serverAddr.sun_family = AF_UNIX;
    strcpy(serverAddr.sun_path, UdpSocketPath.c_str());

    unlink(UdpSocketPath.c_str()); // just in case

    // Capture TERM/INT signals for graceful exit:
    signal(SIGTERM, sighndl);
    signal(SIGINT, sighndl);

    if (bind(Sockfd, (struct sockaddr*)&serverAddr, sizeof(struct sockaddr_un)) < 0) {
        perror("Error binding UDP socket !");
        close(Sockfd);
        exit(EXIT_FAILURE);
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    struct sockaddr_un clientAddr;
    socklen_t clientAddrLen;

    std::cout << "Remember:" << '\n';
    std::cout << " To print accumulated statistics: echo -n STATS | nc -u -q0 -w1 -U " << UdpSocketPath << '\n';
    std::cout << " To flush kafka producer:         echo -n FLUSH | nc -u -q0 -w1 -U " << UdpSocketPath << '\n';
    std::cout << " To stop process:                 echo -n EOF   | nc -u -q0 -w1 -U " << UdpSocketPath << '\n';

    std::cout << '\n';
    std::cout << '\n';
    std::cout << "Waiting for UDP messages..." << '\n' << '\n';
    std::cout << std::setw(COL1_WIDTH) << std::left << "<timestamp>"
              << std::setw(COL2_WIDTH) << std::left << "<sequence>"
              << std::setw(COL3_WIDTH) << std::left << "<udp datagram>"
              << std::setw(COL4_WIDTH) << std::left << "<accumulated status codes>" << '\n';
    std::cout << std::setw(COL1_WIDTH) << std::left << std::string(COL1_WIDTH-1, '_')
              << std::setw(COL2_WIDTH) << std::left << std::string(COL2_WIDTH-1, '_')
              << std::setw(COL3_WIDTH) << std::left << std::string(COL3_WIDTH-1, '_')
              << std::setw(COL4_WIDTH) << std::left << std::string(COL4_WIDTH-1, '_') << '\n';

    std::string udpData;

    // Kafka producer:
    cppkafka::Configuration config = {
        { "metadata.broker.list", brokers }
    };

    cppkafka::Producer Producer(config);

    // Worker threads:
    boost::asio::io_context io_ctx;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> io_ctx_guard = boost::asio::make_work_guard(io_ctx); // this is to avoid terminating io_ctx.run() if no more work is pending
    std::vector<std::thread> myWorkers;
    for (auto i = 0; i < Workers; ++i)
    {
        myWorkers.emplace_back([&io_ctx]() {
            io_ctx.run();
        });
    }

    // While loop to read UDP datagrams:
    unsigned int sequence{};
    while ((bytesRead = recvfrom(Sockfd, buffer, sizeof(buffer) - 1, 0, (struct sockaddr*)&clientAddr, &clientAddrLen)) > 0) {

        buffer[bytesRead] = '\0'; // Agregar terminador nulo al final del texto leído
        udpData.assign(buffer);

        // exit condition:
        if (udpData == "EOF") {
            std::cout<<  '\n' << "Exiting (EOF received) !" << '\n';
            break;
        }
        else if (udpData == "STATS") {
            std::cout << std::setw(COL1_WIDTH) << std::left << "-"
                      << std::setw(COL2_WIDTH) << std::left << "-"
                      << std::setw(COL3_WIDTH) << std::left << "STATS"
                      << std::setw(COL4_WIDTH) << std::left << statsAsString() << '\n';
        }
        else if (udpData == "FLUSH") {
            Producer.flush();
            std::cout << std::setw(COL1_WIDTH) << std::left << "-"
                      << std::setw(COL2_WIDTH) << std::left << "-"
                      << std::setw(COL3_WIDTH) << std::left << "FLUSH"
                      << std::setw(COL4_WIDTH) << std::left << statsAsString() << '\n';
        }
        else {
            sequence++;
            if (sequence % i_printEach == 0 || (sequence == 1) /* first one always shown :-)*/) {
                std::cout << std::setw(COL1_WIDTH) << std::left << ert::tracing::getLocaltime()
                          << std::setw(COL2_WIDTH) << std::left << sequence
                          << std::setw(COL3_WIDTH) << std::left << udpData
                          << std::setw(COL4_WIDTH) << std::left << statsAsString() << '\n';
            }

            // WITH DELAY FEATURE:
            int delayMs = randomProductionDelay ? (rand() % (millisecondsProductionDelay + 1)):millisecondsProductionDelay;
            auto timer = std::make_shared<boost::asio::steady_timer>(io_ctx, std::chrono::milliseconds(delayMs));
            timer->async_wait([&, udpData /* must be passed by copy because it changes its value in every iteration */, timer] (const boost::system::error_code& e) {
                auto stream = std::make_shared<Stream>(udpData);
                stream->setMessage(topic, message);
                stream->process(Producer);
            });
        }
    }
    /*
        // Workers:
        std::vector<std::thread> threads;
        for (int i = 0; i < workers; ++i) {
            threads.emplace_back(producer_thread, i + 1, brokers, topic, message, maxMessages, workerDelayMs);
        }

        // Join threads:
        for (auto &thread : threads) {
            thread.join();
        }
    */
    // Join workers:
    for (auto &w: myWorkers) w.join();

    // wrap up
    wrapup();

    exit(EXIT_SUCCESS);
}

