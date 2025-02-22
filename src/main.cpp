import <filesystem>;
import <iostream>;
import window;

#include <boost/program_options.hpp>


int main(int argc, char *argv[])
{
    namespace po = boost::program_options;
    namespace fs = std::filesystem;

    fs::path path_in;

    po::options_description desc("Options");
    desc.add_options()
        ("help,-h", "Print help message")
        (
            "path",
            po::value<fs::path>(&path_in)->default_value(""),
            "Input file path"
        );

    po::positional_options_description pos_desc;
    pos_desc.add("path", 1);

    po::variables_map var_map;
    try
    {
        auto parsed = po::command_line_parser(argc, argv)
            .options(desc)
            .positional(pos_desc)
            .run();
        po::store(parsed, var_map);

        if (var_map.count("help"))
        {
            std::cout << desc << std::endl;
            return 0;
        }

        po::notify(var_map);
    }
    catch (const po::error& e)
    {
        std::cerr << e.what() << std::endl;
        std::cout << desc << std::endl;
        return 1;
    }

    try
    {
        hierview::Window win;
        return win.draw(path_in);
    }
    catch (std::exception& e)
    {
        std::cerr << e.what();
    }

    return 1;
}
