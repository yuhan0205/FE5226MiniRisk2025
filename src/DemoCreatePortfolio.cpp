#include "TradePayment.h"
#include "PortfolioUtils.h"
#include <iostream>

using namespace minirisk;

void usage(const char* program_name)
{
    std::cerr
        << "Usage: " << program_name << " <output_file>\n"
        << "\n"
        << "Arguments:\n"
        << "  <output_file>    Path to the file where the portfolio will be saved\n"
        << "\n"
        << "Example:\n"
        << "  " << program_name << " portfolio.txt\n"
        << "  " << program_name << " data/portfolio_00.txt\n";
    std::exit(1);
}

int main(int argc, const char **argv)
{
    // Handle no arguments case
    if (argc == 1) {
        usage(argv[0]);
    }
    
    // Validate argument count
    if (argc != 2) {
        std::cerr << "Error: Invalid number of arguments.\n\n";
        usage(argv[0]);
    }

    const char *filename = argv[1];
    
    // Validate filename is not empty
    if (!filename || filename[0] == '\0') {
        std::cerr << "Error: Output filename cannot be empty.\n\n";
        usage(argv[0]);
    }

    // create a portfolio containing 2 payment trades
    portfolio_t portfolio;

    TradePayment pmt;

    pmt.init("USD", 10, Date(2020,2,1));
    portfolio.push_back(ptrade_t(new TradePayment(pmt)));

    pmt.init("EUR", 20, Date(2020,2,2));
    portfolio.push_back(ptrade_t(new TradePayment(pmt)));

    // display portfolio
    print_portfolio(portfolio);

    // save portfolio to file
    save_portfolio(filename, portfolio);

    return 0;
}

