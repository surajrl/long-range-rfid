#include <iostream>
#include <string.h>
#include <vector>
#include <csignal>
#include <unistd.h>
#include <wiringPi.h>
#include <chrono>

#define DATA_PIN 3
#define REQUEST_PIN 4

int symbols_per_bit = 1;
int curr_bit = 0;
int msg_bit = 0;
int msg[10] = {1, 0, 1, 1, 0, 1, 1, 1, 0, 0};
auto prevt = std::chrono::high_resolution_clock::now();

void signalHandler(int sig)
{
    std::cout << "\nInterrupt signal received. Exiting ...\n";

    digitalWrite(DATA_PIN, LOW);
    digitalWrite(REQUEST_PIN, LOW);

    pinMode(DATA_PIN, INPUT);
    pinMode(REQUEST_PIN, INPUT);

    exit(sig);
}

void interruptHandler(void)
{
    auto t1 = std::chrono::high_resolution_clock::now();
    if ((t1 - prevt).count() > 180000)
    {
        std::cout << (t1 - prevt).count() << "\n";
    }
    prevt = t1;

    curr_bit += 1;
    if (curr_bit == symbols_per_bit)
    {
        digitalWrite(DATA_PIN, msg[msg_bit]);
        msg_bit++;
        if (msg_bit == 10)
        {
            msg_bit = 0;
        }
        curr_bit = 0;
    }
}

int main(int argc, char **argv)
{
    signal(SIGINT, signalHandler);

    wiringPiSetupGpio();
    pinMode(DATA_PIN, OUTPUT);
    pinMode(REQUEST_PIN, INPUT);

    if (wiringPiISR(REQUEST_PIN, INT_EDGE_RISING, &interruptHandler) < 0)
    {
        std::cout << "\nUnable to setup ISR\n";
    }

    for (;;)
    {
    }

    return 0;
}