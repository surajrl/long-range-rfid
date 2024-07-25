#include <iostream>
#include <fstream>
#include <string.h>
#include <vector>
#include <csignal>
#include <unistd.h>
#include <wiringPi.h>
#include <termios.h>
#include <fcntl.h>
#include <bitset>
#include <chrono>

#define DATA_PIN 3
#define REQUEST_PIN 4
#define DEBUG 1

int SYMBOLS_PER_BIT = 1;
int CURR_BIT = 0;
int MSG_BIT = 0;
std::vector<int> MSG;
std::vector<int> PREAMBLE = {1, 1, 1, 1, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0, 1, 1};
auto PREVT = std::chrono::high_resolution_clock::now();

void interruptHandler(void)
{
    auto t1 = std::chrono::high_resolution_clock::now();
    if ((t1 - PREVT).count() > 180000)
    {
        std::cout << (t1 - PREVT).count() << "\n";
    }
    PREVT = t1;

    CURR_BIT += 1;
    if (CURR_BIT == SYMBOLS_PER_BIT)
    {
#if DEBUG
        std::cout << MSG[MSG_BIT] << "\n";
#else
        digitalWrite(DATA_PIN, MSG[MSG_BIT]);
#endif
        MSG_BIT++;
        if (MSG_BIT == MSG.size())
        {
            MSG_BIT = 0; // Restart message
        }
        CURR_BIT = 0;
    }
}

bool readFile(const std::string &filepath)
{
    std::ifstream file(filepath, std::ios::binary);

    if (!file.is_open())
    {
        return false;
    }

    // Read the file byte by byte
    char byte;
    while (file.read(&byte, sizeof(byte)))
    {
        // Convert the byte into bits
        std::bitset<8> bits(byte);

        // Store each bit in the vector
        for (int i = 7; i >= 0; --i)
        {
            // Access bits from MSB to LSB
            MSG.push_back(bits[i]);
        }
    }

    file.close();

    std::cout << "File size: " << MSG.size() << " bits\n";

    return true;
}

void setNonBlockingMode()
{
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag &= ~(ICANON | ECHO); // Disable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK); // Set non-blocking mode
}

void resetTerminalMode()
{
    struct termios t;
    tcgetattr(STDIN_FILENO, &t);
    t.c_lflag |= (ICANON | ECHO); // Enable canonical mode and echo
    tcsetattr(STDIN_FILENO, TCSANOW, &t);
    fcntl(STDIN_FILENO, F_SETFL, 0); // Reset to blocking mode
}

void signalHandler(int sig)
{
    std::cout << "\nInterrupt signal received. Exiting ...\n";

#if DEBUG
    resetTerminalMode(); // Restore terminal settings
#else
    digitalWrite(DATA_PIN, LOW);
    digitalWrite(REQUEST_PIN, LOW);

    pinMode(DATA_PIN, INPUT);
    pinMode(REQUEST_PIN, INPUT);
#endif

    exit(sig);
}

int main(int argc, char **argv)
{
    std::cout << "Debug: " << DEBUG << "\n";

    signal(SIGINT, signalHandler);

#if DEBUG
    std::cout << "wiringPiSetupGpio\n";
#else
    wiringPiSetupGpio();
    pinMode(DATA_PIN, OUTPUT);
    pinMode(REQUEST_PIN, INPUT);
#endif

    // Read bit stream from .lyra file
    const std::string filepath = "/home/suraj/dev/long-range-rfid/sample3_16kHz.lyra";
    if (!readFile(filepath))
    {
        std::cerr << "Failed to open file.\n";
        return -1;
    }

    // Prepend size (in bits) of actual message
    std::bitset<16> msg_size_bits(MSG.size());
    std::vector<int> vec(msg_size_bits.size());
    for (int i = msg_size_bits.size() - 1; i >= 0; --i)
    {
        vec[msg_size_bits.size() - i - 1] = msg_size_bits[i];
    }
    MSG.insert(MSG.begin(), vec.begin(), vec.end());
    // Preprend preamble
    MSG.insert(MSG.begin(), std::make_move_iterator(PREAMBLE.begin()), std::make_move_iterator(PREAMBLE.end()));

    for (const int &num : MSG)
    {
        std::cout << num; // Debugging
    }
    std::cout << "\nData to send: " << MSG.size() << " bits\n";

#if DEBUG
    setNonBlockingMode(); // Configure terminal for non-blocking input

    for (;;)
    {
        char ch;
        if (read(STDIN_FILENO, &ch, 1) > 0) // Read a character from stdin
        {
            if (ch == '\n')
            {
                interruptHandler();
            }
        }
        usleep(100000); // Sleep to prevent CPU overuse (100ms)
    }
#else
    if (wiringPiISR(REQUEST_PIN, INT_EDGE_RISING, &interruptHandler) < 0)
    {
        std::cout << "\nUnable to setup ISR\n";
    }

    for (;;)
    {
    }
#endif

    return 0;
}