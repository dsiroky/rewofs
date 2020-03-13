#include <iostream>

#include "rewofs/disablewarnings.hpp"
#include <boost/program_options.hpp>
#include "rewofs/enablewarnings.hpp"

#include "rewofs/log.hpp"
#include "rewofs/client/app.hpp"
#include "rewofs/server/app.hpp"

//==========================================================================

int main(int argc, char *argv[])
{
    namespace po = boost::program_options;

    rewofs::log_init();

    try
    {
        po::options_description conf_generic{"Generic options"};
        conf_generic.add_options()
            ("help", "produce help message")
            ;


        po::options_description conf_server{"Server options"};
        conf_server.add_options()
            ("serve", po::value<std::string>(), "serve a directory")
            ("listen", po::value<std::string>(), "server endpoint")
            ;

        po::options_description conf_client{"Client options"};
        conf_client.add_options()
            ("mountpoint", po::value<std::string>(), "mount point")
            ("connect", po::value<std::string>(), "remote endpoint")
            ;

        po::options_description desc{};
        desc.add(conf_generic).add(conf_server).add(conf_client);
        po::variables_map vm{};
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        if (vm.count("help") or (vm.size() == 0))
        {
            std::cout << desc << "\n";
            return 1;
        }

        if (vm.count("serve") > 0)
        {
            log_info("starting server");
            std::make_unique<rewofs::server::App>(vm)->run();
        }
        else
        {
            log_info("starting client");
            std::make_unique<rewofs::client::App>(vm)->run();
        }
        return 0;
    }
    catch (const std::exception& exc)
    {
        std::cout << exc.what() << '\n';
    }

    return 1;
}
