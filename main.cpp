// Convert an image input to ascii art
#include <iostream>

#include "display.hpp"
#include "codecs/image.hpp"

int main(int argc, char * argv[])
{
    auto args = parse_args(argc, argv);
    if(!args)
        return EXIT_FAILURE;

    try
    {
        auto img = get_image_data(*args);
        if(args->display)
            display_image(*img, *args);
        img->convert(*args);
    }
    catch(Early_exit & e)
    {
        return EXIT_SUCCESS;
    }
    catch(const std::runtime_error & e)
    {
        std::cerr<<e.what()<<'\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
