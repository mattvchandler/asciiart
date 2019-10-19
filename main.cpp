// Convert an image input to ascii art
#include <iostream>

#include "asciiart.hpp"
#include "font.hpp"
#include "decoders/image.hpp"

int main(int argc, char * argv[])
{
    auto args = parse_args(argc, argv);
    if(!args)
        return EXIT_FAILURE;

    try
    {
        auto font_path = get_font_path(args->font_name);
        auto values = get_char_values(font_path, args->font_size);

        auto img = get_image_data(*args);
        write_ascii(*img, values, *args);
    }
    catch(const std::runtime_error & e)
    {
        std::cerr<<e.what()<<'\n';
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
