#include <unistd.h>
#include <string>
#include <vector>
#include <stdint.h>
#include "argparse.h"
#include <hiredis/hiredis.h>
#include <time.h>
#include <sys/time.h>
#include <cstdlib>
#include <iostream>
#include <fstream>

#define REDIS_CHECK_ERROR(prep, ctx, cfg)                    \
    do                                                       \
    {                                                        \
        if (prep == NULL)                                    \
        {                                                    \
            printf("[%d]Error %s\n", cfg.rank, ctx->errstr); \
            return -1;                                       \
        }                                                    \
    } while (false)

struct config
{
    std::string hostip = "127.0.0.1";
    int hostport = 6379;
    int numclients = 1;
    //collectively run this many.
    int requests = 100000;
    int keysize = 1;
    int datasize = 1;
    std::vector<std::string> tests = {"get"};
    bool csv = false;
    int rank = 0;
    std::string outputFile;
    uint64_t start;
    long long totlatency;
    std::vector<uint64_t> latencies;
};

static uint64_t ustime(void)
{
    struct timeval tv;
    long long ust;
    gettimeofday(&tv, NULL);
    ust = ((long)tv.tv_sec) * 1000000;
    ust += tv.tv_usec;
    return ust;
}

int main(int argc, const char **argv)
{
    ArgumentParser parser;
    parser.addArgument("--csv", 0, true);
    parser.addArgument("--payload", 1, true);
    parser.addArgument("--keySize", 1, true);
    parser.addArgument("--host", 1, true);
    parser.addArgument("--port", 1, true);
    parser.addArgument("--requests", 1, true);
    parser.addArgument("--test", '+', true);
    parser.addArgument("--clients", 1, true);
    parser.addArgument("--rank", 1, true);
    parser.addFinalArgument("output");
    //parser.addFinalArgument("output");

    // parse the command-line arguments - throws if invalid format
    parser.parse(argc, argv);
    config cfg;
    // if we get here, the configuration is valid
    cfg.csv = parser.retrieve<bool>("csv");
    cfg.datasize = parser.retrieve<int>("payload");
    cfg.keysize = parser.retrieve<int>("keySize");
    cfg.hostip = parser.retrieve<std::string>("host");
    cfg.hostport = parser.retrieve<int>("port");
    cfg.requests = parser.retrieve<int>("requests");
    cfg.tests = parser.retrieve<std::vector<std::string>>("tests");
    cfg.numclients = parser.retrieve<int>("clients");
    cfg.rank = parser.retrieve<int>("rank");
    cfg.outputFile = parser.retrieve<std::string>("output");
    //first, rendezvous.
    redisContext *c = redisConnect(cfg.hostip.c_str(), cfg.hostport);
    if (c == NULL || c->err)
    {
        if (c)
        {
            printf("Error: %s\n", c->errstr);
            // handle error
        }
        else
        {
            printf("Can't allocate redis context\n");
        }
    }
    while (true)
    {
        auto pRep = (redisReply *)redisCommand(c, "INC R");
        REDIS_CHECK_ERROR(pRep, c, cfg);
        if (pRep->integer == cfg.numclients)
        {
            break;
        }
        sleep(1);
    }
    cfg.start = ustime();
    //now fireup bunch of benchmarks
    for (auto test : cfg.tests)
    {
        if (test == "get")
        {
            while (true)
            {
                auto start = ustime();
                auto pRep = (redisReply *)redisCommand(c, "GET R");
                auto end = ustime();
                REDIS_CHECK_ERROR(pRep, c, cfg);

                //how many do i have?
                if (cfg.latencies.size() < cfg.requests && pRep->integer == cfg.numclients)
                {
                    cfg.latencies.push_back(end - start);
                }
                else
                {
                    //i will dec myself.
                    auto pRep = (redisReply *)redisCommand(c, "DECR R");
                    REDIS_CHECK_ERROR(pRep, c, cfg);
                    break;
                }
            }
            break;
        }
        else
        {
            printf("requested test %s is not supported.\n", test.c_str());
            return -1;
        }
    }
    auto duration = ustime() - cfg.start;
    std::qsort(cfg.latencies.data(), cfg.latencies.size(), sizeof(uint64_t), [](const void *a, const void *b) {
        uint64_t arg1 = *static_cast<const uint64_t *>(a);
        uint64_t arg2 = *static_cast<const uint64_t *>(b);
        if (arg1 < arg2)
            return -1;
        if (arg1 > arg2)
            return 1;
        return 0;
        //  return (arg1 > arg2) - (arg1 < arg2); // possible shortcut
        //  return arg1 - arg2; // erroneous shortcut (fails if INT_MIN is present)
    });
    //process and writing to file.
    auto reqpersec = 1000000.0 * cfg.latencies.size() / duration;
    std::ofstream output;
    output.open(cfg.outputFile);
    printf("%f\n", reqpersec);
    for (int percent = 0; percent <= 100; percent += 10)
    {
        int idx = (int)(percent * cfg.latencies.size() / 100.0);
        if (idx >= cfg.latencies.size())
            idx = cfg.latencies.size() - 1;
        float lat = cfg.latencies[idx];
        output << lat <<"\n";
        //printf("%f\n", lat);
    }
    //output << "Writing this to a file.\n";
    output.close();
    redisFree(c);
    return 0;
}
