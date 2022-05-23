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

        if(args->get_image_count)
        {
            std::cout<<img->num_images()<<'\n';
            return 0;
        }

        if(args->get_frame_count)
        {
            std::cout<<img->num_frames()<<'\n';
            return 0;
        }

        if(!img->supports_multiple_images() && args->image_no > 0)
            throw std::runtime_error{args->help_text + "\nImage type doesn't support multiple images"};

        if(!img->supports_animation() && args->animate)
            throw std::runtime_error{args->help_text + "\nImage type doesn't support animation"};

        if(args->display)
            display_image(*img, *args);

        if(args->convert_filename)
        {
            if(args->frame_no)
                img->get_frame(*args->frame_no).convert(*args);
            else if(args->image_no)
                img->get_image(*args->image_no).convert(*args);
            else
                img->convert(*args);
        }
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
