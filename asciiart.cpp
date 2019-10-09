// Convert an image input to ascii art
#include <iostream>
#include <fstream>

#include <cstring>

#include "args.hpp"
#include "font.hpp"
#include "decoders/image.hpp"

void write_ascii(const Image & img,
                 const Char_vals & char_vals,
                 const Args & args)
{
    std::ofstream output_file;
    if(args.output_filename != "-")
        output_file.open(args.output_filename);
    std::ostream & out = (args.output_filename == "-") ? std::cout : output_file;

    if(!out)
        throw std::runtime_error{"Could not open output file: " + std::string{std::strerror(errno)}};

    const auto px_col = static_cast<float>(img.get_width()) / args.cols;
    const auto px_row = args.rows > 0 ? static_cast<float>(img.get_height()) / args.rows : px_col * 2.0f;

    std::ofstream pgm("out.pgm");
    pgm<<"P2\n"<<img.get_width()<<' '<<img.get_height()<<"\n255\n";
    for(std::size_t row = 0; row < img.get_height(); ++row)
    {
        for(std::size_t col = 0; col < img.get_width(); ++col)
        {
            pgm<<(int)img.get_pix(row, col)<<' ';
        }
        pgm<<'\n';
    }

    for(float row = 0.0f; row < img.get_height(); row += px_row)
    {
        for(float col = 0.0f; col < img.get_width(); col += px_col)
        {
            std::size_t pix_sum = 0;
            std::size_t cell_sum = 0;
            for(float y = row; y < row + px_row && y < img.get_height(); ++y)
            {
                for(float x = col; x < col + px_col && x < img.get_width(); ++x)
                {
                    pix_sum += args.invert ? 255 - img.get_pix(y, x) : img.get_pix(y, x);
                    ++cell_sum;
                }
            }
            out<<char_vals[pix_sum / cell_sum];
        }
        out<<'\n';
    }
}

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
